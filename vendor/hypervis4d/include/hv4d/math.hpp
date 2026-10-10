// hv4d/math.hpp
// 4D linear algebra + geometric algebra (bivectors / rotors) used by the
// Hypervis 4D physics engine.  Re-creation of hypervis' `alg` module.
//
// Conventions
//   * float precision everywhere (like the original).
//   * Vec4 = (x, y, z, w).  W is the "fourth spatial dimension".  Y is up.
//   * Rotations are Rotor4 (scalar + bivector + pseudoscalar).  Quaternions do
//     not generalise to 4D.  A Bivec4 is a rotation *plane* with a magnitude:
//     xy, xz, xw, yz, yw, zw.
#pragma once

#include <cmath>
#include <cstdint>

namespace hv4d {

constexpr float EPSILON = 1e-6f;
constexpr float PI      = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// Vec3 (only used for slice / projection output)
// ---------------------------------------------------------------------------
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};
constexpr inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
constexpr inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 0.0f ? v / l : Vec3{};
}

struct Bivec4;
struct Trivec4;
struct Quadvec4;
struct Rotor4;

// ---------------------------------------------------------------------------
// Vec4
// ---------------------------------------------------------------------------
struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    static constexpr Vec4 zero()   { return {0, 0, 0, 0}; }
    static constexpr Vec4 unit_x() { return {1, 0, 0, 0}; }
    static constexpr Vec4 unit_y() { return {0, 1, 0, 0}; }
    static constexpr Vec4 unit_z() { return {0, 0, 1, 0}; }
    static constexpr Vec4 unit_w() { return {0, 0, 0, 1}; }

    constexpr Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    constexpr Vec4 operator-() const { return {-x, -y, -z, -w}; }
    constexpr Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr Vec4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    Vec4& operator+=(const Vec4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    Vec4& operator-=(const Vec4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    Vec4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vec4& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }

    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }

    constexpr float dot(const Vec4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    constexpr float length2() const { return dot(*this); }
    float length() const { return std::sqrt(length2()); }
    Vec4 normalized() const {
        float l = length();
        return {x / l, y / l, z / l, w / l};  // NaN on zero (like cgmath); use is_finite()
    }
    Vec4 normalized_or_zero() const {
        float l = length();
        return l > 0.0f ? Vec4{x / l, y / l, z / l, w / l} : Vec4{};
    }
    bool is_finite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }
    constexpr Vec3 xyz() const { return {x, y, z}; }

    // Geometric-algebra products (see bivec4 etc. below)
    inline Vec4 left_contract_bv(const Bivec4& b) const;
    inline Trivec4 wedge_bv(const Bivec4& b) const;
    inline Bivec4 wedge_v(const Vec4& v) const;
};
constexpr inline Vec4 operator*(float s, const Vec4& v) { return v * s; }
constexpr inline float dot(const Vec4& a, const Vec4& b) { return a.dot(b); }

// ---------------------------------------------------------------------------
// Quadvec4 / Trivec4 / Bivec4
// ---------------------------------------------------------------------------
struct Quadvec4 {
    float xyzw = 0.0f;
    constexpr Quadvec4() = default;
    constexpr explicit Quadvec4(float v) : xyzw(v) {}
    static constexpr Quadvec4 zero() { return Quadvec4{0.0f}; }
    static constexpr Quadvec4 one()  { return Quadvec4{1.0f}; }

    inline Trivec4 mul_v(const Vec4& v) const;
    inline Bivec4 mul_bv(const Bivec4& b) const;

    constexpr Quadvec4 operator+(const Quadvec4& o) const { return Quadvec4{xyzw + o.xyzw}; }
    constexpr Quadvec4 operator*(float s) const { return Quadvec4{xyzw * s}; }
};
constexpr inline Quadvec4 operator*(float s, const Quadvec4& q) { return q * s; }

struct Trivec4 {
    float xyz = 0.0f, xyw = 0.0f, xzw = 0.0f, yzw = 0.0f;
    constexpr Trivec4() = default;
    constexpr Trivec4(float a, float b, float c, float d) : xyz(a), xyw(b), xzw(c), yzw(d) {}

    inline Vec4 right_contract_bv(const Bivec4& b) const;
    inline Vec4 mul_qv(const Quadvec4& q) const;

    constexpr Trivec4 operator+(const Trivec4& t) const {
        return {xyz + t.xyz, xyw + t.xyw, xzw + t.xzw, yzw + t.yzw};
    }
};

struct Bivec4 {
    float xy = 0.0f, xz = 0.0f, xw = 0.0f, yz = 0.0f, yw = 0.0f, zw = 0.0f;
    constexpr Bivec4() = default;
    constexpr Bivec4(float xy_, float xz_, float xw_, float yz_, float yw_, float zw_)
        : xy(xy_), xz(xz_), xw(xw_), yz(yz_), yw(yw_), zw(zw_) {}
    static constexpr Bivec4 zero() { return {}; }

