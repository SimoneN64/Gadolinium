#include <Core.hpp>
#include <jit/helpers.hpp>

namespace n64 {
#ifndef __aarch64__
JIT::JIT(ParallelRDP &parallel) : regs(this), mem(regs, parallel, this) {
  blockCache.resize(kUpperSize);
  if (cs_open(CS_ARCH_MIPS, static_cast<cs_mode>(CS_MODE_MIPS64 | CS_MODE_BIG_ENDIAN), &disassemblerMips) !=
      CS_ERR_OK) {
    Util::panic("Failed to initialize MIPS disassembler");
  }

  if (cs_open(CS_ARCH_X86, static_cast<cs_mode>(CS_MODE_64 | CS_MODE_LITTLE_ENDIAN), &disassemblerX86) != CS_ERR_OK) {
    Util::panic("Failed to initialize x86 disassembler");
  }
}

bool JIT::ShouldServiceInterrupt() const {
  const bool interrupts_pending = (regs.cop0.status.im & regs.cop0.cause.interruptPending) != 0;
  const bool interrupts_enabled = regs.cop0.status.ie == 1;
  const bool currently_handling_exception = regs.cop0.status.exl == 1;
  const bool currently_handling_error = regs.cop0.status.erl == 1;

  return interrupts_pending && interrupts_enabled && !currently_handling_exception && !currently_handling_error;
}

void JIT::CheckCompareInterrupt() {
  regs.cop0.count++;
  regs.cop0.count &= 0x1FFFFFFFF;
  if (regs.cop0.count == static_cast<u64>(regs.cop0.compare) << 1) {
    regs.cop0.cause.ip7 = 1;
    mem.mmio.mi.UpdateInterrupt();
  }
}

void JIT::InvalidateBlock(const u32 paddr) {
  if (const u32 index = paddr >> kUpperShift; !blockCache[index].empty())
    blockCache[index] = {};
}

int JIT::Step() {
  blockOldPC = regs.oldPC;
  blockPC = regs.pc;
  blockNextPC = regs.nextPC;
  u32 paddr = 0;

  if (!regs.cop0.MapVAddr(Cop0::LOAD, blockPC, paddr)) {
    /*regs.cop0.HandleTLBException(blockPC);
    regs.cop0.FireException(Cop0::GetTLBExceptionCode(regs.cop0.tlbError, Cop0::LOAD), 0, blockPC);
    return 1;*/
    Util::panic("[JIT]: Unhandled exception TLB exception {} when retrieving PC physical address! (virtual: 0x{:016X})",
                static_cast<int>(Cop0::GetTLBExceptionCode(regs.cop0.tlbError, Cop0::LOAD)), static_cast<u64>(blockPC));
  }

  u32 upperIndex = paddr >> kUpperShift;
  u32 lowerIndex = paddr & kLowerMask;

  if (!blockCache[upperIndex].empty()) {
    if (blockCache[upperIndex][lowerIndex]) {
      // Util::trace("[JIT]: Executing already compiled block @ 0x{:016X}", blockPC);
      return blockCache[upperIndex][lowerIndex]();
    }
  } else {
    blockCache[upperIndex].resize(kLowerSize);
  }

  Util::trace("[JIT]: Compiling block @ 0x{:016X}:", blockPC);
  // const auto blockInfo = code.getCurr();
  const auto block = code.getCurr<BlockFn>();
  blockCache[upperIndex][lowerIndex] = block;

  code.setProtectModeRW();

  u32 instructionsInBlock = 0;

  bool instrEndsBlock = false;
  bool instrInDelaySlot = false;
  bool branchWasLikely = false;
  bool blockEndsOnBranch = false;

  code.sub(code.rsp, 8);
  code.push(code.rbp);
  code.mov(code.rbp, reinterpret_cast<uintptr_t>(this)); // Load context pointer

  // cs_insn *insn;
  Util::trace("\tMIPS code (guest PC = 0x{:016X}):", blockPC);
  while (!instrInDelaySlot) {
    // CheckCompareInterrupt();

    if (check_address_error(0b11, u64(blockPC))) [[unlikely]] {
      /*regs.cop0.HandleTLBException(blockPC);
      regs.cop0.FireException(ExceptionCode::AddressErrorLoad, 0, blockPC);
      return 1;*/

      Util::panic("[JIT]: Unhandled exception ADL due to unaligned PC virtual value! (0x{:016X})", blockPC);
    }

    if (!regs.cop0.MapVAddr(Cop0::LOAD, blockPC, paddr)) {
      /*regs.cop0.HandleTLBException(blockPC);
      regs.cop0.FireException(Cop0::GetTLBExceptionCode(regs.cop0.tlbError, Cop0::LOAD), 0, blockPC);
      return 1;*/
      Util::panic(
        "[JIT]: Unhandled exception TLB exception {} when retrieving PC physical address! (virtual: 0x{:016X})",
        static_cast<int>(Cop0::GetTLBExceptionCode(regs.cop0.tlbError, Cop0::LOAD)), static_cast<u64>(blockPC));
    }

    const u32 instruction = mem.Read<u32>(regs, paddr);

    /*u32 bswapped = bswap(instruction);
    auto count = cs_disasm(disassemblerMips, reinterpret_cast<const u8 *>(&bswapped), 4, blockPC, 0, &insn);

    if (count > 0) {
      Util::trace("\t\t0x{:016X}:\t{}\t\t{}\n", insn->address, insn->mnemonic, insn->op_str);
      cs_free(insn, count);
    } else {
      Util::trace("\t\tCould not disassemble 0x{:08X} due to error {}\n", instruction, (int)cs_errno(disassemblerMips));
    }

    if(ShouldServiceInterrupt()) {
      regs.cop0.FireException(ExceptionCode::Interrupt, 0, blockPC);
      return 1;
    }*/

    blockOldPC = blockPC;
    blockPC = blockNextPC;
    blockNextPC += 4;
    instructionsInBlock++;
    Emit(instruction);

    instrInDelaySlot = instrEndsBlock;
    instrEndsBlock = InstrEndsBlock(instruction);
    if (instrEndsBlock) {
      branchWasLikely = IsBranchLikely(instruction);
    }

    if (instrInDelaySlot) {
      blockEndsOnBranch = true;
    }

    if (instrInDelaySlot && branchWasLikely) {
      branchWasLikely = false;
      code.L(branch_likely_not_taken);
      code.mov(code.rax, blockPC);
      code.mov(REG(qword, pc), code.rax);
    }
  }

  // emit code to store the value of pc
  if (!blockEndsOnBranch) {
    code.mov(code.rax, blockPC);
    code.mov(REG(qword, pc), code.rax);
  }
  code.mov(code.rax, instructionsInBlock);
  code.pop(code.rbp);
  code.add(code.rsp, 8);
  code.ret();
  code.setProtectModeRE();
  /* static auto blockInfoSize = 0;
  blockInfoSize = code.getSize() - blockInfoSize;

  Util::trace("\tX86 code (block address = 0x{:016X}):", (uintptr_t)block);
  auto count = cs_disasm(disassemblerX86, blockInfo, blockInfoSize, (uintptr_t)block, 0, &insn);
  if (count > 0) {
    for (size_t j = 0; j < count; j++) {
      Util::trace("\t\t0x{:016X}:\t{}\t\t{}\n", insn[j].address, insn[j].mnemonic, insn[j].op_str);
    }

    cs_free(insn, count);
  }*/
  // const auto dump = code.getCode();
  // Util::WriteFileBinary(dump, code.getSize(), "jit.dump");
  //  Util::panic("");
  return block();
}

std::vector<u8> JIT::Serialize() {
  std::vector<u8> res{};

  res.resize(sizeof(Registers));

  memcpy(res.data(), &regs, sizeof(Registers));

  return res;
}

void JIT::Deserialize(const std::vector<u8> &data) { memcpy(&regs, data.data(), sizeof(Registers)); }
#endif
} // namespace n64
