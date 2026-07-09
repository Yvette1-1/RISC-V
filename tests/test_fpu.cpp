#include "fpu.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

std::uint32_t bits(float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t out = 0;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

float as_float(std::uint32_t value) {
    float out = 0.0f;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

bool is_nan_bits(std::uint32_t value) {
    const auto exp = (value >> 23) & 0xffu;
    const auto frac = value & 0x7fffffu;
    return exp == 0xffu && frac != 0u;
}

}  // namespace

int main() {
    // Basic arithmetic
    assert(riscv::f32_add(bits(1.0f), bits(2.0f)) == bits(3.0f));
    assert(riscv::f32_sub(bits(5.5f), bits(2.0f)) == bits(3.5f));
    assert(riscv::f32_mul(bits(3.0f), bits(4.0f)) == bits(12.0f));
    assert(riscv::f32_div(bits(9.0f), bits(3.0f)) == bits(3.0f));
    assert(riscv::f32_sqrt(bits(16.0f)) == bits(4.0f));

    // Sign injection
    assert(riscv::f32_fsgnj(bits(1.5f), bits(-2.5f)) == bits(-1.5f));
    assert(riscv::f32_fsgnjn(bits(1.5f), bits(-2.5f)) == bits(1.5f));
    assert(riscv::f32_fsgnjx(bits(1.5f), bits(-2.5f)) == bits(-1.5f));

    // Min/max
    assert(riscv::f32_fmin(bits(1.5f), bits(2.5f)) == bits(1.5f));
    assert(riscv::f32_fmax(bits(1.5f), bits(2.5f)) == bits(2.5f));
    assert(riscv::f32_fmin(bits(-0.0f), bits(+0.0f)) == bits(-0.0f));
    assert(riscv::f32_fmax(bits(-0.0f), bits(+0.0f)) == bits(+0.0f));

    // Comparison
    assert(riscv::f32_feq(bits(1.0f), bits(1.0f)) == 1u);
    assert(riscv::f32_feq(bits(1.0f), bits(2.0f)) == 0u);
    assert(riscv::f32_flt(bits(1.0f), bits(2.0f)) == 1u);
    assert(riscv::f32_flt(bits(2.0f), bits(1.0f)) == 0u);
    assert(riscv::f32_fle(bits(1.0f), bits(1.0f)) == 1u);
    assert(riscv::f32_fle(bits(2.0f), bits(1.0f)) == 0u);
    assert(riscv::f32_feq(bits(std::numeric_limits<float>::quiet_NaN()), bits(1.0f)) == 0u);
    assert(riscv::f32_flt(bits(std::numeric_limits<float>::quiet_NaN()), bits(1.0f)) == 0u);
    assert(riscv::f32_fle(bits(1.0f), bits(std::numeric_limits<float>::quiet_NaN())) == 0u);

    // Integer/float moves and classify
    const auto one_bits = bits(1.0f);
    assert(riscv::f32_fmv_x_w(one_bits) == one_bits);
    assert(riscv::f32_fmv_w_x(0x3f800000u) == 0x3f800000u);
    assert(riscv::f32_classify(bits(+0.0f)) == static_cast<std::uint32_t>(riscv::FpClass::PositiveZero));
    assert(riscv::f32_classify(bits(-0.0f)) == static_cast<std::uint32_t>(riscv::FpClass::NegativeZero));
    assert(riscv::f32_classify(bits(std::numeric_limits<float>::infinity())) == static_cast<std::uint32_t>(riscv::FpClass::PositiveInfinity));
    assert(riscv::f32_classify(bits(std::numeric_limits<float>::quiet_NaN())) == static_cast<std::uint32_t>(riscv::FpClass::QuietNaN));

    // Conversions
    assert(riscv::f32_fcvt_w_s(bits(3.75f)) == 3u);
    assert(riscv::f32_fcvt_w_s(bits(-3.75f)) == static_cast<std::uint32_t>(-3));
    assert(riscv::f32_fcvt_wu_s(bits(3.75f)) == 3u);
    assert(riscv::f32_fcvt_s_w(3u) == bits(3.0f));
    assert(riscv::f32_fcvt_s_wu(3u) == bits(3.0f));
    assert(is_nan_bits(riscv::f32_fcvt_w_s(bits(std::numeric_limits<float>::quiet_NaN()))));
    assert(is_nan_bits(riscv::f32_fcvt_wu_s(bits(std::numeric_limits<float>::quiet_NaN()))));
    assert(is_nan_bits(riscv::f32_fcvt_wu_s(bits(-1.0f))));

    // Special values sanity
    assert(is_nan_bits(riscv::f32_div(bits(0.0f), bits(0.0f))));
    assert(riscv::f32_add(bits(std::numeric_limits<float>::infinity()), bits(1.0f)) == bits(std::numeric_limits<float>::infinity()));
    assert(is_nan_bits(riscv::f32_sub(bits(std::numeric_limits<float>::infinity()), bits(std::numeric_limits<float>::infinity()))));

    return 0;
}
