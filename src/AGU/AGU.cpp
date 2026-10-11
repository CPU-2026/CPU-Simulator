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

uint32_t AGU::headValue() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
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

RobTag AGU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
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

uint8_t AGU::headMemIndex() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (static_cast<bool>(slotValid[i]) &&
        (best == -1 ||
         ROB::isOlder(
              static_cast<RobTag>(static_cast<uint32_t>(slots[i].robTag)),
              static_cast<RobTag>(
                  static_cast<uint32_t>(slots[best].robTag)))))
      best = i;
  }
  return best >= 0 ?
             static_cast<uint8_t>(static_cast<uint32_t>(slots[best].memIndex)) : 0;
}

void AGU::work() {
  const bool squash = static_cast<bool>(needSquash);
  const RobTag squashTag = static_cast<uint32_t>(SquashTag);
  const bool dValid = static_cast<bool>(dispatchValid);
  const RobTag dTag = static_cast<uint32_t>(dispatchRobTag);

  // The reference removes/flushes OLD results before writing the new result.
  const bool oldAny = !isEmpty();
  const RobTag headTag = oldAny ? headRobTag() : 0;

  // Reuse a slot released this cycle, deriving availability only from OLD
  // state and the same removal/flush intents used by the final validity write.
  bool found = false;
  uint32_t target = 0;
  for (uint32_t i = 0; i < AGU_CAP; ++i) {
    const bool oldValid = static_cast<bool>(slotValid[i]);
    const RobTag oldTag = static_cast<uint32_t>(slots[i].robTag);
    const bool removed = oldAny && oldValid && oldTag == headTag;
    const bool flushed = squash && oldValid && !ROB::isOlder(oldTag, squashTag);
    if (!found && (!oldValid || removed || flushed)) {
      target = i;
      found = true;
    }
  }
  const bool pushHere = dValid && found;
  // New validity depends on the NEW tag. An old younger result's cancellation
  // must not clear a strictly older result refilling its slot in the same cycle.
  const bool pushFlushed = squash && !ROB::isOlder(dTag, squashTag);

  // The redundant int32_t adds above the outer cast used to be evaluated
  // first and could overflow int32_t (host UB). Plain uint32 add wraps
  // by definition.
  const uint32_t v =
      static_cast<uint32_t>(src1Value) + static_cast<uint32_t>(src2Value);
  const uint32_t dMem = static_cast<uint32_t>(memIndex);

  // Single assignment per Register: replacement wins old release; keep for
  // the replacement is independently qualified by its own flush age.
  for (uint32_t i = 0; i < AGU_CAP; ++i) {
    const bool old_v = static_cast<bool>(slotValid[i]);
    const RobTag tag_i = static_cast<uint32_t>(slots[i].robTag);
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
