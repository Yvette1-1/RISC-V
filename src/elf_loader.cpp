#include "elf_loader.hpp"
#include "memory.hpp"

#include <algorithm>
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
constexpr std::uint8_t kElfClass64 = 2;
constexpr std::uint8_t kElfDataLittleEndian = 1;
constexpr std::uint8_t kElfVersionCurrent = 1;
constexpr std::uint16_t kElfMachineRiscv = 243;
constexpr std::uint32_t kProgramHeaderLoad = 1;
constexpr std::uint32_t kSectionTypeProgbits = 1;
constexpr std::uint32_t kSectionTypeNobits = 8;
constexpr std::uint32_t kSectionFlagAlloc = 0x2u;
constexpr std::size_t kElf32HeaderSize = 52;
constexpr std::size_t kElf64HeaderSize = 64;
constexpr std::size_t kElf32ProgramHeaderSize = 32;
constexpr std::size_t kElf64ProgramHeaderSize = 56;
constexpr std::size_t kElf32SectionHeaderSize = 40;
constexpr std::size_t kElf64SectionHeaderSize = 64;

struct ElfHeader {
    std::array<std::uint8_t, 16> ident{};
    bool is_64 = false;
    std::uint16_t machine = 0;
    std::uint32_t version = 0;
    std::uint64_t entry = 0;
    std::uint64_t phoff = 0;
    std::uint64_t shoff = 0;
    std::uint16_t ehsize = 0;
    std::uint16_t phentsize = 0;
    std::uint16_t phnum = 0;
    std::uint16_t shentsize = 0;
    std::uint16_t shnum = 0;
};

struct ProgramHeader {
    std::uint32_t type = 0;
    std::uint64_t offset = 0;
    std::uint64_t vaddr = 0;
    std::uint64_t filesz = 0;
    std::uint64_t memsz = 0;
    std::uint64_t flags = 0;
};

struct SectionHeader {
    std::uint32_t type = 0;
    std::uint64_t flags = 0;
    std::uint64_t addr = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
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

std::uint64_t read_u64_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint64_t>(read_u32_le(bytes, offset)) |
           (static_cast<std::uint64_t>(read_u32_le(bytes, offset + 4)) << 32);
}

bool checked_range(std::uint64_t base, std::uint64_t length, std::size_t limit) {
    return base <= limit && length <= static_cast<std::uint64_t>(limit) - base;
}

ElfLoadResult fail(std::string message) {
    ElfLoadResult result;
    result.error = std::move(message);
    return result;
}

ElfHeader parse_header(const std::vector<std::uint8_t>& bytes) {
    ElfHeader header;
    for (std::size_t i = 0; i < header.ident.size(); ++i) header.ident[i] = bytes[i];
    header.is_64 = header.ident[4] == kElfClass64;
    header.machine = read_u16_le(bytes, 18);
    header.version = read_u32_le(bytes, 20);
    if (!header.is_64) {
        header.entry = read_u32_le(bytes, 24);
        header.phoff = read_u32_le(bytes, 28);
        header.shoff = read_u32_le(bytes, 32);
        header.ehsize = read_u16_le(bytes, 40);
        header.phentsize = read_u16_le(bytes, 42);
        header.phnum = read_u16_le(bytes, 44);
        header.shentsize = read_u16_le(bytes, 46);
        header.shnum = read_u16_le(bytes, 48);
    } else {
        header.entry = read_u64_le(bytes, 24);
        header.phoff = read_u64_le(bytes, 32);
        header.shoff = read_u64_le(bytes, 40);
        header.ehsize = read_u16_le(bytes, 52);
        header.phentsize = read_u16_le(bytes, 54);
        header.phnum = read_u16_le(bytes, 56);
        header.shentsize = read_u16_le(bytes, 58);
        header.shnum = read_u16_le(bytes, 60);
    }
    return header;
}

