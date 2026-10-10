#include "hv4d/todd_coxeter.hpp"

#include <stdexcept>

namespace hv4d {
namespace todd_coxeter {

namespace {

constexpr int NONE = -1;

struct CosetTable {
    int num_gens;
    std::vector<std::vector<int>> table;
    std::vector<int> live_map;

    explicit CosetTable(int gens) : num_gens(gens) {
        table.emplace_back(num_gens, NONE);
        live_map.push_back(0);
    }

    void define(int coset, int g) {
        const int fresh = static_cast<int>(table.size());
        table.emplace_back(num_gens, NONE);
        table[coset][g] = fresh;
        table[fresh][g] = coset;
        live_map.push_back(fresh);
    }

    int rep(int coset) {
        int m = coset;
        while (m != live_map[m]) m = live_map[m];
        int j = coset;
        while (j != live_map[j]) {
            const int next = live_map[j];
            live_map[j] = m;
            j = next;
        }
        return m;
    }

    void merge(std::vector<int>& queue, int coset1, int coset2) {
        const int s = rep(coset1), t = rep(coset2);
        if (s != t) {
            const int lo = s < t ? s : t;
            const int hi = s < t ? t : s;
            live_map[hi] = lo;
            queue.push_back(hi);
        }
    }

    void coincidence(int coset1, int coset2) {
        std::vector<int> queue;
        merge(queue, coset1, coset2);
        size_t head = 0;
        while (head < queue.size()) {
            const int e = queue[head++];
            for (int g = 0; g < num_gens; ++g) {
                const int f = table[e][g];
                if (f == NONE) continue;
                table[f][g] = NONE;

                const int e_ = rep(e), f_ = rep(f);
                if (table[e_][g] != NONE) {
                    merge(queue, f_, table[e_][g]);
                } else if (table[f_][g] != NONE) {
                    merge(queue, e_, table[f_][g]);
                } else {
                    table[e_][g] = f_;
                    table[f_][g] = e_;
                }
            }
        }
    }

    void scan_and_fill(int coset, const std::vector<int>& word) {
        int f = coset;
        int b = coset;
        long i = 0;
        long j = static_cast<long>(word.size()) - 1;

        for (;;) {
            // scan forwards as far as possible
            while (i <= j && table[f][word[i]] != NONE) {
                f = table[f][word[i]];
                ++i;
            }

            if (i > j) {
                if (f != b) coincidence(f, b);  // found a coincidence
                return;
            }

            // scan backwards as far as possible
            while (j >= i && table[b][word[j]] != NONE) {
                b = table[b][word[j]];
                --j;
            }

            if (j < i) {
                coincidence(f, b);
                return;
            } else if (i == j) {
                // deduction
                table[f][word[i]] = b;
                table[b][word[i]] = f;
            } else {
                // define a new coset, continue scanning
                define(f, word[i]);
            }
        }
    }
};

}  // namespace

Table coset_table(int num_gens, const std::vector<Relation>& relations, const std::vector<int>& sub_gens) {
    CosetTable t(num_gens);

    // fill in initial information for the first coset
    for (int g : sub_gens) t.scan_and_fill(0, std::vector<int>{g});

    for (size_t i = 0; i < t.table.size(); ++i) {
        if (t.live_map[i] == static_cast<int>(i)) {
            for (const auto& rel : relations) t.scan_and_fill(static_cast<int>(i), rel);
            for (int g = 0; g < num_gens; ++g) {
                if (t.table[i][g] == NONE) t.define(static_cast<int>(i), g);
            }
        }
    }

    // compress the resulting table
    std::vector<int> forward(t.table.size(), NONE);
    std::vector<int> backward;
    for (size_t coset = 0; coset < t.table.size(); ++coset) {
        if (t.live_map[coset] == static_cast<int>(coset)) {
            forward[coset] = static_cast<int>(backward.size());
            backward.push_back(static_cast<int>(coset));
        }
    }

    Table compressed(backward.size());
    for (size_t i = 0; i < backward.size(); ++i) {
        compressed[i].resize(num_gens);
        for (int g = 0; g < num_gens; ++g) {
            const int x = t.table[backward[i]][g];
            if (x == NONE || forward[x] == NONE) {
                throw std::runtime_error("hv4d::todd_coxeter: inconsistent coset table");
            }
            compressed[i][g] = forward[x];
        }
    }
    return compressed;
}

}  // namespace todd_coxeter
}  // namespace hv4d
