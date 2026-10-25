#include "../include/Decoder.hpp"

Uop Decoder::decode(int32_t raw_inst) {
  Uop inst;
  uint32_t raw = static_cast<uint32_t>(raw_inst);
  auto opcode = raw & 0x7F;
  inst.opcode = opcode;
  inst.funct3 = (raw >> 12) & 0x7;
  inst.rd = (raw >> 7) & 0x1F;
  inst.rs1 = (raw >> 15) & 0x1F;
  inst.rs2 = (raw >> 20) & 0x1F;

  auto getImm = [&](RISC_V type) -> int32_t {
    switch (type) {
    case RISC_V::Istar:
      return static_cast<int32_t>((raw >> 20) & 0x1F);
    case RISC_V::I:
      return signExtend(static_cast<int32_t>((raw >> 20) & 0xFFF), 12);
    case RISC_V::S:
      return signExtend(
          static_cast<int32_t>(((raw >> 7) & 0x1F) | ((raw >> 25) << 5)), 12);
    case RISC_V::B:
      return signExtend(static_cast<int32_t>(
                            ((raw >> 31) << 12) | (((raw >> 25) & 0x3F) << 5) |
                            ((inst.rd & 1) << 11) | ((inst.rd >> 1) << 1)),
                        13);
    case RISC_V::J:
      return signExtend(static_cast<int32_t>(((raw >> 31) << 20) |
                                             (((raw >> 21) & 0x3FF) << 1) |
                                             (((raw << 11) >> 31) << 11) |
                                             (((raw << 12) >> 24) << 12)),
                        21);
    case RISC_V::U:
      return static_cast<int32_t>(raw_inst & 0xFFFFF000);
    default:
      return 0;
    }
  };

  switch (opcode) {
  case 0b0110011: {
    inst.funct7 = (raw >> 25) & 0x7F;
    if (inst.funct7 == 0b0000001) {
      inst.type = RISC_V::M;
    } else {
      inst.type = RISC_V::R;
    }
    break;
  }
  case 0b0010011: {
    inst.funct7 = (raw >> 25) & 0x7F;
    auto link_funct = (inst.funct3 << 7) | inst.funct7;
    if (link_funct == 0b0010000000 || link_funct == 0b1010000000 ||
        link_funct == 0b1010100000) {
      inst.type = RISC_V::Istar;
    } else {
      inst.type = RISC_V::I;
    }
    inst.imm = getImm(inst.type);
    break;
  }
  case 0b0000011:
  case 0b1100111: {
    inst.type = RISC_V::I;
    inst.imm = getImm(RISC_V::I);
    break;
  }
  case 0b0100011: {
    inst.type = RISC_V::S;
    inst.imm = getImm(RISC_V::S);
    break;
  }
  case 0b1100011: {
    inst.type = RISC_V::B;
    inst.imm = getImm(RISC_V::B);
    break;
  }
  case 0b1101111: {
    inst.type = RISC_V::J;
    inst.imm = getImm(RISC_V::J);
    break;
  }
  case 0b0010111:
  case 0b0110111: {
    inst.type = RISC_V::U;
    inst.imm = getImm(RISC_V::U);
    break;
  }
  default:
    inst.type = RISC_V::RV_INVALID;
    break;
  }
  if (inst.type != RISC_V::S && inst.type != RISC_V::B && inst.rd != 0) {
    inst.allocDest = true;
  }
  return inst;
}

void DecodeUnit::work() {
  // stage 3 flush first: on squash drop all decoded uops (head==tail makes
  // stale entries unreachable)
  if (needSquash) {
    head <= 0;
    tail <= 0;
  } else {
    // stage 2 pop: release the head uop(s) once the issue arbiter consumed them
    const unsigned pop = static_cast<uint32_t>(issuePopCount) & 0x3u;
    if (pop > 0 && !isEmpty()) {
      head <= ((static_cast<uint32_t>(head) + pop) & IQ_SEQ_MASK);
    }
    // stage 1 push: decode up to decodePushCount FQ entries into the IQ.
    const unsigned push = static_cast<uint32_t>(decodePushCount) & 0x3u;
    auto decodeInto = [&](unsigned slot, uint32_t raw, uint32_t pc,
                          uint32_t predictedPC, uint32_t ckptId) {
      Uop uop = Decoder::decode(static_cast<int32_t>(raw));
      uop.pc = pc;
      uop.predictedPC = static_cast<int32_t>(predictedPC);
      uop.ckptId = static_cast<uint8_t>(ckptId);
      uop.isHalt = legacyHaltEnabled && (raw == 0x0ff00513);
      entries[slot].type <= static_cast<uint32_t>(uop.type);
      entries[slot].opcode <= static_cast<uint32_t>(uop.opcode);
      entries[slot].funct3 <= static_cast<uint32_t>(uop.funct3);
      entries[slot].funct7 <= static_cast<uint32_t>(uop.funct7);
      entries[slot].rd <= static_cast<uint32_t>(uop.rd);
      entries[slot].rs1 <= static_cast<uint32_t>(uop.rs1);
      entries[slot].rs2 <= static_cast<uint32_t>(uop.rs2);
      entries[slot].imm <= static_cast<uint32_t>(uop.imm);
      entries[slot].pc <= static_cast<uint32_t>(uop.pc);
      entries[slot].isHalt <= static_cast<uint32_t>(uop.isHalt);
      entries[slot].allocDest <= static_cast<uint32_t>(uop.allocDest);
      entries[slot].predictedPC <= static_cast<uint32_t>(uop.predictedPC);
      entries[slot].ckptId <= static_cast<uint32_t>(uop.ckptId);
    };
    if (push >= 1) {
      const unsigned t0 = static_cast<uint32_t>(tail) & (IQ_CAP - 1);
      decodeInto(t0, static_cast<uint32_t>(fqHeadRaw),
                 static_cast<uint32_t>(fqHeadPc),
                 static_cast<uint32_t>(fqHeadPredictedPC),
                 static_cast<uint32_t>(fqHeadCkptId));
    }
    if (push >= 2) {
      const unsigned t1 = (static_cast<uint32_t>(tail) + 1) & (IQ_CAP - 1);
      decodeInto(t1, static_cast<uint32_t>(fqHeadRaw1),
                 static_cast<uint32_t>(fqHeadPc1),
                 static_cast<uint32_t>(fqHeadPredictedPC1),
                 static_cast<uint32_t>(fqHeadCkptId1));
    }
    if (push > 0) {
      tail <= ((static_cast<uint32_t>(tail) + push) & IQ_SEQ_MASK);
    }
  }
}