ProgramHeader parse_program_header(const std::vector<std::uint8_t>& bytes, std::size_t offset, bool is_64) {
    ProgramHeader ph;
    if (!is_64) {
        ph.type = read_u32_le(bytes, offset);
        ph.offset = read_u32_le(bytes, offset + 4);
        ph.vaddr = read_u32_le(bytes, offset + 8);
        ph.filesz = read_u32_le(bytes, offset + 16);
        ph.memsz = read_u32_le(bytes, offset + 20);
        ph.flags = read_u32_le(bytes, offset + 24);
    } else {
        ph.type = read_u32_le(bytes, offset);
        ph.flags = read_u32_le(bytes, offset + 4);
        ph.offset = read_u64_le(bytes, offset + 8);
        ph.vaddr = read_u64_le(bytes, offset + 16);
        ph.filesz = read_u64_le(bytes, offset + 32);
        ph.memsz = read_u64_le(bytes, offset + 40);
    }
    return ph;
}

SectionHeader parse_section_header(const std::vector<std::uint8_t>& bytes, std::size_t offset, bool is_64) {
    SectionHeader sh;
    sh.type = read_u32_le(bytes, offset + 4);
    if (!is_64) {
        sh.flags = read_u32_le(bytes, offset + 8);
        sh.addr = read_u32_le(bytes, offset + 12);
        sh.offset = read_u32_le(bytes, offset + 16);
        sh.size = read_u32_le(bytes, offset + 20);
    } else {
        sh.flags = read_u64_le(bytes, offset + 8);
        sh.addr = read_u64_le(bytes, offset + 16);
        sh.offset = read_u64_le(bytes, offset + 24);
        sh.size = read_u64_le(bytes, offset + 32);
    }
    return sh;
}

std::uint32_t program_permissions(std::uint64_t flags) {
    if ((flags & (MEM_READ | MEM_WRITE | MEM_EXEC)) != 0u) {
        return static_cast<std::uint32_t>(flags & (MEM_READ | MEM_WRITE | MEM_EXEC));
    }
    std::uint32_t permissions = MEM_NONE;
    if ((flags & 0x4u) != 0u) permissions |= MEM_READ;
    if ((flags & 0x2u) != 0u) permissions |= MEM_WRITE;
    if ((flags & 0x1u) != 0u) permissions |= MEM_EXEC;
    return permissions;
}

std::uint32_t section_permissions(std::uint64_t flags) {
    std::uint32_t permissions = MEM_READ;
    if ((flags & 0x1u) != 0u) permissions |= MEM_WRITE;
    if ((flags & 0x4u) != 0u) permissions |= MEM_EXEC;
    return permissions;
}

bool validate_header(const ElfHeader& header, std::size_t file_size, std::string& error) {
    if (!std::equal(kElfMagic.begin(), kElfMagic.end(), header.ident.begin())) { error = "invalid ELF magic"; return false; }
    if (header.ident[4] != kElfClass32 && header.ident[4] != kElfClass64) { error = "unsupported ELF class"; return false; }
    if (header.ident[5] != kElfDataLittleEndian) { error = "only little-endian ELF is supported"; return false; }
    if (header.ident[6] != kElfVersionCurrent || header.version != kElfVersionCurrent) { error = "unsupported ELF version"; return false; }
    if (header.machine != kElfMachineRiscv) { error = "ELF machine is not RISC-V"; return false; }
    if (header.ehsize < (header.is_64 ? kElf64HeaderSize : kElf32HeaderSize)) { error = "ELF header is too small"; return false; }
    if (header.phnum == 0 && header.shnum == 0) { error = "ELF has no loadable content"; return false; }
    if (header.phnum != 0 && header.phentsize < (header.is_64 ? kElf64ProgramHeaderSize : kElf32ProgramHeaderSize)) { error = "ELF program header entry is too small"; return false; }
    if (header.shnum != 0 && header.shentsize < (header.is_64 ? kElf64SectionHeaderSize : kElf32SectionHeaderSize)) { error = "ELF section header entry is too small"; return false; }
    if (header.phnum != 0 && !checked_range(header.phoff, static_cast<std::uint64_t>(header.phentsize) * header.phnum, file_size)) { error = "ELF program header table is outside file"; return false; }
    if (header.shnum != 0 && !checked_range(header.shoff, static_cast<std::uint64_t>(header.shentsize) * header.shnum, file_size)) { error = "ELF section header table is outside file"; return false; }
    return true;
}

