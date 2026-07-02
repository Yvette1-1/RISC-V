#include "elf_loader.hpp"
#include "memory.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

namespace riscv {

namespace {

#pragma pack(push, 1)
struct Elf32HeaderDisk {
    std::uint8_t ident[16];
    std::uint16_t type;
    std::uint16_t machine;
    std::uint32_t version;
    std::uint32_t entry;
    std::uint32_t phoff;
    std::uint32_t shoff;
    std::uint32_t flags;
    std::uint16_t ehsize;
    std::uint16_t phentsize;
    std::uint16_t phnum;
    std::uint16_t shentsize;
    std::uint16_t shnum;
    std::uint16_t shstrndx;
};

struct Elf32ProgramHeaderDisk {
    std::uint32_t type;
    std::uint32_t offset;
    std::uint32_t vaddr;
    std::uint32_t paddr;
    std::uint32_t filesz;
    std::uint32_t memsz;
    std::uint32_t flags;
    std::uint32_t align;
};
#pragma pack(pop)

constexpr std::uint32_t PT_LOAD = 1u;

bool valid_ident(const Elf32HeaderDisk& hdr) {
    return hdr.ident[0] == 0x7f && hdr.ident[1] == 'E' && hdr.ident[2] == 'L' && hdr.ident[3] == 'F' &&
           hdr.ident[4] == 1 && hdr.ident[5] == 1 && hdr.ident[6] == 1;
}

bool safe_add(std::uint32_t a, std::uint32_t b, std::uint32_t& out) {
    const std::uint64_t sum = static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b);
    if (sum > std::numeric_limits<std::uint32_t>::max()) return false;
    out = static_cast<std::uint32_t>(sum);
    return true;
}

}  // namespace

ElfLoadResult load_elf(const std::string& path, Memory& memory) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {false, 0u, {}, "failed to open ELF file"};
    }

    Elf32HeaderDisk hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) {
        return {false, 0u, {}, "failed to read ELF header"};
    }
    if (!valid_ident(hdr)) {
        return {false, 0u, {}, "invalid ELF magic"};
    }
    if (hdr.machine != 243 || hdr.version != 1) {
        return {false, 0u, {}, "unsupported ELF target"};
    }
    if (hdr.ehsize < sizeof(Elf32HeaderDisk) || hdr.phentsize < sizeof(Elf32ProgramHeaderDisk)) {
        return {false, 0u, {}, "corrupt ELF header sizes"};
    }
    if (hdr.phnum == 0) {
        return {false, 0u, {}, "ELF has no program headers"};
    }

    const auto file_pos = in.tellg();
    in.seekg(0, std::ios::end);
    const auto file_size = in.tellg();
    in.seekg(file_pos, std::ios::beg);

    ElfLoadResult result{};
    result.ok = true;
    result.entry_point = hdr.entry;

    in.seekg(static_cast<std::streamoff>(hdr.phoff), std::ios::beg);
    if (!in) {
        return {false, 0u, {}, "failed to seek program headers"};
    }

    for (std::uint16_t i = 0; i < hdr.phnum; ++i) {
        Elf32ProgramHeaderDisk ph{};
        in.read(reinterpret_cast<char*>(&ph), sizeof(ph));
        if (!in) {
            return {false, 0u, {}, "failed to read program header"};
        }
        if (ph.type != PT_LOAD) {
            continue;
        }
        if (ph.memsz < ph.filesz) {
            return {false, 0u, {}, "ELF segment memsz smaller than filesz"};
        }

        std::uint32_t end_vaddr = 0;
        if (!safe_add(ph.vaddr, ph.memsz, end_vaddr)) {
            return {false, 0u, {}, "ELF segment address overflow"};
        }

        std::uint32_t end_offset = 0;
        if (!safe_add(ph.offset, ph.filesz, end_offset)) {
            return {false, 0u, {}, "ELF file offset overflow"};
        }
        if (static_cast<std::uint64_t>(end_offset) > static_cast<std::uint64_t>(file_size)) {
            return {false, 0u, {}, "ELF segment extends past end of file"};
        }
        if (ph.memsz > memory.size()) {
            return {false, 0u, {}, "ELF segment does not fit in memory"};
        }
        if (!memory.map_region({ph.vaddr, ph.memsz, ph.flags, "segment"})) {
            return {false, 0u, {}, "failed to map ELF segment"};
        }

        const auto cur = in.tellg();
        in.seekg(static_cast<std::streamoff>(ph.offset), std::ios::beg);
        if (!in) {
            return {false, 0u, {}, "failed to seek segment"};
        }

        std::vector<std::uint8_t> buf(ph.filesz);
        if (ph.filesz != 0) {
            in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
            if (!in) {
                return {false, 0u, {}, "failed to read segment data"};
            }
            std::memcpy(memory.data() + ph.vaddr, buf.data(), buf.size());
        }

        if (ph.memsz > ph.filesz) {
            std::memset(memory.data() + ph.vaddr + ph.filesz, 0, ph.memsz - ph.filesz);
        }

        in.seekg(cur);
        result.segments.push_back(ElfSegment{ph.vaddr, ph.memsz, ph.filesz, ph.flags, ph.offset});
    }

    return result;
}

std::uint32_t get_entry_point(const ElfLoadResult& result) {
    return result.entry_point;
}

}  // namespace riscv
