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

struct Elf32SectionHeaderDisk {
    std::uint32_t name;
    std::uint32_t type;
    std::uint32_t flags;
    std::uint32_t addr;
    std::uint32_t offset;
    std::uint32_t size;
    std::uint32_t link;
    std::uint32_t info;
    std::uint32_t addralign;
    std::uint32_t entsize;
};

struct Elf64HeaderDisk {
    std::uint8_t ident[16];
    std::uint16_t type;
    std::uint16_t machine;
    std::uint32_t version;
    std::uint64_t entry;
    std::uint64_t phoff;
    std::uint64_t shoff;
    std::uint32_t flags;
    std::uint16_t ehsize;
    std::uint16_t phentsize;
    std::uint16_t phnum;
    std::uint16_t shentsize;
    std::uint16_t shnum;
    std::uint16_t shstrndx;
};

struct Elf64ProgramHeaderDisk {
    std::uint32_t type;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t vaddr;
    std::uint64_t paddr;
    std::uint64_t filesz;
    std::uint64_t memsz;
    std::uint64_t align;
};

struct Elf64SectionHeaderDisk {
    std::uint32_t name;
    std::uint32_t type;
    std::uint64_t flags;
    std::uint64_t addr;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t link;
    std::uint32_t info;
    std::uint64_t addralign;
    std::uint64_t entsize;
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

void write_u64(std::ofstream& out, std::uint64_t value) {
    const char bytes[8] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu),
        static_cast<char>((value >> 16) & 0xffu),
        static_cast<char>((value >> 24) & 0xffu),
        static_cast<char>((value >> 32) & 0xffu),
        static_cast<char>((value >> 40) & 0xffu),
        static_cast<char>((value >> 48) & 0xffu),
        static_cast<char>((value >> 56) & 0xffu),
    };
    out.write(bytes, 8);
}

void write_padding(std::ofstream& out, std::uint64_t target) {
    const auto current = static_cast<std::uint64_t>(out.tellp());
    for (std::uint64_t i = current; i < target; ++i) out.put(0);
}

void write_elf32_header(std::ofstream& out, std::uint16_t phnum, std::uint16_t shnum, std::uint32_t phoff = sizeof(Elf32HeaderDisk), std::uint32_t shoff = 0x300) {
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
    hdr.shoff = shoff;
    hdr.ehsize = sizeof(Elf32HeaderDisk);
    hdr.phentsize = sizeof(Elf32ProgramHeaderDisk);
    hdr.phnum = phnum;
    hdr.shentsize = sizeof(Elf32SectionHeaderDisk);
    hdr.shnum = shnum;
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
}

void write_elf32_ph(std::ofstream& out, std::uint32_t type, std::uint32_t offset, std::uint32_t vaddr, std::uint32_t filesz, std::uint32_t memsz, std::uint32_t flags) {
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

void write_elf32_sh(std::ofstream& out, std::uint32_t type, std::uint32_t flags, std::uint32_t addr, std::uint32_t offset, std::uint32_t size) {
    Elf32SectionHeaderDisk sh{};
    sh.type = type;
    sh.flags = flags;
    sh.addr = addr;
    sh.offset = offset;
    sh.size = size;
    sh.addralign = 4;
    out.write(reinterpret_cast<const char*>(&sh), sizeof(sh));
}

void write_elf64_header(std::ofstream& out, std::uint16_t phnum, std::uint16_t shnum, std::uint64_t phoff = sizeof(Elf64HeaderDisk), std::uint64_t shoff = 0x400) {
    Elf64HeaderDisk hdr{};
    hdr.ident[0] = 0x7f;
    hdr.ident[1] = 'E';
    hdr.ident[2] = 'L';
    hdr.ident[3] = 'F';
    hdr.ident[4] = 2;
    hdr.ident[5] = 1;
    hdr.ident[6] = 1;
    hdr.type = 2;
    hdr.machine = 243;
    hdr.version = 1;
    hdr.entry = 0x1000u;
    hdr.phoff = phoff;
    hdr.shoff = shoff;
    hdr.ehsize = sizeof(Elf64HeaderDisk);
    hdr.phentsize = sizeof(Elf64ProgramHeaderDisk);
    hdr.phnum = phnum;
    hdr.shentsize = sizeof(Elf64SectionHeaderDisk);
    hdr.shnum = shnum;
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
}

void write_elf64_ph(std::ofstream& out, std::uint32_t type, std::uint64_t offset, std::uint64_t vaddr, std::uint64_t filesz, std::uint64_t memsz, std::uint32_t flags) {
    Elf64ProgramHeaderDisk ph{};
    ph.type = type;
    ph.flags = flags;
    ph.offset = offset;
    ph.vaddr = vaddr;
    ph.paddr = vaddr;
    ph.filesz = filesz;
    ph.memsz = memsz;
    ph.align = 0x1000u;
    out.write(reinterpret_cast<const char*>(&ph), sizeof(ph));
}

void write_elf64_sh(std::ofstream& out, std::uint32_t type, std::uint64_t flags, std::uint64_t addr, std::uint64_t offset, std::uint64_t size) {
    Elf64SectionHeaderDisk sh{};
    sh.type = type;
    sh.flags = flags;
    sh.addr = addr;
    sh.offset = offset;
    sh.size = size;
    sh.addralign = 8;
    out.write(reinterpret_cast<const char*>(&sh), sizeof(sh));
}

}  // namespace

