#include "../include/ALU.hpp"
#include "../include/ROB.hpp"
#include <cstdint>

namespace {
uint32_t evaluate(Operation op, uint32_t op1, uint32_t op2) {
  // Operands and the result are uint32 bit vectors (RTL semantics).
  // Signedness is selected by the instruction: only SLT/SLTI and SRA
  // interpret the operand as int32_t; every other op is pure bit-vector
  // arithmetic.
  if (isControlOp(op))
    // JALR clears the target's bit 0 (RISC-V: (rs1 + imm) & ~1). J-type
    // imm bit0 is always 0, so the same mask is harmless for JAL.
    return (op1 + op2) & 0xFFFFFFFEu;
  switch (op) {
  case Operation::ADD:
  case Operation::AUIPC:
    return op1 + op2;
  case Operation::SUB:
    return op1 - op2;
  case Operation::XOR:
    return op1 ^ op2;
  case Operation::OR:
    return op1 | op2;
  case Operation::AND:
    return op1 & op2;
  case Operation::SL:
    return op1 << (op2 & 0x1F);
  case Operation::SRL:
    return op1 >> (op2 & 0x1F);
  case Operation::SRA:
    // C++20: right shift of a negative signed value is arithmetic.
    return static_cast<uint32_t>(static_cast<int32_t>(op1) >> (op2 & 0x1F));
  case Operation::SLT:
    return static_cast<int32_t>(op1) < static_cast<int32_t>(op2) ? 1u : 0u;
  case Operation::SLTU:
    return op1 < op2 ? 1u : 0u;
  case Operation::LUI:
    return op2;
  default:
    return 0;
  }
}
} // namespace

bool ALU::isFull() const {
  for (int i = 0; i < ALU_CAP; i++)
    if (!static_cast<bool>(slotValid[i]))
      return false;
  return true;
}

bool ALU::isEmpty() const {
  for (int i = 0; i < ALU_CAP; i++)
    if (static_cast<bool>(slotValid[i]))
      return false;
  return true;
}

uint32_t ALU::headValue() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
              static_cast<RobTag>(static_cast<uint32_t>(slots[i].robTag)),
              static_cast<RobTag>(
                  static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ? static_cast<uint32_t>(slots[best].value) : 0;
}

RobTag ALU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
              static_cast<RobTag>(static_cast<uint32_t>(slots[i].robTag)),
              static_cast<RobTag>(
                  static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ?
             static_cast<RobTag>(static_cast<uint32_t>(slots[best].robTag)) : 0;
}

bool ALU::headIsControl() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
              static_cast<RobTag>(static_cast<uint32_t>(slots[i].robTag)),
              static_cast<RobTag>(
                  static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ? static_cast<bool>(slots[best].isControl) : false;
}

void ALU::work() {
  const bool squash = static_cast<bool>(needSquash);
  const RobTag squashTag = static_cast<uint32_t>(SquashTag);
  const bool dValid = static_cast<bool>(dispatchValid);
  const RobTag dTag = static_cast<uint32_t>(dispatchRobTag);
  const Operation dOp =
      static_cast<Operation>(static_cast<uint32_t>(op));
  // grant = this entry is the aluCDB winner; dual-CDB leaves ALU as the sole
  // candidate so valid implies granted (retired the aluGranted select bit)
  const bool grant = static_cast<bool>(cdbValid);
  const RobTag cdbTag = static_cast<uint32_t>(cdbRobTag);

  // push target from the OLD validity bitmap only: the reference picked the
  // free slot before remove/flush ran, so a slot freed this cycle stays
  // unusable until the next cycle
  bool found = false;
  uint32_t target = 0;
  for (uint32_t i = 0; i < ALU_CAP; ++i) {
    if (!found && !static_cast<bool>(slotValid[i])) {
      target = i;
      found = true;
    }
  }
  const bool pushHere = dValid && found;
  // a freshly pushed entry is flushed by judging its NEW tag (the reference
  // flushed after push had overwritten the payload)
  const bool pushFlushed = squash && !ROB::isOlder(dTag, squashTag);

  const uint32_t v = evaluate(dOp, static_cast<uint32_t>(src1Value),
                              static_cast<uint32_t>(src2Value));

  // single-assignment convergence: push/remove/flush all land on one final
  // value per slot; priority flush > remove > keep, push only into dead slots
  for (uint32_t i = 0; i < ALU_CAP; ++i) {
    const bool old_v = static_cast<bool>(slotValid[i]);
    const RobTag tag_i = static_cast<uint32_t>(slots[i].robTag);
    const bool removed = grant && old_v && tag_i == cdbTag;
    const bool flushed = squash && old_v && !ROB::isOlder(tag_i, squashTag);
    const bool here = pushHere && i == target;
    if (here) {
      slots[i].value <= v;
      slots[i].robTag <= dTag;
      slots[i].isControl <= isControlOp(dOp);
    }
    slotValid[i] <= (here ? !pushFlushed : (old_v && !removed && !flushed));
  }
}
