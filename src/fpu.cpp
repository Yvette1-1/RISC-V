#include "fpu.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace riscv {
namespace {

constexpr std::uint32_t kFlagInexact = 1u << 0;
constexpr std::uint32_t kFlagUnderflow = 1u << 1;
constexpr std::uint32_t kFlagOverflow = 1u << 2;
constexpr std::uint32_t kFlagDivByZero = 1u << 3;
constexpr std::uint32_t kFlagInvalid = 1u << 4;

std::uint32_t g_fpu_flags = 0;

float to_float(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

std::uint32_t to_bits(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

bool is_snan_bits(std::uint32_t bits) {
    const auto exp = (bits >> 23) & 0xffu;
    const auto frac = bits & 0x7fffffu;
    return exp == 0xffu && frac != 0u && (frac & 0x00400000u) == 0u;
}

bool is_qnan_bits(std::uint32_t bits) {
    const auto exp = (bits >> 23) & 0xffu;
    const auto frac = bits & 0x7fffffu;
    return exp == 0xffu && frac != 0u && (frac & 0x00400000u) != 0u;
}

bool is_nan_bits(std::uint32_t bits) {
    return is_snan_bits(bits) || is_qnan_bits(bits);
}

bool is_zero_bits(std::uint32_t bits) {
    return (bits & 0x7fffffffu) == 0u;
}

bool is_subnormal_bits(std::uint32_t bits) {
    return ((bits >> 23) & 0xffu) == 0u && (bits & 0x7fffffu) != 0u;
}

bool is_infinite_bits(std::uint32_t bits) {
    return ((bits >> 23) & 0xffu) == 0xffu && (bits & 0x7fffffu) == 0u;
}

void set_flag(std::uint32_t flag) {
    g_fpu_flags |= flag;
}

std::uint32_t canonical_nan() {
    return 0x7fc00000u;
}

}  // namespace

void fpu_clear_flags() { g_fpu_flags = 0; }
std::uint32_t fpu_flags() { return g_fpu_flags; }

std::uint32_t f32_add(std::uint32_t a, std::uint32_t b) {
    fpu_clear_flags();
    if (is_nan_bits(a) || is_nan_bits(b)) { set_flag(kFlagInvalid); return canonical_nan(); }
    const long double exact = static_cast<long double>(to_float(a)) + static_cast<long double>(to_float(b));
    const float rf = static_cast<float>(exact);
    const auto rb = to_bits(rf);
    if (is_nan_bits(rb)) set_flag(kFlagInvalid);
    if (std::isinf(rf)) set_flag(kFlagOverflow);
    if (static_cast<long double>(rf) != exact) set_flag(kFlagInexact);
    return rb;
}
std::uint32_t f32_sub(std::uint32_t a, std::uint32_t b) {
    fpu_clear_flags();
    if (is_nan_bits(a) || is_nan_bits(b)) { set_flag(kFlagInvalid); return canonical_nan(); }
    const long double exact = static_cast<long double>(to_float(a)) - static_cast<long double>(to_float(b));
    const float rf = static_cast<float>(exact);
    if (std::isnan(rf)) { set_flag(kFlagInvalid); return canonical_nan(); }
    if (std::isinf(rf)) set_flag(kFlagOverflow);
    if (static_cast<long double>(rf) != exact) set_flag(kFlagInexact);
    return to_bits(rf);
}
std::uint32_t f32_mul(std::uint32_t a, std::uint32_t b) {
    fpu_clear_flags();
    if (is_nan_bits(a) || is_nan_bits(b)) { set_flag(kFlagInvalid); return canonical_nan(); }
    const long double exact = static_cast<long double>(to_float(a)) * static_cast<long double>(to_float(b));
    const float rf = static_cast<float>(exact);
    const auto rb = to_bits(rf);
    if (is_nan_bits(rb)) set_flag(kFlagInvalid);
    if (std::isinf(rf)) set_flag(kFlagOverflow);
    if (static_cast<long double>(rf) != exact) set_flag(kFlagInexact);
    return rb;
}
std::uint32_t f32_div(std::uint32_t a, std::uint32_t b) {
    fpu_clear_flags();
    if (is_nan_bits(a) || is_nan_bits(b)) { set_flag(kFlagInvalid); return canonical_nan(); }
    if (is_zero_bits(b)) { set_flag(kFlagDivByZero); return canonical_nan(); }
    const long double exact = static_cast<long double>(to_float(a)) / static_cast<long double>(to_float(b));
    const float rf = static_cast<float>(exact);
    const auto rb = to_bits(rf);
    if (is_nan_bits(rb)) set_flag(kFlagInvalid);
    if (std::isinf(rf)) set_flag(kFlagOverflow);
    if (static_cast<long double>(rf) != exact) set_flag(kFlagInexact);
    return rb;
}
std::uint32_t f32_sqrt(std::uint32_t a) {
    fpu_clear_flags();
    if (is_nan_bits(a)) { set_flag(kFlagInvalid); return canonical_nan(); }
    if (to_float(a) < 0.0f) { set_flag(kFlagInvalid); return canonical_nan(); }
    const long double exact = std::sqrt(static_cast<long double>(to_float(a)));
    const float rf = static_cast<float>(exact);
    if (static_cast<long double>(rf) != exact) set_flag(kFlagInexact);
    return to_bits(rf);
}
std::uint32_t f32_fmin(std::uint32_t a, std::uint32_t b) {
    fpu_clear_flags();
    if (is_snan_bits(a) || is_snan_bits(b)) { set_flag(kFlagInvalid); return canonical_nan(); }
    if (is_nan_bits(a) && is_nan_bits(b)) return canonical_nan();
    if (is_nan_bits(a)) return b;
    if (is_nan_bits(b)) return a;
    if (is_zero_bits(a) && is_zero_bits(b)) return (a & 0x80000000u) ? a : b;
    return to_bits(std::fmin(to_float(a), to_float(b)));
}
std::uint32_t f32_fmax(std::uint32_t a, std::uint32_t b) {
    fpu_clear_flags();
    if (is_snan_bits(a) || is_snan_bits(b)) { set_flag(kFlagInvalid); return canonical_nan(); }
    if (is_nan_bits(a) && is_nan_bits(b)) return canonical_nan();
    if (is_nan_bits(a)) return b;
    if (is_nan_bits(b)) return a;
    if (is_zero_bits(a) && is_zero_bits(b)) return (a & 0x80000000u) ? b : a;
    return to_bits(std::fmax(to_float(a), to_float(b)));
}
std::uint32_t f32_fsgnj(std::uint32_t a, std::uint32_t b) { return (a & 0x7fffffffu) | (b & 0x80000000u); }
std::uint32_t f32_fsgnjn(std::uint32_t a, std::uint32_t b) { return (a & 0x7fffffffu) | (~b & 0x80000000u); }
std::uint32_t f32_fsgnjx(std::uint32_t a, std::uint32_t b) { return (a & 0x7fffffffu) | ((a ^ b) & 0x80000000u); }
std::uint32_t f32_feq(std::uint32_t a, std::uint32_t b) {
    const bool any_nan = is_nan_bits(a) || is_nan_bits(b);
    const bool has_snan = is_snan_bits(a) || is_snan_bits(b);
    if (has_snan) {
        set_flag(kFlagInvalid);
    }
    if (any_nan) {
        return 0u;
    }
    const bool both_zero = ((a & 0x7fffffffu) == 0u) && ((b & 0x7fffffffu) == 0u);
    if (both_zero) {
        return 1u;
    }
    return (a == b) ? 1u : 0u;
}
std::uint32_t f32_flt(std::uint32_t a, std::uint32_t b) {
    const bool any_nan = is_nan_bits(a) || is_nan_bits(b);
    const bool has_snan = is_snan_bits(a) || is_snan_bits(b);
    if (has_snan) {
        set_flag(kFlagInvalid);
    }
    if (any_nan) {
        return 0u;
    }
    return (to_float(a) < to_float(b)) ? 1u : 0u;
}
std::uint32_t f32_fle(std::uint32_t a, std::uint32_t b) {
    const bool any_nan = is_nan_bits(a) || is_nan_bits(b);
    const bool has_snan = is_snan_bits(a) || is_snan_bits(b);
    if (has_snan) {
        set_flag(kFlagInvalid);
    }
    if (any_nan) {
        return 0u;
    }
    return (to_float(a) <= to_float(b)) ? 1u : 0u;
}
std::uint32_t f32_fmv_x_w(std::uint32_t a) { return a; }
std::uint32_t f32_fmv_w_x(std::uint32_t a) { return a; }
std::uint32_t f32_fcvt_w_s(std::uint32_t a) {
    fpu_clear_flags();
    if (is_nan_bits(a) || is_infinite_bits(a)) { set_flag(kFlagInvalid); return 0x80000000u; }
    const auto f = to_float(a);
    const auto t = std::trunc(f);
    if (t != f) set_flag(kFlagInexact);
    if (t > static_cast<float>(std::numeric_limits<std::int32_t>::max()) || t < static_cast<float>(std::numeric_limits<std::int32_t>::min())) {
        set_flag(kFlagInvalid);
        return 0x80000000u;
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}
std::uint32_t f32_fcvt_wu_s(std::uint32_t a) {
    fpu_clear_flags();
    if (is_nan_bits(a) || is_infinite_bits(a) || to_float(a) < 0.0f) { set_flag(kFlagInvalid); return 0xffffffffu; }
    const auto f = to_float(a);
    const auto t = std::trunc(f);
    if (t != f) set_flag(kFlagInexact);
    if (t > static_cast<float>(std::numeric_limits<std::uint32_t>::max())) { set_flag(kFlagInvalid); return 0xffffffffu; }
    return static_cast<std::uint32_t>(t);
}
std::uint32_t f32_fcvt_s_w(std::uint32_t a) { return to_bits(static_cast<float>(static_cast<std::int32_t>(a))); }
std::uint32_t f32_fcvt_s_wu(std::uint32_t a) { return to_bits(static_cast<float>(a)); }
std::uint32_t f32_classify(std::uint32_t a) {
    const bool sign = (a & 0x80000000u) != 0u;
    if (is_snan_bits(a)) return static_cast<std::uint32_t>(FpClass::SignalingNaN);
    if (is_qnan_bits(a)) return static_cast<std::uint32_t>(FpClass::QuietNaN);
    if (is_infinite_bits(a)) return sign ? static_cast<std::uint32_t>(FpClass::NegativeInfinity) : static_cast<std::uint32_t>(FpClass::PositiveInfinity);
    if (is_zero_bits(a)) return sign ? static_cast<std::uint32_t>(FpClass::NegativeZero) : static_cast<std::uint32_t>(FpClass::PositiveZero);
    if (is_subnormal_bits(a)) return sign ? static_cast<std::uint32_t>(FpClass::NegativeSubnormal) : static_cast<std::uint32_t>(FpClass::PositiveSubnormal);
    return sign ? static_cast<std::uint32_t>(FpClass::NegativeNormal) : static_cast<std::uint32_t>(FpClass::PositiveNormal);
}

}  // namespace riscv