int main() {
    {
        std::ofstream out("bad_magic.elf", std::ios::binary | std::ios::trunc);
        write_elf32_header(out, 0, 0);
        out.seekp(0);
        out.write("NOPE", 4);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("bad_magic.elf", mem);
        assert(!res.ok);
    }

    {
        std::ofstream out("bad_segment.elf", std::ios::binary | std::ios::trunc);
        write_elf32_header(out, 1, 0);
        write_elf32_ph(out, 1, 0x200, 0x1000u, 0x200u, 0x100u, 0x5u);
        write_padding(out, 0x200);
        write_u32(out, 0x12345678u);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("bad_segment.elf", mem);
        assert(!res.ok);
    }

    {
        std::ofstream out("bss.elf", std::ios::binary | std::ios::trunc);
        write_elf32_header(out, 1, 0);
        write_elf32_ph(out, 1, 0x200, 0x1000u, 4u, 16u, riscv::MEM_READ | riscv::MEM_WRITE);
        write_padding(out, 0x200);
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

    {
        std::ofstream out("overlap.elf", std::ios::binary | std::ios::trunc);
        write_elf32_header(out, 2, 0);
        write_elf32_ph(out, 1, 0x200, 0x1000u, 4u, 0x100u, 0x6u);
        write_elf32_ph(out, 1, 0x204, 0x1080u, 4u, 0x100u, 0x6u);
        write_padding(out, 0x200);
        write_u32(out, 0x11111111u);
        write_u32(out, 0x22222222u);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("overlap.elf", mem);
        assert(!res.ok);
    }

    {
        std::ofstream out("section32.elf", std::ios::binary | std::ios::trunc);
        write_elf32_header(out, 0, 2, 0, 0x300);
        write_padding(out, 0x300);
        write_elf32_sh(out, 1, 0x6u, 0x1000u, 0x380u, 4u);
        write_elf32_sh(out, 8, 0x6u, 0x1100u, 0x000u, 16u);
        write_padding(out, 0x380);
        write_u32(out, 0x0a0b0c0du);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("section32.elf", mem);
        assert(res.ok);
        std::uint32_t value = 0;
        assert(mem.load32(0x1000u, value) && value == 0x0a0b0c0du);
        std::uint32_t zero = 0xffffffffu;
        assert(mem.load32(0x1100u, zero) && zero == 0u);
    }

    {
        std::ofstream out("elf64_ph.elf", std::ios::binary | std::ios::trunc);
        write_elf64_header(out, 1, 0);
        write_elf64_ph(out, 1, 0x200, 0x1000u, 8u, 16u, 0x6u);
        write_padding(out, 0x200);
        write_u64(out, 0x0102030405060708ull);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("elf64_ph.elf", mem);
        assert(res.ok);
        std::uint32_t lo = 0;
        assert(mem.load32(0x1000u, lo) && lo == 0x05060708u);
    }

    {
        std::ofstream out("elf64_sh.elf", std::ios::binary | std::ios::trunc);
        write_elf64_header(out, 0, 2, 0, 0x400);
        write_padding(out, 0x400);
        write_elf64_sh(out, 1, 0x6u, 0x1200u, 0x480u, 8u);
        write_elf64_sh(out, 8, 0x6u, 0x1300u, 0x000u, 16u);
        write_padding(out, 0x480);
        write_u64(out, 0x1122334455667788ull);
        out.close();
        riscv::Memory mem(0x4000u);
        auto res = riscv::load_elf("elf64_sh.elf", mem);
        assert(res.ok);
        std::uint32_t lo = 0;
        assert(mem.load32(0x1200u, lo) && lo == 0x55667788u);
        std::uint32_t zero = 0xffffffffu;
        assert(mem.load32(0x1300u, zero) && zero == 0u);
    }

    std::remove("bad_magic.elf");
    std::remove("bad_segment.elf");
    std::remove("bss.elf");
    std::remove("overlap.elf");
    std::remove("section32.elf");
    std::remove("elf64_ph.elf");
    std::remove("elf64_sh.elf");
    return 0;
}
