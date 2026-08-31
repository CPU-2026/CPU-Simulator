#include "../include/ALU.hpp"
#include "../include/ROB.hpp"
#include <cstdint>

namespace {
int32_t evaluate(Operation op, int32_t op1, int32_t op2) {
  if (isControlOp(op))
    return op1 + op2;
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
    return static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
  case Operation::SRL:
    return static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
  case Operation::SRA:
    return op1 >> (op2 & 0x1F);
  case Operation::SLT:
    return op1 < op2 ? 1 : 0;
  case Operation::SLTU:
    return static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
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

int32_t ALU::headValue() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
             static_cast<uint8_t>(static_cast<uint32_t>(slots[i].robTag)),
             static_cast<uint8_t>(
                 static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ?
             static_cast<int32_t>(static_cast<uint32_t>(slots[best].value)) : 0;
}

uint8_t ALU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
             static_cast<uint8_t>(static_cast<uint32_t>(slots[i].robTag)),
             static_cast<uint8_t>(
                 static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ?
             static_cast<uint8_t>(static_cast<uint32_t>(slots[best].robTag)) : 0;
}

bool ALU::headIsControl() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
             static_cast<uint8_t>(static_cast<uint32_t>(slots[i].robTag)),
             static_cast<uint8_t>(
                 static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ? static_cast<bool>(slots[best].isControl) : false;
}

void ALU::work() {
  const bool squash = static_cast<bool>(needSquash);
  const uint32_t squashTag = static_cast<uint32_t>(SquashTag);
  const bool dValid = static_cast<bool>(dispatchValid);
  const uint32_t dTag = static_cast<uint32_t>(dispatchRobTag);
  const Operation dOp =
      static_cast<Operation>(static_cast<uint32_t>(op));
  const bool grant =
      static_cast<bool>(cdbValid) && static_cast<bool>(aluGranted);
  const uint32_t cdbTag = static_cast<uint32_t>(cdbRobTag);

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

  const int32_t v = evaluate(
      dOp, static_cast<int32_t>(static_cast<uint32_t>(src1Value)),
      static_cast<int32_t>(static_cast<uint32_t>(src2Value)));

  // single-assignment convergence: push/remove/flush all land on one final
  // value per slot; priority flush > remove > keep, push only into dead slots
  for (uint32_t i = 0; i < ALU_CAP; ++i) {
    const bool old_v = static_cast<bool>(slotValid[i]);
    const uint32_t tag_i = static_cast<uint32_t>(slots[i].robTag);
    const bool removed = grant && old_v && tag_i == cdbTag;
    const bool flushed = squash && old_v && !ROB::isOlder(tag_i, squashTag);
    const bool here = pushHere && i == target;
    if (here) {
      slots[i].value <= static_cast<uint32_t>(v);
      slots[i].robTag <= dTag;
      slots[i].isControl <= isControlOp(dOp);
    }
    slotValid[i] <= (here ? !pushFlushed : (old_v && !removed && !flushed));
  }
}
