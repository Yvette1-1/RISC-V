#include "simulator.hpp"

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

std::filesystem::path write_exit_elf() {
    constexpr std::uint32_t kEntry = 0x1000u;
    constexpr std::uint32_t kSegmentOffset = 0x100u;

    std::vector<std::uint8_t> code;
    append_u32(code, 0x02a00513u); // addi a0, x0, 42
    append_u32(code, 0x05d00893u); // addi a7, x0, 93
    append_u32(code, 0x00000073u); // ecall

    std::vector<std::uint8_t> bytes;
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
    append_u32(bytes, static_cast<std::uint32_t>(code.size()));
    append_u32(bytes, static_cast<std::uint32_t>(code.size()));
    append_u32(bytes, 0x5u);
    append_u32(bytes, 0x1000u);

    bytes.resize(kSegmentOffset, 0);
    bytes.insert(bytes.end(), code.begin(), code.end());

    const auto path = std::filesystem::temp_directory_path() / "riscv_exit_syscall.elf";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}  // namespace

int main() {
    riscv::Simulator sim({});
    assert(sim.state() == riscv::SimulatorState::Idle);
    assert(sim.set_reg(1, 0x1234u));
    assert(sim.reg(1) == 0x1234u);

    const auto elf_path = write_exit_elf();
    assert(sim.load_program(elf_path.string()));
    assert(sim.state() == riscv::SimulatorState::Loaded);
    assert(sim.pc() == 0x1000u);
    assert(sim.reg(2) == 64u * 1024u * 1024u);
    assert(sim.brk() == 0x2000u);

    sim.run(16);
    assert(sim.state() == riscv::SimulatorState::Exited);
    assert(sim.exit_code() == 42u);

    std::filesystem::remove(elf_path);
    return 0;
}
