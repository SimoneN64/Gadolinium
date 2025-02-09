#pragma once
#include <BaseCPU.hpp>
#include <Mem.hpp>
#include <vector>
#include <xbyak.h>
#include <jit/helpers.hpp>
#include <capstone/capstone.h>

namespace n64 {
struct Core;

static constexpr u64 kAddressSpaceSize = 0x8000'0000;
static constexpr u8 kUpperShift = 8;
static constexpr u8 kLowerMask = 0xff;
static constexpr u32 kUpperSize = kAddressSpaceSize >> kUpperShift; // 0x800000
static constexpr u32 kLowerSize = 0x100; // 0x80
static constexpr u32 kCodeCacheSize = 32_mb;
static constexpr u32 kCodeCacheAllocSize = kCodeCacheSize + 4_kb;
#define REG(acc, x) code.acc[code.rbp + (reinterpret_cast<uintptr_t>(&regs.x) - (uintptr_t)this)]

#ifdef __aarch64__
struct JIT : BaseCPU {};
#else
struct JIT : BaseCPU {
  explicit JIT(ParallelRDP &);
  ~JIT() override = default;
  int Step() override;

  void Reset() override {
    regs.Reset();
    mem.Reset();
    code.reset();
    blockCache = {};
    blockCache.resize(kUpperSize);
  }

  void InvalidateBlock(u32);

  Mem &GetMem() override { return mem; }

  Registers &GetRegs() override { return regs; }

  [[nodiscard]] Disassembler::DisassemblyResult Disassemble(u32, u32) const override { return {}; }

private:
  Xbyak::CodeGenerator code{kCodeCacheAllocSize};
  Registers regs;
  Mem mem;
  u64 cop2Latch{};
  u64 blockOldPC = 0, blockPC = 0, blockNextPC = 0;
  friend struct Cop1;
  friend struct Registers;
  using BlockFn = int (*)();
  std::vector<std::vector<BlockFn>> blockCache;
  Xbyak::Label branch_likely_not_taken;
  csh disassemblerMips, disassemblerX86;

  template <typename T>
  Xbyak::Address GPR(const size_t index) const {
    if constexpr (sizeof(T) == 1) {
      return code.byte[code.rbp + (reinterpret_cast<uintptr_t>(&regs.gpr[index]) - reinterpret_cast<uintptr_t>(this))];
    } else if constexpr (sizeof(T) == 2) {
      return code.word[code.rbp + (reinterpret_cast<uintptr_t>(&regs.gpr[index]) - reinterpret_cast<uintptr_t>(this))];
    } else if constexpr (sizeof(T) == 4) {
      return code.dword[code.rbp + (reinterpret_cast<uintptr_t>(&regs.gpr[index]) - reinterpret_cast<uintptr_t>(this))];
    } else if constexpr (sizeof(T) == 8) {
      return code.qword[code.rbp + (reinterpret_cast<uintptr_t>(&regs.gpr[index]) - reinterpret_cast<uintptr_t>(this))];
    }

    Util::panic("[JIT]: Invalid register addressing");
    // never actually hit, but just to silence the warning "not all control paths return a value"
    return Xbyak::Address{0};
  }

  // Thanks to https://github.com/grumpycoders/pcsx-redux
  // Load a pointer to the JIT object in "reg"
  template <typename T>
  void emitMemberFunctionCall(T func, void *thisObject) {
    uintptr_t functionPtr;
    auto thisPtr = reinterpret_cast<uintptr_t>(thisObject);

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    static_assert(sizeof(T) == 8, "[x64 JIT] Invalid size for member function pointer");
    std::memcpy(&functionPtr, &func, sizeof(T));
#else
    static_assert(sizeof(T) == 16, "[x64 JIT] Invalid size for member function pointer");
    uintptr_t arr[2];
    std::memcpy(arr, &func, sizeof(T));
    // First 8 bytes correspond to the actual pointer to the function
    functionPtr = reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(arr[0]));
    // Next 8 bytes correspond to the "this" pointer adjustment
    thisPtr += arr[1];
#endif

    code.mov(code.ARG1, thisPtr);
    code.mov(code.rax, functionPtr);
    code.sub(code.rsp, 8);
    code.call(code.rax);
    code.add(code.rsp, 8);
  }

