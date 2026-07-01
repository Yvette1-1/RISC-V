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

std::uint32_t encode_i(std::uint32_t imm, std::uint32_t rs1, std::uint32_t funct3, std::uint32_t rd, std::uint32_t opcode) {
    return ((imm & 0xfffu) << 20) | ((rs1 & 0x1fu) << 15) | ((funct3 & 0x7u) << 12) | ((rd & 0x1fu) << 7) |
           (opcode & 0x7fu);
}

std::uint32_t encode_r(std::uint32_t funct7, std::uint32_t rs2, std::uint32_t rs1, std::uint32_t funct3, std::uint32_t rd, std::uint32_t opcode) {
    return ((funct7 & 0x7fu) << 25) | ((rs2 & 0x1fu) << 20) | ((rs1 & 0x1fu) << 15) | ((funct3 & 0x7u) << 12) |
           ((rd & 0x1fu) << 7) | (opcode & 0x7fu);
}

std::uint32_t encode_b(std::int32_t imm, std::uint32_t rs2, std::uint32_t rs1, std::uint32_t funct3, std::uint32_t opcode) {
    const std::uint32_t uimm = static_cast<std::uint32_t>(imm);
    return (((uimm >> 12) & 0x1u) << 31) | (((uimm >> 5) & 0x3fu) << 25) | ((rs2 & 0x1fu) << 20) |
           ((rs1 & 0x1fu) << 15) | ((funct3 & 0x7u) << 12) | (((uimm >> 1) & 0xfu) << 8) |
           (((uimm >> 11) & 0x1u) << 7) | (opcode & 0x7fu);
}

void build_elf(const std::string& path, const std::vector<std::uint32_t>& instructions) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out && "failed to create loop ELF");

    constexpr std::uint32_t text_vaddr = 0x1000u;
    constexpr std::uint32_t text_offset = 0x100;
    const std::uint32_t text_size = static_cast<std::uint32_t>(instructions.size() * sizeof(std::uint32_t));

    Elf32HeaderDisk header{};
    header.ident[0] = 0x7f;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 1;
    header.ident[5] = 1;
    header.ident[6] = 1;
    header.type = 2;
    header.machine = 243;
    header.version = 1;
    header.entry = text_vaddr;
    header.phoff = sizeof(Elf32HeaderDisk);
    header.ehsize = sizeof(Elf32HeaderDisk);
    header.phentsize = sizeof(Elf32ProgramHeaderDisk);
    header.phnum = 1;
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    Elf32ProgramHeaderDisk ph{};
    ph.type = 1;
    ph.offset = text_offset;
    ph.vaddr = text_vaddr;
    ph.paddr = text_vaddr;
    ph.filesz = text_size;
    ph.memsz = text_size;
    ph.flags = 0x5u;
    ph.align = 0x1000u;
    out.write(reinterpret_cast<const char*>(&ph), sizeof(ph));

    auto current = static_cast<std::uint32_t>(out.tellp());
    for (std::uint32_t i = current; i < text_offset; ++i) {
        out.put(0);
    }
    for (const auto inst : instructions) {
        write_u32(out, inst);
    }
}

}  // namespace

int main() {
    const std::string path = "loop_test.elf";
    const std::vector<std::uint32_t> instructions = {
        encode_i(1, 0, 0x0, 1, 0x13),          // x1 = 1
        encode_i(11, 0, 0x0, 2, 0x13),         // x2 = 11
        encode_i(0, 0, 0x0, 3, 0x13),          // x3 = 0
        encode_r(0x00, 1, 3, 0x0, 3, 0x33),    // loop: x3 += x1
        encode_i(1, 1, 0x0, 1, 0x13),          // x1++
        encode_b(-12, 1, 2, 0x4, 0x63),        // while (x1 < x2)
        encode_i(0, 3, 0x0, 10, 0x13),         // a0 = x3
        encode_i(93, 0, 0x0, 17, 0x13),        // a7 = exit
        0x00000073u,
    };
    build_elf(path, instructions);

    riscv::Simulator sim({});
    assert(sim.load_program(path));
    sim.run();
    assert(sim.state() == riscv::SimulatorState::Exited);
    assert(sim.exit_code() == 55u);
    assert(sim.reg(3) == 55u);

    std::remove(path.c_str());
    return 0;
}