    constexpr Bivec4 operator+(const Bivec4& c) const {
        return {xy + c.xy, xz + c.xz, xw + c.xw, yz + c.yz, yw + c.yw, zw + c.zw};
    }
    constexpr Bivec4 operator-(const Bivec4& c) const {
        return {xy - c.xy, xz - c.xz, xw - c.xw, yz - c.yz, yw - c.yw, zw - c.zw};
    }
    constexpr Bivec4 operator-() const { return {-xy, -xz, -xw, -yz, -yw, -zw}; }
    constexpr Bivec4 operator*(float s) const { return {xy * s, xz * s, xw * s, yz * s, yw * s, zw * s}; }
    Bivec4& operator+=(const Bivec4& c) { *this = *this + c; return *this; }

    constexpr Bivec4 reverse() const { return {-xy, -xz, -xw, -yz, -yw, -zw}; }
    float length2() const { return xy * xy + xz * xz + xw * xw + yz * yz + yw * yw + zw * zw; }
    float length() const { return std::sqrt(length2()); }
    constexpr float dot(const Bivec4& c) const {
        return xy * c.xy + xz * c.xz + xw * c.xw + yz * c.yz + yw * c.yw + zw * c.zw;
    }

    // B . v
    inline Vec4 dot_v(const Vec4& v) const {
        return {xw * v.w + xy * v.y + xz * v.z,
                -xy * v.x + yw * v.w + yz * v.z,
                -xz * v.x - yz * v.y + zw * v.w,
                -xw * v.x - yw * v.y - zw * v.z};
    }
    // B ^ v
    inline Trivec4 wedge_v(const Vec4& v) const {
        return {xy * v.z - xz * v.y + yz * v.x,
                -xw * v.y + xy * v.w + yw * v.x,
                -xw * v.z + xz * v.w + zw * v.x,
                -yw * v.z + yz * v.w + zw * v.y};
    }

    // Full geometric product B * C = (scalar, bivector, quadvector)
    inline void mul_bv(const Bivec4& c, float& s, Bivec4& d, Quadvec4& q) const {
        const Bivec4& b = *this;
        s = -b.xy * c.xy - b.xz * c.xz - b.xw * c.xw - b.yz * c.yz - b.yw * c.yw - b.zw * c.zw;
        d.xy = -b.xw * c.yw - b.xz * c.yz + b.yw * c.xw + b.yz * c.xz;
        d.xz = -b.xw * c.zw + b.xy * c.yz - b.yz * c.xy + b.zw * c.xw;
        d.xw =  b.xy * c.yw + b.xz * c.zw - b.yw * c.xy - b.zw * c.xz;
        d.yz = -b.xy * c.xz + b.xz * c.xy - b.yw * c.zw + b.zw * c.yw;
        d.yw =  b.xw * c.xy - b.xy * c.xw + b.yz * c.zw - b.zw * c.yz;
        d.zw =  b.xw * c.xz - b.xz * c.xw + b.yw * c.yz - b.yz * c.yw;
        q.xyzw = b.xw * c.yz + b.xy * c.zw - b.xz * c.yw - b.yw * c.xz + b.yz * c.xw + b.zw * c.xy;
    }

    // Split into self-dual / anti-self-dual (isoclinic) halves.
    inline void decompose(Bivec4& b_plus, Bivec4& b_minus) const;
    // Bivector exponential -> rotor.
    inline Rotor4 exp() const;
};
constexpr inline Bivec4 operator*(float s, const Bivec4& b) { return b * s; }

// --- Vec4 / Trivec4 / Quadvec4 product bodies --------------------------------
inline Vec4 Vec4::left_contract_bv(const Bivec4& b) const {
    return {-y * b.xy - z * b.xz - w * b.xw,
            x * b.xy - z * b.yz - w * b.yw,
            x * b.xz + y * b.yz - w * b.zw,
            x * b.xw + y * b.yw + z * b.zw};
}
// NOTE: upstream hypervis has a typo in the `yzw` term (`v.x * b.zw`).  The
// correct wedge is used here.  (Unused by the physics, kept for completeness.)
inline Trivec4 Vec4::wedge_bv(const Bivec4& b) const {
    return {x * b.yz - y * b.xz + z * b.xy,
            x * b.yw - y * b.xw + w * b.xy,
            x * b.zw - z * b.xw + w * b.xz,
            y * b.zw - z * b.yw + w * b.yz};
}
inline Bivec4 Vec4::wedge_v(const Vec4& v) const {
    return {x * v.y - y * v.x,
            x * v.z - z * v.x,
            -w * v.x + x * v.w,
            y * v.z - z * v.y,
            -w * v.y + y * v.w,
            -w * v.z + z * v.w};
}
inline Vec4 Trivec4::right_contract_bv(const Bivec4& b) const {
    return {-b.yw * xyw - b.yz * xyz - b.zw * xzw,
            b.xw * xyw + b.xz * xyz - b.zw * yzw,
            b.xw * xzw - b.xy * xyz + b.yw * yzw,
            -b.xy * xyw - b.xz * xzw - b.yz * yzw};
}
inline Vec4 Trivec4::mul_qv(const Quadvec4& q) const {
    return {q.xyzw * yzw, -q.xyzw * xzw, q.xyzw * xyw, -q.xyzw * xyz};
}
inline Trivec4 Quadvec4::mul_v(const Vec4& v) const {
    return {xyzw * v.w, -xyzw * v.z, xyzw * v.y, -xyzw * v.x};
}
inline Bivec4 Quadvec4::mul_bv(const Bivec4& b) const {
    return {-b.zw * xyzw, b.yw * xyzw, -b.yz * xyzw, -b.xw * xyzw, b.xz * xyzw, -b.xy * xyzw};
}

