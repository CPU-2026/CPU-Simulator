#include "../include/AGU.hpp"
#include "../include/ROB.hpp"
#include <cstdint>

bool AGU::isFull() const {
  for (int i = 0; i < AGU_CAP; i++)
    if (!static_cast<bool>(slotValid[i]))
      return false;
  return true;
}

bool AGU::isEmpty() const {
  for (int i = 0; i < AGU_CAP; i++)
    if (static_cast<bool>(slotValid[i]))
      return false;
  return true;
}

int32_t AGU::headValue() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
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

uint8_t AGU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
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

uint8_t AGU::headMemIndex() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
             static_cast<uint8_t>(static_cast<uint32_t>(slots[i].robTag)),
             static_cast<uint8_t>(
                 static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ?
             static_cast<uint8_t>(static_cast<uint32_t>(slots[best].memIndex)) : 0;
}

void AGU::work() {
  const bool squash = static_cast<bool>(needSquash);
  const uint32_t squashTag = static_cast<uint32_t>(SquashTag);
  const bool dValid = static_cast<bool>(dispatchValid);
  const uint32_t dTag = static_cast<uint32_t>(dispatchRobTag);

  // the reference removes the oldest entry every cycle, judged over the
  // cycle-start (committed) state -- before this cycle's push landed
  const bool oldAny = !isEmpty();
  const uint32_t headTag = oldAny ? static_cast<uint32_t>(headRobTag()) : 0;

  // push target from the OLD validity bitmap only: the reference picked the
  // free slot before remove/flush ran
  bool found = false;
  uint32_t target = 0;
  for (uint32_t i = 0; i < AGU_CAP; ++i) {
    if (!found && !static_cast<bool>(slotValid[i])) {
      target = i;
      found = true;
    }
  }
  const bool pushHere = dValid && found;
  // a freshly pushed entry is flushed by judging its NEW tag (the reference
  // flushed after push had overwritten the payload)
  const bool pushFlushed = squash && !ROB::isOlder(dTag, squashTag);

  const uint32_t v = static_cast<uint32_t>(
      static_cast<int32_t>(static_cast<uint32_t>(src1Value)) +
      static_cast<int32_t>(static_cast<uint32_t>(src2Value)));
  const uint32_t dMem = static_cast<uint32_t>(memIndex);

  // single-assignment convergence: push/removeHead/flush all land on one
  // final value per slot; priority flush > removeHead > keep, push only into
  // dead slots
  for (uint32_t i = 0; i < AGU_CAP; ++i) {
    const bool old_v = static_cast<bool>(slotValid[i]);
    const uint32_t tag_i = static_cast<uint32_t>(slots[i].robTag);
    const bool removed = oldAny && old_v && tag_i == headTag;
    const bool flushed = squash && old_v && !ROB::isOlder(tag_i, squashTag);
    const bool here = pushHere && i == target;
    if (here) {
      slots[i].value <= v;
      slots[i].robTag <= dTag;
      slots[i].memIndex <= dMem;
    }
    slotValid[i] <= (here ? !pushFlushed : (old_v && !removed && !flushed));
  }
}
