#include "elf_loader.hpp"
#include "memory.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

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

void write_u32(std::ofstream& out, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu),
        static_cast<char>((value >> 16) & 0xffu),
        static_cast<char>((value >> 24) & 0xffu),
    };
    out.write(bytes, 4);
}

void write_header(std::ofstream& out, std::uint16_t phnum, std::uint32_t phoff = sizeof(Elf32HeaderDisk)) {
    Elf32HeaderDisk hdr{};
    hdr.ident[0] = 0x7f;
    hdr.ident[1] = 'E';
    hdr.ident[2] = 'L';
    hdr.ident[3] = 'F';
    hdr.ident[4] = 1;
    hdr.ident[5] = 1;
    hdr.ident[6] = 1;
    hdr.type = 2;
    hdr.machine = 243;
    hdr.version = 1;
    hdr.entry = 0x1000u;
    hdr.phoff = phoff;
    hdr.ehsize = sizeof(Elf32HeaderDisk);
    hdr.phentsize = sizeof(Elf32ProgramHeaderDisk);
    hdr.phnum = phnum;
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
}

void write_ph(std::ofstream& out, std::uint32_t type, std::uint32_t offset, std::uint32_t vaddr, std::uint32_t filesz, std::uint32_t memsz, std::uint32_t flags) {
    Elf32ProgramHeaderDisk ph{};
    ph.type = type;
    ph.offset = offset;
    ph.vaddr = vaddr;
    ph.paddr = vaddr;
    ph.filesz = filesz;
    ph.memsz = memsz;
    ph.flags = flags;
    ph.align = 0x1000u;
    out.write(reinterpret_cast<const char*>(&ph), sizeof(ph));
}

}  // namespace

int main() {
    {
        std::ofstream out("bad_magic.elf", std::ios::binary | std::ios::trunc);
        write_header(out, 0);
        out.seekp(0);
        out.write("NOPE", 4);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("bad_magic.elf", mem);
        assert(!res.ok);
    }

    {
        std::ofstream out("bad_segment.elf", std::ios::binary | std::ios::trunc);
        write_header(out, 1);
        write_ph(out, 1, 0x200, 0x1000u, 0x200u, 0x100u, 0x5u); // memsz < filesz
        out.seekp(0x200);
        write_u32(out, 0x12345678u);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("bad_segment.elf", mem);
        assert(!res.ok);
    }

    {
        std::ofstream out("bss.elf", std::ios::binary | std::ios::trunc);
        write_header(out, 1);
        write_ph(out, 1, 0x200, 0x1000u, 4u, 16u, riscv::MEM_READ | riscv::MEM_WRITE);
        out.seekp(0x200);
        write_u32(out, 0x01020304u);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("bss.elf", mem);
        assert(res.ok);
        std::uint32_t value = 0;
        assert(mem.load32(0x1000u, value) && value == 0x01020304u);
        std::uint32_t zero = 0xffffffffu;
        assert(mem.load32(0x1004u, zero) && zero == 0u);
    }

    std::remove("bad_magic.elf");
    std::remove("bad_segment.elf");
    std::remove("bss.elf");
    return 0;
}
