#include "simulator.hpp"

#include "elf_loader.hpp"
#include "fpu.hpp"
#include "riscv_isa.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

namespace riscv {

namespace {
constexpr std::size_t kDefaultMemorySize = 64u * 1024u * 1024u;
constexpr std::uint32_t kStackAlignment = 16u;
constexpr std::uint32_t kHeapAlignment = 0x1000u;
constexpr std::uint32_t kSysWrite = 64u;
constexpr std::uint32_t kSysRead = 63u;
constexpr std::uint32_t kSysExit = 93u;
constexpr std::uint32_t kSysBrk = 214u;

std::uint32_t align_up(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

std::int32_t make_imm_i(std::uint32_t inst) {
    return sign_extend(get_bits(inst, 31, 20), 12);
}

std::int32_t make_imm_u(std::uint32_t inst) {
    return static_cast<std::int32_t>(inst & 0xfffff000u);
}

std::int32_t make_imm_s(std::uint32_t inst) {
    return sign_extend((get_bits(inst, 31, 25) << 5) | get_bits(inst, 11, 7), 12);
}

std::int32_t make_imm_b(std::uint32_t inst) {
    auto imm = 0u;
    imm |= get_bits(inst, 31, 31) << 12;
    imm |= get_bits(inst, 7, 7) << 11;
    imm |= get_bits(inst, 30, 25) << 5;
    imm |= get_bits(inst, 11, 8) << 1;
    return sign_extend(imm, 13);
}

std::int32_t make_imm_j(std::uint32_t inst) {
    auto imm = 0u;
    imm |= get_bits(inst, 31, 31) << 20;
    imm |= get_bits(inst, 19, 12) << 12;
    imm |= get_bits(inst, 20, 20) << 11;
    imm |= get_bits(inst, 30, 21) << 1;
    return sign_extend(imm, 21);
}
}  // namespace

Simulator::Simulator(SimulatorConfig config)
    : config_(std::move(config)), memory_(config_.memory_size ? config_.memory_size : kDefaultMemorySize) {}

bool Simulator::load_program(const std::string& path) {
    config_.program_path = path;
    memory_.clear();
    breakpoints_.clear();
    exit_code_ = 0;
    brk_ = 0;
    heap_base_ = 0;
    tohost_ = 0;
    fromhost_ = 0;
    csr_mstatus_ = 0;
    csr_mtvec_ = 0;
    csr_mepc_ = 0;
    csr_mcause_ = 0;
    csr_mtval_ = 0;
    pipeline_ = {};
    pipeline_stats_ = {};
    last_error_.clear();
    for (auto& reg : regs_) {
        reg = 0;
    }

    const auto result = load_elf(path, memory_);
    if (!result.ok) {
        state_ = SimulatorState::Error;
        last_error_ = result.error;
        return false;
    }

    if (config_.stack_size == 0 || config_.stack_top > memory_.size() || config_.stack_size > config_.stack_top) {
        state_ = SimulatorState::Error;
        last_error_ = "invalid stack configuration";
        return false;
    }
    const auto stack_base = config_.stack_top - config_.stack_size;
    if (!memory_.map_region({stack_base, config_.stack_size, MEM_READ | MEM_WRITE, "stack"})) {
        state_ = SimulatorState::Error;
        last_error_ = "failed to map initial stack";
        return false;
    }

    std::uint32_t max_segment_end = 0;
    for (const auto& segment : result.segments) {
        max_segment_end = std::max(max_segment_end, segment.vaddr + segment.mem_size);
    }
    heap_base_ = align_up(max_segment_end, kHeapAlignment);
    brk_ = heap_base_;
    if (heap_base_ >= stack_base) {
        state_ = SimulatorState::Error;
        last_error_ = "ELF image leaves no room for heap and stack";
        return false;
    }

    pc_ = result.entry_point;
    regs_[2] = config_.stack_top & ~(kStackAlignment - 1u);
    tohost_ = result.tohost;
    fromhost_ = result.fromhost;
    state_ = SimulatorState::Loaded;
    return true;
}

bool Simulator::reset() {
    pc_ = 0;
    state_ = SimulatorState::Idle;
    last_error_.clear();
    breakpoints_.clear();
    exit_code_ = 0;
    brk_ = 0;
    heap_base_ = 0;
    tohost_ = 0;
    fromhost_ = 0;
    csr_mstatus_ = 0;
    csr_mtvec_ = 0;
    csr_mepc_ = 0;
    csr_mcause_ = 0;
    csr_mtval_ = 0;
    pipeline_ = {};
    pipeline_stats_ = {};
    for (auto& reg : regs_) {
        reg = 0;
    }
    for (auto& freg : fregs_) {
        freg = 0;
    }
    return true;
}

Simulator::ExecuteResult Simulator::handle_syscall() {
    last_error_.clear();
    const auto sysno = regs_[17];
    switch (sysno) {
    case kSysWrite: {
        const auto fd = regs_[10];
        const auto addr = regs_[11];
        const auto len = regs_[12];
        if (fd != 1 && fd != 2) {
            regs_[10] = static_cast<std::uint32_t>(-1);
            return ExecuteResult::Advanced;
        }
        std::string buffer(len, '\0');
        if (len != 0 && !memory_.load_bytes(addr, buffer.data(), len)) {
            regs_[10] = static_cast<std::uint32_t>(-1);
            return ExecuteResult::Advanced;
        }
        std::cout.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::cout.flush();
        regs_[10] = len;
        return ExecuteResult::Advanced;
    }
    case kSysRead: {
        const auto fd = regs_[10];
        const auto addr = regs_[11];
        const auto len = regs_[12];
        if (fd != 0) {
            regs_[10] = static_cast<std::uint32_t>(-1);
            return ExecuteResult::Advanced;
        }
        if (len == 0) {
            regs_[10] = 0;
            return ExecuteResult::Advanced;
        }
        std::string buffer(len, '\0');
        std::cin.read(buffer.data(), static_cast<std::streamsize>(len));
        const auto got = static_cast<std::size_t>(std::cin.gcount());
        if (got == 0) {
            regs_[10] = 0;
            return ExecuteResult::Advanced;
        }
        if (!memory_.store_bytes(addr, buffer.data(), got)) {
            regs_[10] = static_cast<std::uint32_t>(-1);
            return ExecuteResult::Advanced;
        }
        regs_[10] = static_cast<std::uint32_t>(got);
        return ExecuteResult::Advanced;
    }
    case kSysExit:
        exit_code_ = regs_[10];
        state_ = SimulatorState::Exited;
        return ExecuteResult::Exited;
    case kSysBrk: {
        const auto requested = regs_[10];
        const auto stack_base = config_.stack_top - config_.stack_size;
        const auto heap_limit = stack_base > kHeapAlignment ? stack_base - kHeapAlignment : stack_base;
        if (heap_base_ == 0) {
            heap_base_ = align_up(brk_, kHeapAlignment);
            brk_ = heap_base_;
        }
        if (requested == 0) {
            regs_[10] = brk_;
            return ExecuteResult::Advanced;
        }
        if (requested < heap_base_ || requested > heap_limit || requested < brk_) {
            regs_[10] = brk_;
            return ExecuteResult::Advanced;
        }
        if (requested > brk_) {
            const auto mapped_base = brk_;
            const auto mapped_size = requested - brk_;
            if (!memory_.map_region({mapped_base, mapped_size, MEM_READ | MEM_WRITE, "heap"})) {
                regs_[10] = brk_;
                return ExecuteResult::Advanced;
            }
            if (!memory_.fill(mapped_base, 0, mapped_size)) {
                regs_[10] = brk_;
                return ExecuteResult::Advanced;
            }
        }
        brk_ = requested;
        regs_[10] = brk_;
        return ExecuteResult::Advanced;
    }
    default:
        last_error_ = "unsupported syscall";
        state_ = SimulatorState::Trapped;
        return ExecuteResult::Error;
    }
}

Simulator::ExecuteResult Simulator::execute_one() {
    if (state_ == SimulatorState::Idle || state_ == SimulatorState::Error || state_ == SimulatorState::Exited) {
        last_error_ = "simulator is not ready";
        return ExecuteResult::Error;
    }
    if (state_ == SimulatorState::Paused) {
        state_ = SimulatorState::Loaded;
    }

    std::uint32_t inst = 0;
    if (!memory_.load32(pc_, inst)) {
        last_error_ = "instruction fetch failed";
        state_ = SimulatorState::Trapped;
        return ExecuteResult::Error;
    }

    const auto decoded = decode(inst);
    const auto current_pc = pc_;
    std::uint32_t next_pc = pc_ + 4;

    auto read_reg = [this](std::size_t idx) -> std::uint32_t {
        return idx == 0 ? 0u : regs_[idx];
    };

    auto write_reg = [this](std::size_t idx, std::uint32_t value) {
        if (idx != 0 && idx < 32) {
            regs_[idx] = value;
        }
    };

    switch (decoded.opcode) {
    case 0x37:
        write_reg(decoded.rd, static_cast<std::uint32_t>(make_imm_u(inst)));
        break;
    case 0x17:
        write_reg(decoded.rd, pc_ + static_cast<std::uint32_t>(make_imm_u(inst)));
        break;
    case 0x13: {
        const auto lhs = read_reg(decoded.rs1);
        const auto imm_signed = make_imm_i(inst);
        const auto imm_unsigned = static_cast<std::uint32_t>(imm_signed);
        switch (decoded.funct3) {
        case 0x0: write_reg(decoded.rd, lhs + imm_unsigned); break;
        case 0x2: write_reg(decoded.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(lhs) < imm_signed)); break;
        case 0x3: write_reg(decoded.rd, lhs < imm_unsigned ? 1u : 0u); break;
        case 0x4: write_reg(decoded.rd, lhs ^ imm_unsigned); break;
        case 0x6: write_reg(decoded.rd, lhs | imm_unsigned); break;
        case 0x7: write_reg(decoded.rd, lhs & imm_unsigned); break;
        case 0x1: write_reg(decoded.rd, lhs << get_bits(inst, 24, 20)); break;
        case 0x5:
            if (decoded.funct7 == 0x00) {
                write_reg(decoded.rd, lhs >> get_bits(inst, 24, 20));
            } else if (decoded.funct7 == 0x20) {
                write_reg(decoded.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(lhs) >> get_bits(inst, 24, 20)));
            } else {
                last_error_ = "unknown OP-IMM shift";
                state_ = SimulatorState::Trapped;
                return ExecuteResult::Error;
            }
            break;
        default:
            last_error_ = "unsupported OP-IMM";
            state_ = SimulatorState::Trapped;
            return ExecuteResult::Error;
        }
        break;
    }
    case 0x33: {
        const auto lhs = read_reg(decoded.rs1);
        const auto rhs = read_reg(decoded.rs2);
        switch (decoded.funct3) {
        case 0x0:
            if (decoded.funct7 == 0x00) write_reg(decoded.rd, lhs + rhs);
            else if (decoded.funct7 == 0x20) write_reg(decoded.rd, lhs - rhs);
            else if (decoded.funct7 == 0x01) write_reg(decoded.rd, static_cast<std::uint32_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(lhs)) * static_cast<std::int64_t>(static_cast<std::int32_t>(rhs))));
            else { last_error_ = "unsupported OP"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; }
            break;
        default:
            if (decoded.funct7 == 0x01) {
                switch (decoded.funct3) {
                case 0x0: write_reg(decoded.rd, static_cast<std::uint32_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(lhs)) * static_cast<std::int64_t>(static_cast<std::int32_t>(rhs)))); break;
                case 0x1: write_reg(decoded.rd, static_cast<std::uint32_t>((static_cast<std::int64_t>(static_cast<std::int32_t>(lhs)) * static_cast<std::int64_t>(static_cast<std::int32_t>(rhs))) >> 32)); break;
                case 0x2: write_reg(decoded.rd, static_cast<std::uint32_t>((static_cast<std::int64_t>(static_cast<std::int32_t>(lhs)) * static_cast<std::uint64_t>(rhs)) >> 32)); break;
                case 0x3: write_reg(decoded.rd, static_cast<std::uint32_t>((static_cast<std::uint64_t>(lhs) * static_cast<std::uint64_t>(rhs)) >> 32)); break;
                case 0x4: write_reg(decoded.rd, rhs == 0 ? 0xffffffffu : static_cast<std::uint32_t>(static_cast<std::int32_t>(lhs) / static_cast<std::int32_t>(rhs))); break;
                case 0x5: write_reg(decoded.rd, rhs == 0 ? 0xffffffffu : lhs / rhs); break;
                case 0x6: write_reg(decoded.rd, rhs == 0 ? lhs : static_cast<std::uint32_t>(static_cast<std::int32_t>(lhs) % static_cast<std::int32_t>(rhs))); break;
                case 0x7: write_reg(decoded.rd, rhs == 0 ? lhs : lhs % rhs); break;
                default:
                    last_error_ = "unsupported M extension";
                    state_ = SimulatorState::Trapped;
                    return ExecuteResult::Error;
                }
            } else {
                last_error_ = "unsupported OP";
                state_ = SimulatorState::Trapped;
                return ExecuteResult::Error;
            }
            break;
        }
        break;
    }
    case 0x53: {
        const auto lhs = fregs_[decoded.rs1];
        const auto rhs = fregs_[decoded.rs2];
        switch (decoded.funct7) {
        case 0x00: fregs_[decoded.rd] = f32_add(lhs, rhs); break;
        case 0x04: fregs_[decoded.rd] = f32_sub(lhs, rhs); break;
        case 0x08: fregs_[decoded.rd] = f32_mul(lhs, rhs); break;
        case 0x0c: fregs_[decoded.rd] = f32_div(lhs, rhs); break;
        case 0x2c: fregs_[decoded.rd] = f32_sqrt(lhs); break;
        case 0x14: fregs_[decoded.rd] = f32_fmin(lhs, rhs); break;
        case 0x15: fregs_[decoded.rd] = f32_fmax(lhs, rhs); break;
        default:
            last_error_ = "unsupported F extension";
            state_ = SimulatorState::Trapped;
            return ExecuteResult::Error;
        }
        break;
    }
    case 0x03: {
        const auto addr = read_reg(decoded.rs1) + static_cast<std::uint32_t>(make_imm_i(inst));
        switch (decoded.funct3) {
        case 0x0: {
            std::uint8_t v = 0;
            if (!memory_.load8(addr, v)) { last_error_ = "lb failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; }
            write_reg(decoded.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(v))));
            break;
        }
        case 0x1: {
            std::uint16_t v = 0;
            if (!memory_.load16(addr, v)) { last_error_ = "lh failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; }
            write_reg(decoded.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))));
            break;
        }
        case 0x2: {
            std::uint32_t v = 0;
            if (!memory_.load32(addr, v)) { last_error_ = "lw failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; }
            write_reg(decoded.rd, v);
            break;
        }
        case 0x4: {
            std::uint8_t v = 0;
            if (!memory_.load8(addr, v)) { last_error_ = "lbu failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; }
            write_reg(decoded.rd, v);
            break;
        }
        case 0x5: {
            std::uint16_t v = 0;
            if (!memory_.load16(addr, v)) { last_error_ = "lhu failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; }
            write_reg(decoded.rd, v);
            break;
        }
        default:
            last_error_ = "unsupported load";
            state_ = SimulatorState::Trapped;
            return ExecuteResult::Error;
        }
        break;
    }
    case 0x23: {
        const auto addr = read_reg(decoded.rs1) + make_imm_s(inst);
        const auto value = read_reg(decoded.rs2);
        switch (decoded.funct3) {
        case 0x0: if (!memory_.store8(addr, static_cast<std::uint8_t>(value))) { last_error_ = "sb failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; } break;
        case 0x1: if (!memory_.store16(addr, static_cast<std::uint16_t>(value))) { last_error_ = "sh failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; } break;
        case 0x2: if (!memory_.store32(addr, value)) { last_error_ = "sw failed"; state_ = SimulatorState::Trapped; return ExecuteResult::Error; } break;
        default:
            last_error_ = "unsupported store";
            state_ = SimulatorState::Trapped;
            return ExecuteResult::Error;
        }
        if (tohost_ != 0 && addr == tohost_) {
            exit_code_ = value;
            state_ = SimulatorState::Exited;
            return ExecuteResult::Exited;
        }
        if (fromhost_ != 0 && addr == fromhost_) {
            if (value != 0) {
                const std::uint32_t zero = 0;
                (void)memory_.store32(fromhost_, zero);
            }
        }
        break;
    }
    case 0x6f: {
        const auto target = pc_ + static_cast<std::uint32_t>(make_imm_j(inst));
        write_reg(decoded.rd, next_pc);
        next_pc = target;
        break;
    }
    case 0x67: {
        write_reg(decoded.rd, next_pc);
        next_pc = (read_reg(decoded.rs1) + static_cast<std::uint32_t>(make_imm_i(inst))) & ~1u;
        break;
    }
    case 0x63: {
        const auto lhs = read_reg(decoded.rs1);
        const auto rhs = read_reg(decoded.rs2);
        const auto target = pc_ + static_cast<std::uint32_t>(make_imm_b(inst));
        bool take = false;
        switch (decoded.funct3) {
        case 0x0: take = lhs == rhs; break;
        case 0x1: take = lhs != rhs; break;
        case 0x4: take = static_cast<std::int32_t>(lhs) < static_cast<std::int32_t>(rhs); break;
        case 0x5: take = static_cast<std::int32_t>(lhs) >= static_cast<std::int32_t>(rhs); break;
        case 0x6: take = lhs < rhs; break;
        case 0x7: take = lhs >= rhs; break;
        default:
            last_error_ = "unsupported branch";
            state_ = SimulatorState::Trapped;
            return ExecuteResult::Error;
        }
        if (take) {
            next_pc = target;
        }
        break;
    }
    case 0x73: {
        if (inst == 0x00100073u) {
            state_ = SimulatorState::Halted;
            return ExecuteResult::Halted;
        }
        if (inst == 0x00000073u) {
            const auto syscall_result = handle_syscall();
            if (syscall_result == ExecuteResult::Error) {
                return syscall_result;
            }
            pc_ += 4;
            regs_[0] = 0;
            state_ = (syscall_result == ExecuteResult::Exited) ? SimulatorState::Exited : SimulatorState::Loaded;
            return syscall_result;
        }
        if (inst == 0x30200073u) { // mret
            pc_ = csr_mepc_;
            regs_[0] = 0;
            state_ = SimulatorState::Loaded;
            return ExecuteResult::Advanced;
        }

        const auto csr_addr = get_bits(inst, 31, 20);
        auto read_csr = [this](std::uint32_t csr) -> std::uint32_t {
            switch (csr) {
            case 0x300: return csr_mstatus_;
            case 0x305: return csr_mtvec_;
            case 0x341: return csr_mepc_;
            case 0x342: return csr_mcause_;
            case 0x343: return csr_mtval_;
            default: return 0u;
            }
        };
        auto write_csr = [this](std::uint32_t csr, std::uint32_t value) {
            switch (csr) {
            case 0x300: csr_mstatus_ = value; break;
            case 0x305: csr_mtvec_ = value; break;
            case 0x341: csr_mepc_ = value; break;
            case 0x342: csr_mcause_ = value; break;
            case 0x343: csr_mtval_ = value; break;
            default: break;
            }
        };

        switch (decoded.funct3) {
        case 0x1: { // csrrw
            const auto old = read_csr(csr_addr);
            write_csr(csr_addr, read_reg(decoded.rs1));
            write_reg(decoded.rd, old);
            break;
        }
        case 0x2: { // csrrs
            const auto old = read_csr(csr_addr);
            const auto rs1 = read_reg(decoded.rs1);
            if (decoded.rs1 != 0) write_csr(csr_addr, old | rs1);
            write_reg(decoded.rd, old);
            break;
        }
        case 0x3: { // csrrc
            const auto old = read_csr(csr_addr);
            const auto rs1 = read_reg(decoded.rs1);
            if (decoded.rs1 != 0) write_csr(csr_addr, old & ~rs1);
            write_reg(decoded.rd, old);
            break;
        }
        case 0x5: { // csrrwi
            const auto old = read_csr(csr_addr);
            write_csr(csr_addr, decoded.rs1);
            write_reg(decoded.rd, old);
            break;
        }
        case 0x6: { // csrrsi
            const auto old = read_csr(csr_addr);
            if (decoded.rs1 != 0) write_csr(csr_addr, old | decoded.rs1);
            write_reg(decoded.rd, old);
            break;
        }
        case 0x7: { // csrrci
            const auto old = read_csr(csr_addr);
            if (decoded.rs1 != 0) write_csr(csr_addr, old & ~decoded.rs1);
            write_reg(decoded.rd, old);
            break;
        }
        default:
            last_error_ = "unsupported system instruction";
            state_ = SimulatorState::Trapped;
            csr_mepc_ = pc_;
            csr_mcause_ = 2u;
            csr_mtval_ = inst;
            if (csr_mtvec_ != 0) {
                pc_ = csr_mtvec_;
                state_ = SimulatorState::Loaded;
                return ExecuteResult::Advanced;
            }
            return ExecuteResult::Error;
        }
        break;
    }
    default:
        last_error_ = "unsupported opcode";
        state_ = SimulatorState::Trapped;
        return ExecuteResult::Error;
    }

    const bool control_change = next_pc != current_pc + 4;
    pc_ = next_pc;
    regs_[0] = 0;
    update_pipeline(current_pc, control_change);
    if (state_ != SimulatorState::Trapped) {
        state_ = SimulatorState::Loaded;
    }
    return ExecuteResult::Advanced;
}

bool Simulator::step() {
    const auto result = execute_one();
    return result == ExecuteResult::Advanced || result == ExecuteResult::Halted || result == ExecuteResult::Exited;
}

void Simulator::run(std::size_t max_steps) {
    state_ = SimulatorState::Running;
    std::size_t executed = 0;
    while (state_ == SimulatorState::Running || state_ == SimulatorState::Loaded || state_ == SimulatorState::Paused) {
        if (has_breakpoint(pc_)) {
            state_ = SimulatorState::Paused;
            break;
        }
        const auto result = execute_one();
        ++executed;
        if (result == ExecuteResult::Halted || result == ExecuteResult::Error || result == ExecuteResult::Exited) {
            break;
        }
        if (max_steps != 0 && executed >= max_steps) {
            state_ = SimulatorState::Paused;
            break;
        }
        if (state_ == SimulatorState::Paused) {
            break;
        }
    }
    if (state_ == SimulatorState::Running) {
        state_ = SimulatorState::Loaded;
    }
}

SimulatorState Simulator::state() const noexcept {
    return state_;
}

SimulatorStatus Simulator::status() const {
    return SimulatorStatus{state_, pc_, exit_code_, last_error_};
}

std::uint32_t Simulator::pc() const noexcept {
    return pc_;
}

std::uint32_t Simulator::exit_code() const noexcept {
    return exit_code_;
}

std::uint32_t Simulator::brk() const noexcept {
    return brk_;
}

const std::uint32_t* Simulator::regs() const noexcept {
    return regs_;
}

std::uint32_t Simulator::reg(std::size_t index) const {
    return index < 32 ? regs_[index] : 0;
}

bool Simulator::set_reg(std::size_t index, std::uint32_t value) {
    if (index == 0 || index >= 32) {
        return false;
    }
    regs_[index] = value;
    return true;
}

PipelineStats Simulator::pipeline_stats() const noexcept {
    return pipeline_stats_;
}

const PipelineSnapshot& Simulator::pipeline_snapshot() const noexcept {
    return pipeline_;
}

Memory& Simulator::memory() noexcept {
    return memory_;
}

const Memory& Simulator::memory() const noexcept {
    return memory_;
}

void Simulator::set_trace_enabled(bool enabled) {
    config_.enable_trace = enabled;
}

bool Simulator::trace_enabled() const noexcept {
    return config_.enable_trace;
}

bool Simulator::add_breakpoint(std::uint32_t addr) {
    if (has_breakpoint(addr)) {
        return false;
    }
    breakpoints_.push_back(addr);
    return true;
}

bool Simulator::remove_breakpoint(std::uint32_t addr) {
    const auto it = std::remove(breakpoints_.begin(), breakpoints_.end(), addr);
    const bool removed = it != breakpoints_.end();
    breakpoints_.erase(it, breakpoints_.end());
    return removed;
}

bool Simulator::has_breakpoint(std::uint32_t addr) const {
    return std::find(breakpoints_.begin(), breakpoints_.end(), addr) != breakpoints_.end();
}

const std::vector<std::uint32_t>& Simulator::breakpoints() const noexcept {
    return breakpoints_;
}

const std::string& Simulator::program_path() const noexcept {
    return config_.program_path;
}

const std::string& Simulator::last_error() const noexcept {
    return last_error_;
}

void Simulator::update_pipeline(std::uint32_t retired_pc, bool control_change) {
    pipeline_.cycle += 1;
    pipeline_stats_.cycles += 1;
    pipeline_stats_.instructions += 1;
    pipeline_.wb_pc = pipeline_.mem_pc;
    pipeline_.wb_valid = pipeline_.mem_valid;
    pipeline_.mem_pc = pipeline_.ex_pc;
    pipeline_.mem_valid = pipeline_.ex_valid;
    pipeline_.ex_pc = pipeline_.id_pc;
    pipeline_.ex_valid = pipeline_.id_valid;
    pipeline_.id_pc = pipeline_.if_pc;
    pipeline_.id_valid = pipeline_.if_valid;
    pipeline_.if_pc = retired_pc + 4u;
    pipeline_.if_valid = state_ != SimulatorState::Trapped && state_ != SimulatorState::Exited;
    if (control_change) {
        pipeline_stats_.flushes += 1;
        pipeline_stats_.stalls += 1;
        pipeline_.if_valid = false;
        pipeline_.id_valid = false;
    }
    if (pipeline_stats_.instructions != 0) {
        pipeline_stats_.cpi = static_cast<double>(pipeline_stats_.cycles) / static_cast<double>(pipeline_stats_.instructions);
    }
}

}  // namespace riscv