bool load_region(Memory& memory,
                 const std::vector<std::uint8_t>& bytes,
                 std::uint64_t addr,
                 std::uint64_t offset,
                 std::uint64_t file_size,
                 std::uint64_t mem_size,
                 std::uint32_t permissions,
                 const char* name,
                 ElfLoadResult& result) {
    if (mem_size == 0) return true;
    if (file_size > mem_size) return false;
    if (!checked_range(offset, file_size, bytes.size())) return false;
    const auto vaddr = static_cast<std::uint32_t>(addr);
    if (!memory.contains(vaddr, static_cast<std::size_t>(mem_size))) return false;
    const MemoryRegion region{vaddr, static_cast<std::uint32_t>(mem_size), static_cast<std::uint32_t>(MEM_READ | MEM_WRITE | permissions), name};
    if (!memory.map_region(region)) return false;
    if (file_size != 0 && !memory.store_bytes(vaddr, bytes.data() + static_cast<std::size_t>(offset), static_cast<std::size_t>(file_size))) return false;
    if (mem_size > file_size && !memory.fill(vaddr + static_cast<std::uint32_t>(file_size), 0, static_cast<std::size_t>(mem_size - file_size))) return false;
    result.segments.push_back({vaddr, static_cast<std::uint32_t>(mem_size), static_cast<std::uint32_t>(file_size), permissions, static_cast<std::uint32_t>(offset)});
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

    std::vector<std::uint8_t> bytes(file_size);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return fail("failed to read ELF file contents");

    const auto header = parse_header(bytes);
    std::string error;
    if (!validate_header(header, bytes.size(), error)) return fail(error);

    ElfLoadResult result;
    result.ok = true;
    result.entry_point = static_cast<std::uint32_t>(header.entry);

    bool loaded = false;

    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto off = static_cast<std::size_t>(header.phoff) + static_cast<std::size_t>(i) * header.phentsize;
        const auto ph = parse_program_header(bytes, off, header.is_64);
        if (ph.type != kProgramHeaderLoad) continue;
        if (!load_region(memory, bytes, ph.vaddr, ph.offset, ph.filesz, ph.memsz, program_permissions(ph.flags), "elf_segment", result)) return fail("failed to load ELF program segment");
        loaded = true;
    }

    if (!loaded && header.shnum != 0) {
        for (std::uint16_t i = 0; i < header.shnum; ++i) {
            const auto off = static_cast<std::size_t>(header.shoff) + static_cast<std::size_t>(i) * header.shentsize;
            if (!checked_range(off, header.shentsize, bytes.size())) return fail("ELF section header is outside file");
            const auto sh = parse_section_header(bytes, off, header.is_64);
            if ((sh.flags & kSectionFlagAlloc) == 0u || sh.addr == 0 || sh.size == 0) continue;
            const auto perms = section_permissions(sh.flags);
            if (sh.type == kSectionTypeNobits) {
                if (!memory.contains(static_cast<std::uint32_t>(sh.addr), static_cast<std::size_t>(sh.size))) return fail("ELF NOBITS section is outside simulator memory");
                const MemoryRegion region{static_cast<std::uint32_t>(sh.addr), static_cast<std::uint32_t>(sh.size), static_cast<std::uint32_t>(MEM_READ | MEM_WRITE | perms), "elf_section"};
                if (!memory.map_region(region)) return fail("failed to map ELF NOBITS section");
                if (!memory.fill(static_cast<std::uint32_t>(sh.addr), 0, static_cast<std::size_t>(sh.size))) return fail("failed to zero-fill ELF NOBITS section");
                result.segments.push_back({static_cast<std::uint32_t>(sh.addr), static_cast<std::uint32_t>(sh.size), 0, perms, static_cast<std::uint32_t>(sh.offset)});
                loaded = true;
                continue;
            }
            if (sh.type != kSectionTypeProgbits) continue;
            if (!load_region(memory, bytes, sh.addr, sh.offset, sh.size, sh.size, perms, "elf_section", result)) return fail("failed to copy ELF section into memory");
            loaded = true;
        }
    }

    if (!loaded) return fail("ELF has no loadable content");
    if (!memory.contains(static_cast<std::uint32_t>(header.entry), 4)) return fail("ELF entry point is outside simulator memory");
    return result;
}

std::uint32_t get_entry_point(const ElfLoadResult& result) {
    return result.entry_point;
}

}  // namespace riscv
