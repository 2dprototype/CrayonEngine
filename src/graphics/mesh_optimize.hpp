#pragma once
// Mesh post-processing shared by every loader (OBJ, glTF, procedural) and by
// both renderer backends. Header-only and GL-free so it can be unit-tested
// headless.
//
//  * weld():  merges bit-identical vertices and rewrites the index list. The OBJ
//             loaders emit 3 fresh vertices per triangle; welding typically
//             shrinks that 3-6x, which cuts VBO size, vertex-shader work and
//             (for the CPU renderer) transform work by the same factor.
//  * reorder_for_cache(): reorders triangles for post-transform cache locality
//             (Tom Forsyth's linear-speed algorithm) and then renumbers vertices
//             in first-use order for fetch locality.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace crayon::mesh_opt {

// ---- weld ------------------------------------------------------------------
namespace detail {
struct ByteKey {
    const unsigned char* p;
    size_t n;
    bool operator==(const ByteKey& o) const { return n == o.n && std::memcmp(p, o.p, n) == 0; }
};
struct ByteKeyHash {
    size_t operator()(const ByteKey& k) const {
        // FNV-1a over 4-byte words (vertex structs are all multiples of 4 bytes).
        uint64_t h = 1469598103934665603ull;
        const uint32_t* w = reinterpret_cast<const uint32_t*>(k.p);
        for (size_t i = 0; i < k.n / 4; ++i) { h ^= w[i]; h *= 1099511628211ull; }
        return static_cast<size_t>(h ^ (h >> 32));
    }
};
} // namespace detail

// Merge bit-identical vertices. `indices` must index `vertices` (use an identity
// list for non-indexed data). V must be trivially copyable with sizeof % 4 == 0.
template <typename V>
void weld(std::vector<V>& vertices, std::vector<uint32_t>& indices) {
    static_assert(sizeof(V) % 4 == 0, "vertex size must be a multiple of 4 bytes");
    if (vertices.empty()) return;
    for (uint32_t i : indices) if (i >= vertices.size()) return; // malformed input: leave untouched

    std::unordered_map<detail::ByteKey, uint32_t, detail::ByteKeyHash> seen;
    seen.reserve(vertices.size());
    std::vector<uint32_t> remap(vertices.size());
    std::vector<V> out;
    out.reserve(vertices.size() / 2 + 16);

    for (size_t i = 0; i < vertices.size(); ++i) {
        detail::ByteKey key{reinterpret_cast<const unsigned char*>(&vertices[i]), sizeof(V)};
        auto it = seen.find(key);
        if (it != seen.end()) {
            remap[i] = it->second;
        } else {
            uint32_t ni = static_cast<uint32_t>(out.size());
            out.push_back(vertices[i]);
            remap[i] = ni;
            // key points into the source array, which is not modified until we're done
            seen.emplace(key, ni);
        }
    }
    for (auto& idx : indices) idx = remap[idx];
    vertices = std::move(out);
}

// ---- Forsyth vertex-cache optimizer ---------------------------------------
namespace detail {
constexpr int kCacheSize = 32;
inline float vertex_score(int cache_pos, int active_tris) {
    if (active_tris == 0) return -1.0f;
    float score = 0.0f;
    if (cache_pos >= 0) {
        if (cache_pos < 3) score = 0.75f;
        else score = std::pow(1.0f - float(cache_pos - 3) / float(kCacheSize - 3), 1.5f);
    }
    score += 2.0f * std::pow(float(active_tris), -0.5f);
    return score;
}
} // namespace detail

