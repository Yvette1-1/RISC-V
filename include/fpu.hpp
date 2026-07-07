#pragma once

#include <cstdint>

namespace riscv {

struct FpuResult {
    bool ok = true;
    std::uint32_t value = 0;
    bool invalid = false;
};

std::uint32_t f32_add(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_sub(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_mul(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_div(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_sqrt(std::uint32_t a);
std::uint32_t f32_fmin(std::uint32_t a, std::uint32_t b);
std::uint32_t f32_fmax(std::uint32_t a, std::uint32_t b);

}  // namespace riscv
