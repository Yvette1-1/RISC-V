#include "simulator.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

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

void build_elf(const std::string& path, const std::vector<std::uint32_t>& instructions) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);

    constexpr std::uint32_t text_vaddr = 0x1000u;
    constexpr std::uint32_t text_offset = 0x100;
    const std::uint32_t text_size = static_cast<std::uint32_t>(instructions.size() * sizeof(std::uint32_t));

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
    hdr.entry = text_vaddr;
    hdr.phoff = sizeof(Elf32HeaderDisk);
    hdr.ehsize = sizeof(Elf32HeaderDisk);
    hdr.phentsize = sizeof(Elf32ProgramHeaderDisk);
    hdr.phnum = 1;
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    Elf32ProgramHeaderDisk ph{};
    ph.type = 1;
    ph.offset = text_offset;
    ph.vaddr = text_vaddr;
    ph.paddr = text_vaddr;
    ph.filesz = text_size;
    ph.memsz = text_size;
    ph.flags = riscv::MEM_READ | riscv::MEM_EXEC;
    ph.align = 0x1000u;
    out.write(reinterpret_cast<const char*>(&ph), sizeof(ph));

    auto current = static_cast<std::uint32_t>(out.tellp());
    for (std::uint32_t i = current; i < text_offset; ++i) out.put(0);
    for (const auto inst : instructions) write_u32(out, inst);
}

}  // namespace

int main() {
    {
        const std::string path = "illegal_inst.elf";
        build_elf(path, {0xffffffffu});
        riscv::Simulator sim({});
        assert(sim.load_program(path));
        const auto stepped = sim.step();
        assert(!stepped);
        assert(sim.state() == riscv::SimulatorState::Trapped);
        assert(!sim.last_error().empty());
        std::remove(path.c_str());
    }

    {
        const std::string path = "ebreak_exit.elf";
        build_elf(path, {0x00100073u});
        riscv::Simulator sim({});
        assert(sim.load_program(path));
        const auto stepped = sim.step();
        assert(stepped);
        assert(sim.state() == riscv::SimulatorState::Halted);
        std::remove(path.c_str());
    }

    {
        const std::string path = "ecall_exit.elf";
        build_elf(path, {0x00000013u, 0x00000013u, 0x00000073u});
        riscv::Simulator sim({});
        assert(sim.load_program(path));
        assert(sim.set_reg(17, 93u));
        assert(sim.set_reg(10, 7u));
        sim.run();
        assert(sim.state() == riscv::SimulatorState::Exited);
        assert(sim.exit_code() == 7u);
        std::remove(path.c_str());
    }

    return 0;
}
