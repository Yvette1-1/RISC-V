#include "elf_loader.hpp"
#include "memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace riscv {
namespace {

constexpr std::array<std::uint8_t, 4> kElfMagic{0x7fu, 'E', 'L', 'F'};
constexpr std::uint8_t kElfClass32 = 1;
constexpr std::uint8_t kElfDataLittleEndian = 1;
constexpr std::uint8_t kElfVersionCurrent = 1;
constexpr std::uint16_t kElfMachineRiscv = 243;
constexpr std::uint32_t kProgramHeaderLoad = 1;
constexpr std::size_t kElf32HeaderSize = 52;
constexpr std::size_t kElf32ProgramHeaderSize = 32;

struct Elf32Header {
    std::array<std::uint8_t, 16> ident{};
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint32_t version = 0;
    std::uint32_t entry = 0;
    std::uint32_t program_header_offset = 0;
    std::uint32_t section_header_offset = 0;
    std::uint32_t flags = 0;
    std::uint16_t header_size = 0;
    std::uint16_t program_header_entry_size = 0;
    std::uint16_t program_header_count = 0;
    std::uint16_t section_header_entry_size = 0;
    std::uint16_t section_header_count = 0;
    std::uint16_t section_name_index = 0;
};

struct Elf32ProgramHeader {
    std::uint32_t type = 0;
    std::uint32_t offset = 0;
    std::uint32_t virtual_addr = 0;
    std::uint32_t physical_addr = 0;
    std::uint32_t file_size = 0;
    std::uint32_t memory_size = 0;
    std::uint32_t flags = 0;
    std::uint32_t align = 0;
};

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 8 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

bool checked_range(std::size_t base, std::size_t length, std::size_t limit) {
    return base <= limit && length <= limit - base;
}

ElfLoadResult fail(std::string message) {
    ElfLoadResult result;
    result.error = std::move(message);
    return result;
}

Elf32Header parse_header(const std::vector<std::uint8_t>& bytes) {
    Elf32Header header;
    for (std::size_t i = 0; i < header.ident.size(); ++i) {
        header.ident[i] = bytes[i];
    }
    header.type = read_u16_le(bytes, 16);
    header.machine = read_u16_le(bytes, 18);
    header.version = read_u32_le(bytes, 20);
    header.entry = read_u32_le(bytes, 24);
    header.program_header_offset = read_u32_le(bytes, 28);
    header.section_header_offset = read_u32_le(bytes, 32);
    header.flags = read_u32_le(bytes, 36);
    header.header_size = read_u16_le(bytes, 40);
    header.program_header_entry_size = read_u16_le(bytes, 42);
    header.program_header_count = read_u16_le(bytes, 44);
    header.section_header_entry_size = read_u16_le(bytes, 46);
    header.section_header_count = read_u16_le(bytes, 48);
    header.section_name_index = read_u16_le(bytes, 50);
    return header;
}

Elf32ProgramHeader parse_program_header(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    Elf32ProgramHeader header;
    header.type = read_u32_le(bytes, offset);
    header.offset = read_u32_le(bytes, offset + 4);
    header.virtual_addr = read_u32_le(bytes, offset + 8);
    header.physical_addr = read_u32_le(bytes, offset + 12);
    header.file_size = read_u32_le(bytes, offset + 16);
    header.memory_size = read_u32_le(bytes, offset + 20);
    header.flags = read_u32_le(bytes, offset + 24);
    header.align = read_u32_le(bytes, offset + 28);
    return header;
}

std::uint32_t elf_flags_to_memory_permissions(std::uint32_t flags) {
    if ((flags & (MEM_READ | MEM_WRITE | MEM_EXEC)) != 0u) {
        return flags & (MEM_READ | MEM_WRITE | MEM_EXEC);
    }
    std::uint32_t permissions = MEM_NONE;
    if ((flags & 0x4u) != 0u) permissions |= MEM_READ;
    if ((flags & 0x2u) != 0u) permissions |= MEM_WRITE;
    if ((flags & 0x1u) != 0u) permissions |= MEM_EXEC;
    return permissions;
}

bool validate_header(const Elf32Header& header, std::size_t file_size, std::string& error) {
    if (!std::equal(kElfMagic.begin(), kElfMagic.end(), header.ident.begin())) { error = "invalid ELF magic"; return false; }
    if (header.ident[4] != kElfClass32) { error = "only ELF32 is supported"; return false; }
    if (header.ident[5] != kElfDataLittleEndian) { error = "only little-endian ELF is supported"; return false; }
    if (header.ident[6] != kElfVersionCurrent || header.version != kElfVersionCurrent) { error = "unsupported ELF version"; return false; }
    if (header.machine != kElfMachineRiscv) { error = "ELF machine is not RISC-V"; return false; }
    if (header.header_size < kElf32HeaderSize) { error = "ELF header is too small"; return false; }
    if (header.program_header_count == 0) { error = "ELF has no program headers"; return false; }
    if (header.program_header_entry_size < kElf32ProgramHeaderSize) { error = "ELF program header entry is too small"; return false; }

    const auto table_offset = static_cast<std::size_t>(header.program_header_offset);
    const auto entry_size = static_cast<std::size_t>(header.program_header_entry_size);
    const auto entry_count = static_cast<std::size_t>(header.program_header_count);
    if (entry_size != 0 && entry_count > (std::numeric_limits<std::size_t>::max() / entry_size)) { error = "ELF program header table size overflow"; return false; }
    if (!checked_range(table_offset, entry_size * entry_count, file_size)) { error = "ELF program header table is outside file"; return false; }
    return true;
}

}  // namespace

