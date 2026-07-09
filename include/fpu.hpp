#pragma once

#include <cstdint>

namespace riscv {

struct FpuResult {
    bool ok = true;
    std::uint32_t value = 0;
    bool invalid = false;
};

enum class FpClass : std::uint32_t {
    NegativeInfinity = 1u << 0,
    NegativeNormal = 1u << 1,
    NegativeSubnormal = 1u << 2,
    NegativeZero = 1u << 3,
    PositiveZero = 1u << 4,
    PositiveSubnormal = 1u << 5,
    PositiveNormal = 1u << 6,
    PositiveInfinity = 1u << 7,
    SignalingNaN = 1u << 8,
    QuietNaN = 1u << 9,
};

std::uint32_t f32_add(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_sub(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_mul(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_div(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_sqrt(std::uint32_t a);
std::uint32_t f32_fmin(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fmax(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fsgnj(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fsgnjn(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fsgnjx(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_feq(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_flt(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fle(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fmv_x_w(std::uint32_t a);
std::uint32_t f32_fmv_w_x(std::uint32_t a);
std::uint32_t f32_fcvt_w_s(std::uint32_t a);
std::uint32_t f32_fcvt_wu_s(std::uint32_t a);
std::uint32_t f32_fcvt_s_w(std::uint32_t a);
std::uint32_t f32_fcvt_s_wu(std::uint32_t a);
std::uint32_t f32_classify(std::uint32_t a);

void fpu_clear_flags();
std::uint32_t fpu_flags();

}  // namespace riscv
