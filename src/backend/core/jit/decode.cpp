#include <JIT.hpp>
#include <CpuDefinitions.hpp>

namespace n64 {
void JIT::special(const u32 instr) {
  // 00rr_rccc
  switch (const u8 mask = instr & 0x3F) {
  case SLL:
    if (instr != 0) {
      sll(instr);
    }
    break;
  case SRL:
    srl(instr);
    break;
  case SRA:
    sra(instr);
    break;
  case SLLV:
    sllv(instr);
    break;
  case SRLV:
    srlv(instr);
    break;
  case SRAV:
    srav(instr);
    break;
  case JR:
    jr(instr);
    break;
  case JALR:
    jalr(instr);
    break;
  case SYSCALL:
    regs.cop0.FireException(ExceptionCode::Syscall, 0, regs.oldPC);
    break;
  case BREAK:
    regs.cop0.FireException(ExceptionCode::Breakpoint, 0, regs.oldPC);
    break;
  case SYNC:
    break; // SYNC
  case MFHI:
    mfhi(instr);
    break;
  case MTHI:
    mthi(instr);
    break;
  case MFLO:
    mflo(instr);
    break;
  case MTLO:
    mtlo(instr);
    break;
  case DSLLV:
    dsllv(instr);
    break;
  case DSRLV:
    dsrlv(instr);
    break;
  case DSRAV:
    dsrav(instr);
    break;
  case MULT:
    mult(instr);
    break;
  case MULTU:
    multu(instr);
    break;
  case DIV:
    div(instr);
    break;
  case DIVU:
    divu(instr);
    break;
  case DMULT:
    dmult(instr);
    break;
  case DMULTU:
    dmultu(instr);
    break;
  case DDIV:
    ddiv(instr);
    break;
  case DDIVU:
    ddivu(instr);
    break;
  case ADD:
    add(instr);
    break;
  case ADDU:
    addu(instr);
    break;
  case SUB:
    sub(instr);
    break;
  case SUBU:
    subu(instr);
    break;
  case AND:
    and_(instr);
    break;
  case OR:
    or_(instr);
    break;
  case XOR:
    xor_(instr);
    break;
  case NOR:
    nor(instr);
    break;
  case SLT:
    slt(instr);
    break;
  case SLTU:
    sltu(instr);
    break;
  case DADD:
    dadd(instr);
    break;
  case DADDU:
    daddu(instr);
    break;
  case DSUB:
    dsub(instr);
    break;
  case DSUBU:
    dsubu(instr);
    break;
  case TGE:
    trap(regs.Read<s64>(RS(instr)) >= regs.Read<s64>(RT(instr)));
    break;
  case TGEU:
    trap(regs.Read<u64>(RS(instr)) >= regs.Read<u64>(RT(instr)));
    break;
  case TLT:
    trap(regs.Read<s64>(RS(instr)) < regs.Read<s64>(RT(instr)));
    break;
  case TLTU:
    trap(regs.Read<u64>(RS(instr)) < regs.Read<u64>(RT(instr)));
    break;
  case TEQ:
    trap(regs.Read<s64>(RS(instr)) == regs.Read<s64>(RT(instr)));
    break;
  case TNE:
    trap(regs.Read<s64>(RS(instr)) != regs.Read<s64>(RT(instr)));
    break;
  case DSLL:
    dsll(instr);
    break;
  case DSRL:
    dsrl(instr);
    break;
  case DSRA:
    dsra(instr);
    break;
  case DSLL32:
    dsll32(instr);
    break;
  case DSRL32:
    dsrl32(instr);
    break;
  case DSRA32:
    dsra32(instr);
    break;
  default:
    Util::panic("Unimplemented special {} {} ({:08X}) (pc: {:016X})", (mask >> 3) & 7, mask & 7, instr,
                static_cast<u64>(regs.oldPC));
  }
}

void JIT::regimm(const u32 instr) {
  // 000r_rccc
  switch (const u8 mask = instr >> 16 & 0x1F) {
  case BLTZ:
    bltz(instr);
    break;
  case BGEZ:
    bgez(instr);
    break;
  case BLTZL:
    bltzl(instr);
    break;
  case BGEZL:
    bgezl(instr);
    break;
  case TGEI:
    trap(regs.Read<s64>(RS(instr)) >= static_cast<s64>(static_cast<s16>(instr)));
    break;
  case TGEIU:
    trap(regs.Read<u64>(RS(instr)) >= static_cast<u64>(static_cast<s64>(static_cast<s16>(instr))));
    break;
  case TLTI:
    trap(regs.Read<s64>(RS(instr)) < static_cast<s64>(static_cast<s16>(instr)));
    break;
  case TLTIU:
    trap(regs.Read<u64>(RS(instr)) < static_cast<u64>(static_cast<s64>(static_cast<s16>(instr))));
    break;
  case TEQI:
    trap(regs.Read<s64>(RS(instr)) == static_cast<s64>(static_cast<s16>(instr)));
    break;
  case TNEI:
    trap(regs.Read<s64>(RS(instr)) != static_cast<s64>(static_cast<s16>(instr)));
    break;
  case BLTZAL:
    bltzal(instr);
    break;
  case BGEZAL:
    bgezal(instr);
    break;
  case BLTZALL:
    bltzall(instr);
    break;
  case BGEZALL:
    bgezall(instr);
    break;
  default:
    Util::panic("Unimplemented regimm {} {} ({:08X}) (pc: {:016X})", (mask >> 3) & 3, mask & 7, instr,
                static_cast<u64>(regs.oldPC));
  }
}

void JIT::Emit(const u32 instr) {
  switch (const u8 mask = instr >> 26 & 0x3f) {
  case SPECIAL:
    special(instr);
    break;
  case REGIMM:
    regimm(instr);
    break;
  case J:
    j(instr);
    break;
  case JAL:
    jal(instr);
    break;
  case BEQ:
    beq(instr);
    break;
  case BNE:
    bne(instr);
    break;
  case BLEZ:
    blez(instr);
    break;
  case BGTZ:
    bgtz(instr);
    break;
  case ADDI:
    addi(instr);
    break;
  case ADDIU:
    addiu(instr);
    break;
  case SLTI:
    slti(instr);
    break;
  case SLTIU:
    sltiu(instr);
    break;
  case ANDI:
    andi(instr);
    break;
  case ORI:
    ori(instr);
    break;
  case XORI:
    xori(instr);
    break;
  case LUI:
    lui(instr);
    break;
  case COP0:
    Util::panic("[JIT]: Unimplemented Cop0 decode");
    break;
  case COP1:
    {
      const u8 mask_sub = (instr >> 21) & 0x1F;
      const u8 mask_branch = (instr >> 16) & 0x1F;
      if (mask_sub == 0x08) {
        switch (mask_branch) {
        case 0:
          // if (!regs.cop1.CheckFPUUsable())
          //   return;
          bfc0(instr);
          break;
        case 1:
          // if (!regs.cop1.CheckFPUUsable())
          //   return;
          bfc1(instr);
          break;
        case 2:
          // if (!regs.cop1.CheckFPUUsable())
          //   return;
          blfc0(instr);
          break;
        case 3:
          // if (!regs.cop1.CheckFPUUsable())
          //   return;
          blfc1(instr);
          break;
        default:
          Util::panic("Undefined BC COP1 {:02X}", mask_branch);
        }
        break;
      }
      regs.cop1.decode(instr);
    }
    break;
  case COP2:
    break;
  case BEQL:
    beql(instr);
    break;
  case BNEL:
    bnel(instr);
    break;
  case BLEZL:
    blezl(instr);
    break;
  case BGTZL:
    bgtzl(instr);
    break;
  case DADDI:
    daddi(instr);
    break;
  case DADDIU:
    daddiu(instr);
    break;
  case LDL:
    ldl(instr);
    break;
  case LDR:
    ldr(instr);
    break;
  case 0x1F:
    regs.cop0.FireException(ExceptionCode::ReservedInstruction, 0, regs.oldPC);
    break;
  case LB:
    lb(instr);
    break;
  case LH:
    lh(instr);
    break;
  case LWL:
    lwl(instr);
    break;
  case LW:
    lw(instr);
    break;
  case LBU:
    lbu(instr);
    break;
  case LHU:
    lhu(instr);
    break;
  case LWR:
    lwr(instr);
    break;
  case LWU:
    lwu(instr);
    break;
  case SB:
    sb(instr);
    break;
  case SH:
    sh(instr);
    break;
  case SWL:
    swl(instr);
    break;
  case SW:
    sw(instr);
    break;
  case SDL:
    sdl(instr);
    break;
  case SDR:
    sdr(instr);
    break;
  case SWR:
    swr(instr);
    break;
  case CACHE:
    break; // CACHE
  case LL:
    ll(instr);
    break;
  case LWC1:
    lwc1(instr);
    break;
  case LLD:
    lld(instr);
    break;
  case LDC1:
    ldc1(instr);
    break;
  case LD:
    ld(instr);
    break;
  case SC:
    sc(instr);
    break;
  case SWC1:
    swc1(instr);
    break;
  case SCD:
    scd(instr);
    break;
  case SDC1:
    sdc1(instr);
    break;
  case SD:
    sd(instr);
    break;
  default:
    Util::panic("Unimplemented instruction {:02X} ({:08X}) (pc: {:016X})", mask, instr, static_cast<u64>(regs.oldPC));
  }
}
} // namespace n64