  void SkipSlot();
  void SkipSlotConstant();
  void BranchNotTaken();
  void BranchTaken(s64 offs);
  void BranchTaken(const Xbyak::Reg64 &offs);
  void BranchAbsTaken(s64 addr);
  void BranchAbsTaken(const Xbyak::Reg64 &addr);

#define check_address_error(mask, vaddr)                                                                               \
  (((!regs.cop0.is64BitAddressing) && (s32)(vaddr) != (vaddr)) || (((vaddr) & (mask)) != 0))

  [[nodiscard]] bool ShouldServiceInterrupt() const override;
  void CheckCompareInterrupt() override;
  std::vector<u8> Serialize() override;
  void Deserialize(const std::vector<u8> &) override;

  void Emit(u32);
  void special(u32);
  void regimm(u32);
  void add(u32);
  void addu(u32);
  void addi(u32);
  void addiu(u32);
  void andi(u32);
  void and_(u32);
  void branch_constant(bool cond, s64 offset);
  void branch_likely_constant(bool cond, s64 offset);
  void branch_abs_constant(bool cond, s64 address);
  void bltz(u32);
  void bgez(u32);
  void bltzl(u32);
  void bgezl(u32);
  void bltzal(u32);
  void bgezal(u32);
  void bltzall(u32);
  void bgezall(u32);
  void beq(u32);
  void beql(u32);
  void bne(u32);
  void bnel(u32);
  void blez(u32);
  void blezl(u32);
  void bgtz(u32);
  void bgtzl(u32);
  void bfc1(u32 instr);
  void blfc1(u32 instr);
  void bfc0(u32 instr);
  void blfc0(u32 instr);
  void dadd(u32);
  void daddu(u32);
  void daddi(u32);
  void daddiu(u32);
  void ddiv(u32);
  void ddivu(u32);
  void div(u32);
  void divu(u32);
  void dmult(u32);
  void dmultu(u32);
  void dsll(u32);
  void dsllv(u32);
  void dsll32(u32);
  void dsra(u32);
  void dsrav(u32);
  void dsra32(u32);
  void dsrl(u32);
  void dsrlv(u32);
  void dsrl32(u32);
  void dsub(u32);
  void dsubu(u32);
  void j(u32);
  void jr(u32);
  void jal(u32);
  void jalr(u32);
  void lui(u32);
  void lbu(u32);
  void lb(u32);
  void ld(u32);
  void ldc1(u32);
  void ldl(u32);
  void ldr(u32);
  void lh(u32);
  void lhu(u32);
  void ll(u32);
  void lld(u32);
  void lw(u32);
  void lwc1(u32);
  void lwl(u32);
  void lwu(u32);
  void lwr(u32);
  void mfhi(u32);
  void mflo(u32);
  void mult(u32);
  void multu(u32);
  void mthi(u32);
  void mtlo(u32);
  void nor(u32);
  void sb(u32) { Util::panic("[JIT] Implement sb!"); }
  void sc(u32) { Util::panic("[JIT] Implement sc!"); }
  void scd(u32) { Util::panic("[JIT] Implement scd!"); }
  void sd(u32) { Util::panic("[JIT] Implement sd!"); }
  void sdc1(u32) { Util::panic("[JIT] Implement sdc1!"); }
  void sdl(u32) { Util::panic("[JIT] Implement sdl!"); }
  void sdr(u32) { Util::panic("[JIT] Implement sdr!"); }
  void sh(u32) { Util::panic("[JIT] Implement sh!"); }
  void sw(u32);
  void swl(u32) { Util::panic("[JIT] Implement swl!"); }
  void swr(u32) { Util::panic("[JIT] Implement swr!"); }
  void slti(u32);
  void sltiu(u32);
  void slt(u32);
  void sltu(u32);
  void sll(u32);
  void sllv(u32);
  void sub(u32);
  void subu(u32);
  void swc1(u32) { Util::panic("[JIT] Implement swc1!"); }
  void sra(u32);
  void srav(u32);
  void srl(u32);
  void srlv(u32);
  void trap(bool) { Util::panic("[JIT] Implement trap!"); }
  void or_(u32);
  void ori(u32);
  void xor_(u32);
  void xori(u32);
};
#endif
} // namespace n64
