#include "../include/BRU.hpp"
#include "../include/ROB.hpp"
#include <cstdint>

namespace {
bool branchTaken(Operation op, int32_t op1, int32_t op2) {
  switch (op) {
  case Operation::EQ:
    return op1 == op2;
  case Operation::NE:
    return op1 != op2;
  case Operation::LT:
    return op1 < op2;
  case Operation::GE:
    return op1 >= op2;
  case Operation::LTU:
    return static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2);
  case Operation::GEU:
    return static_cast<uint32_t>(op1) >= static_cast<uint32_t>(op2);
  default:
    return false;
  }
}
} // namespace

bool BRU::isFull() const {
  for (int i = 0; i < BRU_CAP; i++)
    if (!static_cast<bool>(slotValid[i]))
      return false;
  return true;
}

bool BRU::isEmpty() const {
  for (int i = 0; i < BRU_CAP; i++)
    if (static_cast<bool>(slotValid[i]))
      return false;
  return true;
}

int32_t BRU::headPCFrom() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
             static_cast<uint8_t>(static_cast<uint32_t>(slots[i].robTag)),
             static_cast<uint8_t>(
                 static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ?
             static_cast<int32_t>(static_cast<uint32_t>(slots[best].pcFrom)) : 0;
}

int32_t BRU::headPCResult() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
             static_cast<uint8_t>(static_cast<uint32_t>(slots[i].robTag)),
             static_cast<uint8_t>(
                 static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ? static_cast<int32_t>(
                         static_cast<uint32_t>(slots[best].pcResult)) : 0;
}

uint8_t BRU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
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

void BRU::work() {
  const bool squash = static_cast<bool>(needSquash);
  const uint32_t squashTag = static_cast<uint32_t>(SquashTag);
  const bool dValid = static_cast<bool>(dispatchValid);
  const uint32_t dTag = static_cast<uint32_t>(dispatchRobTag);

  // the reference removes the oldest entry every cycle, judged over the
  // cycle-start (committed) state -- before this cycle's push landed
  const bool oldAny = !isEmpty();
  const uint32_t headTag = oldAny ? static_cast<uint32_t>(headRobTag()) : 0;

  // push target from the OLD validity bitmap only
  bool found = false;
  uint32_t target = 0;
  for (uint32_t i = 0; i < BRU_CAP; ++i) {
    if (!found && !static_cast<bool>(slotValid[i])) {
      target = i;
      found = true;
    }
  }
  const bool pushHere = dValid && found;
  // a freshly pushed entry is flushed by judging its NEW tag
  const bool pushFlushed = squash && !ROB::isOlder(dTag, squashTag);

  const int32_t s1 =
      static_cast<int32_t>(static_cast<uint32_t>(src1Value));
  const int32_t s2 =
      static_cast<int32_t>(static_cast<uint32_t>(src2Value));
  // pc/imm are inherited Input wires; no shadowing locals
  const int32_t pcV = static_cast<int32_t>(static_cast<uint32_t>(pc));
  const int32_t immV = static_cast<int32_t>(static_cast<uint32_t>(imm));
  const auto dOp = static_cast<Operation>(static_cast<uint32_t>(op));

  const uint32_t from = static_cast<uint32_t>(pcV);
  const uint32_t to =
      static_cast<uint32_t>(branchTaken(dOp, s1, s2) ? pcV + immV : pcV + 4);

  // single-assignment convergence: push/removeHead/flush all land on one
  // final value per slot; priority flush > removeHead > keep, push only into
  // dead slots
  for (uint32_t i = 0; i < BRU_CAP; ++i) {
    const bool old_v = static_cast<bool>(slotValid[i]);
    const uint32_t tag_i = static_cast<uint32_t>(slots[i].robTag);
    const bool removed = oldAny && old_v && tag_i == headTag;
    const bool flushed = squash && old_v && !ROB::isOlder(tag_i, squashTag);
    const bool here = pushHere && i == target;
    if (here) {
      slots[i].pcFrom <= from;
      slots[i].pcResult <= to;
      slots[i].robTag <= dTag;
    }
    slotValid[i] <= (here ? !pushFlushed : (old_v && !removed && !flushed));
  }
}