inline void Bivec4::decompose(Bivec4& b_plus, Bivec4& b_minus) const {
    const Quadvec4 pos_half{0.5f};
    const Quadvec4 neg_half{-0.5f};
    b_plus  = 0.5f * *this + pos_half.mul_bv(*this);
    b_minus = 0.5f * *this + neg_half.mul_bv(*this);
}

// A vector perpendicular to all three arguments (4D "cross product").
inline Vec4 triple_cross_product(const Vec4& u, const Vec4& v, const Vec4& w) {
    return u.wedge_v(v).wedge_v(w).mul_qv(Quadvec4::one());
}

// ---------------------------------------------------------------------------
// Mat4 (column vectors; col[i] is the image of basis vector i)
// ---------------------------------------------------------------------------
struct Mat4 {
    Vec4 col[4] = {Vec4::unit_x(), Vec4::unit_y(), Vec4::unit_z(), Vec4::unit_w()};
    Vec4 operator*(const Vec4& v) const { return col[0] * v.x + col[1] * v.y + col[2] * v.z + col[3] * v.w; }
    // Row-major copy of the 16 floats (m[row*4+col]) for convenience.
    void to_row_major(float out[16]) const {
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) out[r * 4 + c] = col[c][r];
    }
    float determinant() const;
};

// ---------------------------------------------------------------------------
// Rotor4:  R = s + B + q*xyzw   (element of the even subalgebra of G(4,0))
// ---------------------------------------------------------------------------
struct Rotor4 {
    float s = 1.0f;
    Bivec4 b{};
    Quadvec4 q{};

    constexpr Rotor4() = default;
    constexpr Rotor4(float s_, const Bivec4& b_, const Quadvec4& q_) : s(s_), b(b_), q(q_) {}
    static constexpr Rotor4 identity() { return {}; }

    Rotor4 reverse() const { return {s, b.reverse(), q}; }

    // p = R v ~R
    Vec4 rotate(const Vec4& v) const;

    // R * c  where c is a bivector
    Rotor4 mul_bv(const Bivec4& c) const;

    Rotor4 operator+(const Rotor4& o) const { return {s + o.s, b + o.b, q + o.q}; }
    Rotor4 operator*(const Rotor4& r1) const;

    // Integrate an angular displacement (bivector * dt):  R <- R * exp(-delta/2)
    void update(const Bivec4& delta);

    // Re-orthonormalise via the two isoclinic (quaternion) halves.
    void normalize();
    float mag() const;
    float weird_term() const;
    // Cayley factorisation into two pure isoclinic rotations.
    void decompose(Rotor4& r_plus, Rotor4& r_minus) const;

    Mat4 to_matrix() const;

    // Rotor that rotates by the given plane-angles (radians) with the same
    // handedness as `Bivec4 angular velocity` integrated by `update`.
    static Rotor4 from_angles(const Bivec4& angles) { Rotor4 r; r.update(angles); return r; }
};

inline Rotor4 Bivec4::exp() const {
    Bivec4 b_plus, b_minus;
    decompose(b_plus, b_minus);

    const float theta_plus  = 2.0f * std::sqrt(b_plus.xy * b_plus.xy + b_plus.xz * b_plus.xz + b_plus.xw * b_plus.xw);
    const float theta_minus = 2.0f * std::sqrt(b_minus.xy * b_minus.xy + b_minus.xz * b_minus.xz + b_minus.xw * b_minus.xw);

    const float inv_plus  = theta_plus  > 0.0f ? 1.0f / theta_plus  : 0.0f;
    const float inv_minus = theta_minus > 0.0f ? 1.0f / theta_minus : 0.0f;

    const Bivec4 unit_plus  = inv_plus * b_plus;
    const Bivec4 unit_minus = inv_minus * b_minus;

    const float cp = std::cos(theta_plus), cm = std::cos(theta_minus);
    return Rotor4(0.5f * cp + 0.5f * cm,
                  std::sin(theta_plus) * unit_plus + std::sin(theta_minus) * unit_minus,
                  Quadvec4(0.5f * cp - 0.5f * cm));
}

// Misc helpers --------------------------------------------------------------

// Orthonormal basis of the 3D space perpendicular to the unit vector `a`.
// (Extension of Box2D's "computing a basis" to 4 dimensions.)
void orthonormal_basis(const Vec4& a, Vec4 out[3]);

// Reflect v in the mirror with unit normal n.
constexpr inline Vec4 reflect(const Vec4& v, const Vec4& mirror_normal) {
    return v - mirror_normal * (2.0f * mirror_normal.dot(v));
}

}  // namespace hv4d
