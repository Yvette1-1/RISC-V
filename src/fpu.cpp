#include "fpu.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace riscv {
namespace {

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

}  // namespace

std::uint32_t f32_add(std::uint32_t a, std::uint32_t b) { return to_bits(to_float(a) + to_float(b)); }
std::uint32_t f32_sub(std::uint32_t a, std::uint32_t b) { return to_bits(to_float(a) - to_float(b)); }
std::uint32_t f32_mul(std::uint32_t a, std::uint32_t b) { return to_bits(to_float(a) * to_float(b)); }
std::uint32_t f32_div(std::uint32_t a, std::uint32_t b) { return to_bits(to_float(a) / to_float(b)); }
std::uint32_t f32_sqrt(std::uint32_t a) { return to_bits(std::sqrt(to_float(a))); }
std::uint32_t f32_fmin(std::uint32_t a, std::uint32_t b) { return to_bits(std::fmin(to_float(a), to_float(b))); }
std::uint32_t f32_fmax(std::uint32_t a, std::uint32_t b) { return to_bits(std::fmax(to_float(a), to_float(b))); }

}  // namespace riscv
