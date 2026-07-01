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

void build_elf(const std::string& path, const std::vector<std::uint32_t>& instructions, const std::vector<std::uint32_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out && "failed to create array ELF");

    constexpr std::uint32_t text_vaddr = 0x1000u;
    constexpr std::uint32_t data_vaddr = 0x3000u;
    constexpr std::uint32_t text_offset = 0x100;
    constexpr std::uint32_t data_offset = 0x200;
    const std::uint32_t text_size = static_cast<std::uint32_t>(instructions.size() * sizeof(std::uint32_t));
    const std::uint32_t data_size = static_cast<std::uint32_t>(data.size() * sizeof(std::uint32_t));

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
    header.phnum = 2;
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    Elf32ProgramHeaderDisk text_ph{};
    text_ph.type = 1;
    text_ph.offset = text_offset;
    text_ph.vaddr = text_vaddr;
    text_ph.paddr = text_vaddr;
    text_ph.filesz = text_size;
    text_ph.memsz = text_size;
    text_ph.flags = 0x5u;
    text_ph.align = 0x1000u;
    out.write(reinterpret_cast<const char*>(&text_ph), sizeof(text_ph));

    Elf32ProgramHeaderDisk data_ph{};
    data_ph.type = 1;
    data_ph.offset = data_offset;
    data_ph.vaddr = data_vaddr;
    data_ph.paddr = data_vaddr;
    data_ph.filesz = data_size;
    data_ph.memsz = data_size;
    data_ph.flags = 0x6u;
    data_ph.align = 0x1000u;
    out.write(reinterpret_cast<const char*>(&data_ph), sizeof(data_ph));

    auto current = static_cast<std::uint32_t>(out.tellp());
    for (std::uint32_t i = current; i < text_offset; ++i) {
        out.put(0);
    }
    for (const auto inst : instructions) {
        write_u32(out, inst);
    }
    current = static_cast<std::uint32_t>(out.tellp());
    for (std::uint32_t i = current; i < data_offset; ++i) {
        out.put(0);
    }
    for (const auto value : data) {
        write_u32(out, value);
    }
}

}  // namespace

int main() {
    const std::string path = "array_test.elf";
    const std::vector<std::uint32_t> data = {1u, 2u, 3u, 4u, 5u};
    const std::vector<std::uint32_t> instructions = {
        encode_i(5, 0, 0x0, 1, 0x13),          // counter = 5
        encode_i(0, 0, 0x0, 2, 0x13),          // sum = 0
        encode_u(0x3000u, 4, 0x37),           // base address
        encode_i(0, 4, 0x0, 4, 0x13),         // base stays in x4
        encode_i(0, 4, 0x2, 5, 0x03),         // loop: load element
        encode_r(0x00, 5, 2, 0x0, 2, 0x33),   // sum += element
        encode_i(4, 4, 0x0, 4, 0x13),         // advance pointer
        encode_i(-1, 1, 0x0, 1, 0x13),        // counter--
        encode_b(-16, 1, 0, 0x1, 0x63),       // if counter != 0, loop
        encode_i(0, 2, 0x0, 10, 0x13),        // a0 = sum
        encode_i(93, 0, 0x0, 17, 0x13),       // exit
        0x00000073u,
    };
    build_elf(path, instructions, data);

    riscv::Simulator sim({});
    assert(sim.load_program(path));
    sim.run();
    assert(sim.state() == riscv::SimulatorState::Exited);
    assert(sim.exit_code() == 15u);
    assert(sim.reg(2) == 15u);

    std::remove(path.c_str());
    return 0;
}
