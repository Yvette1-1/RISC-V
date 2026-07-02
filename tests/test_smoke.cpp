#include "elf_loader.hpp"
#include "memory.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

std::filesystem::path write_minimal_elf32() {
    constexpr std::uint32_t kEntry = 0x100u;
    constexpr std::uint32_t kSegmentOffset = 0x100u;
    constexpr std::uint32_t kSegmentFileSize = 4u;
    constexpr std::uint32_t kSegmentMemorySize = 8u;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(kSegmentOffset + kSegmentFileSize);

    bytes.insert(bytes.end(), {0x7fu, 'E', 'L', 'F'});
    bytes.push_back(1);
    bytes.push_back(1);
    bytes.push_back(1);
    bytes.push_back(0);
    while (bytes.size() < 16) {
        bytes.push_back(0);
    }
    append_u16(bytes, 2);
    append_u16(bytes, 243);
    append_u32(bytes, 1);
    append_u32(bytes, kEntry);
    append_u32(bytes, 52);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u16(bytes, 52);
    append_u16(bytes, 32);
    append_u16(bytes, 1);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);

    append_u32(bytes, 1);
    append_u32(bytes, kSegmentOffset);
    append_u32(bytes, kEntry);
    append_u32(bytes, kEntry);
    append_u32(bytes, kSegmentFileSize);
    append_u32(bytes, kSegmentMemorySize);
    append_u32(bytes, 5);
    append_u32(bytes, 0x1000);

    bytes.resize(kSegmentOffset, 0);
    bytes.insert(bytes.end(), {0x13u, 0x00u, 0x00u, 0x00u});

    const auto path = std::filesystem::temp_directory_path() / "riscv_minimal_loadable.elf";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}  // namespace

int main() {
    riscv::Memory memory(1024);
    assert(memory.size() == 1024);
    assert(memory.contains(0));
    assert(memory.contains(1023));
    assert(memory.contains(1020, 4));
    assert(!memory.contains(1024));
    assert(!memory.contains(1021, 4));

    std::uint8_t value8 = 0;
    assert(memory.store8(7, 0xabu));
    assert(memory.load8(7, value8));
    assert(value8 == 0xabu);
    assert(!memory.store8(1024, 0xffu));
    assert(!memory.load8(1024, value8));

    std::uint16_t value16 = 0;
    assert(memory.store16(8, 0x1234u));
    assert(memory.load16(8, value16));
    assert(value16 == 0x1234u);
    assert(memory.load8(8, value8));
    assert(value8 == 0x34u);
    assert(memory.load8(9, value8));
    assert(value8 == 0x12u);
    assert(!memory.store16(1023, 0xffffu));
    assert(!memory.load16(1023, value16));

    std::uint32_t value = 0;
    assert(memory.store32(12, 0x12345678u));
    assert(memory.load32(12, value));
    assert(value == 0x12345678u);
    assert(memory.load8(12, value8));
    assert(value8 == 0x78u);
    assert(memory.load8(15, value8));
    assert(value8 == 0x12u);
    assert(!memory.load32(1021, value));
    assert(!memory.store32(1021, value));

    const std::uint8_t bytes[] = {1u, 2u, 3u, 4u};
    assert(memory.store_bytes(100, bytes, sizeof(bytes)));
    assert(memory.load32(100, value));
    assert(value == 0x04030201u);
    assert(!memory.store_bytes(1022, bytes, sizeof(bytes)));
    assert(memory.fill(100, 0xa5u, sizeof(bytes)));
    assert(memory.load32(100, value));
    assert(value == 0xa5a5a5a5u);
    assert(!memory.fill(1022, 0, sizeof(bytes)));

    const auto elf_path = write_minimal_elf32();
    riscv::Memory elf_memory(4096);
    const auto load_result = riscv::load_elf(elf_path.string(), elf_memory);
    assert(load_result.ok);
    assert(load_result.entry_point == 0x100u);
    assert(elf_memory.load32(0x100u, value));
    assert(value == 0x00000013u);
    assert(elf_memory.load32(0x104u, value));
    assert(value == 0u);

    const auto missing_result = riscv::load_elf("missing-file.elf", elf_memory);
    assert(!missing_result.ok);
    assert(!missing_result.error.empty());

    std::filesystem::remove(elf_path);
    return 0;
}