// Reorders triangles for the GPU post-transform cache, then renumbers vertices in
// first-use order. Safe on any triangle list; a no-op for tiny meshes.
template <typename V>
void reorder_for_cache(std::vector<V>& vertices, std::vector<uint32_t>& indices) {
    const size_t tri_count = indices.size() / 3;
    const size_t vert_count = vertices.size();
    if (tri_count < 2 || vert_count < 3) return;
    for (uint32_t i : indices) if (i >= vert_count) return;      // malformed input: leave untouched

    std::vector<int> active(vert_count, 0);
    std::vector<int> cache_pos(vert_count, -1);
    std::vector<float> vscore(vert_count);
    for (size_t i = 0; i < indices.size(); ++i) active[indices[i]]++;

    std::vector<uint32_t> tri_start(vert_count + 1, 0);
    for (size_t v = 0; v < vert_count; ++v) tri_start[v + 1] = tri_start[v] + active[v];
    std::vector<uint32_t> tri_list(indices.size());
    {
        std::vector<uint32_t> fill(tri_start.begin(), tri_start.end() - 1);
        for (size_t t = 0; t < tri_count; ++t)
            for (int k = 0; k < 3; ++k) tri_list[fill[indices[t * 3 + k]]++] = static_cast<uint32_t>(t);
    }

    for (size_t v = 0; v < vert_count; ++v) vscore[v] = detail::vertex_score(-1, active[v]);
    std::vector<float> tscore(tri_count);
    std::vector<char> done(tri_count, 0);
    for (size_t t = 0; t < tri_count; ++t)
        tscore[t] = vscore[indices[t * 3]] + vscore[indices[t * 3 + 1]] + vscore[indices[t * 3 + 2]];

    std::vector<uint32_t> out;
    out.reserve(indices.size());
    std::vector<uint32_t> cache;           // front = most recent
    cache.reserve(detail::kCacheSize + 3);
    size_t scan = 0;
    int best = -1;
    float best_score = -1.0f;
    for (size_t t = 0; t < tri_count; ++t) if (tscore[t] > best_score) { best_score = tscore[t]; best = int(t); }

    for (size_t emitted = 0; emitted < tri_count; ++emitted) {
        if (best < 0 || done[best]) {
            best = -1;
            best_score = -1.0f;
            while (scan < tri_count && done[scan]) ++scan;
            for (size_t t = scan; t < tri_count; ++t)
                if (!done[t] && tscore[t] > best_score) { best_score = tscore[t]; best = int(t); }
            if (best < 0) break;
        }
        done[best] = 1;
        uint32_t tv[3] = {indices[best * 3], indices[best * 3 + 1], indices[best * 3 + 2]};
        for (int k = 0; k < 3; ++k) { out.push_back(tv[k]); active[tv[k]]--; }

        // update LRU cache
        std::vector<uint32_t> new_cache;
        new_cache.reserve(detail::kCacheSize + 3);
        for (int k = 2; k >= 0; --k) new_cache.push_back(tv[k]);
        for (uint32_t c : cache)
            if (c != tv[0] && c != tv[1] && c != tv[2]) new_cache.push_back(c);
        for (uint32_t c : cache) cache_pos[c] = -1;
        for (int k = 0; k < 3; ++k) cache_pos[tv[k]] = -1;
        if (new_cache.size() > size_t(detail::kCacheSize)) new_cache.resize(detail::kCacheSize);
        for (size_t i = 0; i < new_cache.size(); ++i) cache_pos[new_cache[i]] = int(i);
        cache.swap(new_cache);

        best = -1;
        best_score = -1.0f;
        for (uint32_t c : cache) {
            vscore[c] = detail::vertex_score(cache_pos[c], active[c]);
        }
        for (uint32_t v : tv) vscore[v] = detail::vertex_score(cache_pos[v], active[v]);
        for (uint32_t c : cache) {
            for (uint32_t i = tri_start[c]; i < tri_start[c + 1]; ++i) {
                uint32_t t = tri_list[i];
                if (done[t]) continue;
                tscore[t] = vscore[indices[t * 3]] + vscore[indices[t * 3 + 1]] + vscore[indices[t * 3 + 2]];
                if (tscore[t] > best_score) { best_score = tscore[t]; best = int(t); }
            }
        }
    }

    // Renumber vertices in first-use order (fetch locality) and drop unused ones.
    std::vector<uint32_t> remap(vert_count, 0xFFFFFFFFu);
    std::vector<V> new_verts;
    new_verts.reserve(vert_count);
    for (auto& idx : out) {
        if (remap[idx] == 0xFFFFFFFFu) {
            remap[idx] = static_cast<uint32_t>(new_verts.size());
            new_verts.push_back(vertices[idx]);
        }
        idx = remap[idx];
    }
    vertices = std::move(new_verts);
    indices = std::move(out);
}

// Convenience: weld then cache-optimize.
template <typename V>
void optimize(std::vector<V>& vertices, std::vector<uint32_t>& indices) {
    weld(vertices, indices);
    reorder_for_cache(vertices, indices);
}

// Average post-transform-cache miss ratio (lower is better, ~0.5 ideal, 3.0 worst)
// using a FIFO cache; handy for tests and benchmarks.
inline float acmr(const std::vector<uint32_t>& indices, int cache_size = 16) {
    std::vector<uint32_t> fifo;
    size_t misses = 0;
    for (uint32_t i : indices) {
        if (std::find(fifo.begin(), fifo.end(), i) == fifo.end()) {
            ++misses;
            fifo.push_back(i);
            if (int(fifo.size()) > cache_size) fifo.erase(fifo.begin());
        }
    }
    return indices.empty() ? 0.0f : float(misses) / float(indices.size() / 3);
}

} // namespace crayon::mesh_opt
