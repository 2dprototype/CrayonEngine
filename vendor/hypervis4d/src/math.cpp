#include "hv4d/math.hpp"

namespace hv4d {

Vec4 Rotor4::rotate(const Vec4& v) const {
    // p = R v ~R, in two steps.   Q = R v
    const Vec4 a1 = b.dot_v(v);
    const Trivec4 a3 = b.wedge_v(v);
    const Trivec4 b3 = q.mul_v(v);
    const Vec4 q1 = v * s + a1;
    const Trivec4 q3 = a3 + b3;

    // p = Q ~R
    const Bivec4 b_rev = b.reverse();
    return q1 * s + q1.left_contract_bv(b_rev) + q3.right_contract_bv(b_rev) + q3.mul_qv(q);
}

Rotor4 Rotor4::mul_bv(const Bivec4& c) const {
    float a0;
    Bivec4 a2;
    Quadvec4 a4;
    b.mul_bv(c, a0, a2, a4);
    return Rotor4(a0, s * c + a2 + q.mul_bv(c), a4);
}

Rotor4 Rotor4::operator*(const Rotor4& r1) const {
    const Rotor4& r0 = *this;
    float a0;
    Bivec4 a2;
    Quadvec4 a4;
    r0.b.mul_bv(r1.b, a0, a2, a4);
    return Rotor4(r0.s * r1.s + a0 + r0.q.xyzw * r1.q.xyzw,
                  r0.s * r1.b + r1.s * r0.b + a2 + r0.q.mul_bv(r1.b) + r1.q.mul_bv(r0.b),
                  r0.s * r1.q + r1.s * r0.q + a4);
}

void Rotor4::update(const Bivec4& delta) {
    *this = *this * (-0.5f * delta).exp();
    normalize();
}

void Rotor4::decompose(Rotor4& r_plus, Rotor4& r_minus) const {
    const Quadvec4 pos_half{0.5f};
    const Quadvec4 neg_half{-0.5f};

    r_plus = Rotor4(0.5f + 0.5f * s + 0.5f * q.xyzw,
                    0.5f * b + pos_half.mul_bv(b),
                    0.5f * q + s * pos_half + neg_half);

    r_minus = Rotor4(0.5f + 0.5f * s - 0.5f * q.xyzw,
                     0.5f * b + neg_half.mul_bv(b),
                     0.5f * q + s * neg_half + pos_half);
}

void Rotor4::normalize() {
    // Decompose into two isoclinic rotations (each equivalent to a quaternion),
    // normalise each quaternion, and recover the original rotor.
    Rotor4 r_plus, r_minus;
    decompose(r_plus, r_minus);

    // get rid of the 1/2 (1 +- xyzw) components
    r_plus.s -= 0.5f;
    r_minus.s -= 0.5f;

    const float plus_mag = 2.0f * std::sqrt(r_plus.s * r_plus.s + r_plus.b.xy * r_plus.b.xy +
                                            r_plus.b.xz * r_plus.b.xz + r_plus.b.xw * r_plus.b.xw);
    const float minus_mag = 2.0f * std::sqrt(r_minus.s * r_minus.s + r_minus.b.xy * r_minus.b.xy +
                                             r_minus.b.xz * r_minus.b.xz + r_minus.b.xw * r_minus.b.xw);

    if (plus_mag > 0.0f) {
        const float inv = 1.0f / plus_mag;
        r_plus.s *= inv;
        r_plus.b = inv * r_plus.b;
        r_plus.q.xyzw = r_plus.s;
        // readd 1/2 (1 - xyzw)
        r_plus.s += 0.5f;
        r_plus.q.xyzw -= 0.5f;
    } else {
        r_plus = Rotor4::identity();
    }

    if (minus_mag > 0.0f) {
        const float inv = 1.0f / minus_mag;
        r_minus.s *= inv;
        r_minus.b = inv * r_minus.b;
        r_minus.q.xyzw = -r_minus.s;
        // readd 1/2 (1 + xyzw)
        r_minus.s += 0.5f;
        r_minus.q.xyzw += 0.5f;
    } else {
        r_minus = Rotor4::identity();
    }

    *this = r_plus * r_minus;
}

float Rotor4::mag() const {
    return std::sqrt(s * s + b.xy * b.xy + b.xz * b.xz + b.xw * b.xw + b.yz * b.yz + b.yw * b.yw +
                     b.zw * b.zw + q.xyzw * q.xyzw);
}

float Rotor4::weird_term() const {
    return -2.0f * b.xw * b.yz - 2.0f * b.xy * b.zw + 2.0f * b.xz * b.yw + 2.0f * q.xyzw * s;
}

Mat4 Rotor4::to_matrix() const {
    Mat4 m;
    m.col[0] = rotate(Vec4::unit_x());
    m.col[1] = rotate(Vec4::unit_y());
    m.col[2] = rotate(Vec4::unit_z());
    m.col[3] = rotate(Vec4::unit_w());
    return m;
}

float Mat4::determinant() const {
    // triple_cross_product(ex,ey,ez) = -ew in this algebra's convention, so negate
    // to make the identity matrix have determinant +1.
    return -triple_cross_product(col[0], col[1], col[2]).dot(col[3]);
}

void orthonormal_basis(const Vec4& a, Vec4 out[3]) {
    // If a is normalised, since 1/4 = 0.25 at least one component of a must be
    // >= sqrt(0.25) in magnitude.
    Vec4 b, c;
    if (std::fabs(a.x) >= 0.5f || std::fabs(a.y) >= 0.5f) {
        b = Vec4(a.y, -a.x, 0.0f, 0.0f).normalized();
        const Vec3 c3 = normalize(cross(Vec3(a.x, a.y, a.z), Vec3(b.x, b.y, b.z)));
        c = Vec4(c3.x, c3.y, c3.z, 0.0f);
    } else {
        b = Vec4(0.0f, 0.0f, a.w, -a.z).normalized();
        const Vec3 c3 = normalize(cross(Vec3(a.y, a.z, a.w), Vec3(b.y, b.z, b.w)));
        c = Vec4(0.0f, c3.x, c3.y, c3.z);
    }
    const Vec4 d = triple_cross_product(a, b, c).normalized();
    out[0] = b;
    out[1] = c;
    out[2] = d;
}

}  // namespace hv4d