ElfLoadResult load_elf(const std::string& path, Memory& memory) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return fail("failed to open ELF file: " + path);

    input.seekg(0, std::ios::end);
    const auto file_size_pos = input.tellg();
    if (file_size_pos < 0) return fail("failed to determine ELF file size");
    const auto file_size = static_cast<std::size_t>(file_size_pos);
    input.seekg(0, std::ios::beg);
    if (file_size < kElf32HeaderSize) return fail("file is too small to be an ELF32 file");

    std::vector<std::uint8_t> bytes(file_size);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return fail("failed to read ELF file contents");
    }

    const auto elf_header = parse_header(bytes);
    std::string error;
    if (!validate_header(elf_header, bytes.size(), error)) return fail(error);

    ElfLoadResult result;
    result.ok = true;
    result.entry_point = elf_header.entry;

    bool loaded_segment = false;
    for (std::uint16_t i = 0; i < elf_header.program_header_count; ++i) {
        const auto offset = static_cast<std::size_t>(elf_header.program_header_offset) + static_cast<std::size_t>(i) * elf_header.program_header_entry_size;
        const auto program_header = parse_program_header(bytes, offset);
        if (program_header.type != kProgramHeaderLoad) continue;
        if (program_header.file_size > program_header.memory_size) return fail("ELF load segment file size is larger than memory size");
        if (!checked_range(program_header.offset, program_header.file_size, bytes.size())) return fail("ELF load segment is outside file");
        if (!memory.contains(program_header.virtual_addr, program_header.memory_size)) return fail("ELF load segment is outside simulator memory");

        const auto permissions = elf_flags_to_memory_permissions(program_header.flags);
        const auto load_permissions = static_cast<std::uint32_t>(permissions | MEM_WRITE);
        MemoryRegion region{program_header.virtual_addr, program_header.memory_size, load_permissions, "elf_segment"};
        if (!memory.map_region(region)) return fail("failed to map ELF load segment");
        if (!memory.store_bytes(program_header.virtual_addr, bytes.data() + program_header.offset, program_header.file_size)) return fail("failed to copy ELF load segment into memory");
        const auto zero_fill_size = program_header.memory_size - program_header.file_size;
        if (zero_fill_size != 0 && !memory.fill(program_header.virtual_addr + program_header.file_size, 0, zero_fill_size)) return fail("failed to zero-fill ELF bss segment");

        result.segments.push_back({program_header.virtual_addr, program_header.memory_size, program_header.file_size, permissions, program_header.offset});
        loaded_segment = true;
    }

    if (!loaded_segment) return fail("ELF has no loadable segments");
    if (!memory.contains(elf_header.entry, 4)) return fail("ELF entry point is outside simulator memory");
    return result;
}

std::uint32_t get_entry_point(const ElfLoadResult& result) {
    return result.entry_point;
}

}  // namespace riscv
