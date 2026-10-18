#pragma once
#include "common.h"
#include "module.h"

struct RecoveryNarrowPort {
  Wire<ROB_TAG_WIDTH> tag;
  Wire<5> rd;
  Wire<PHY_TAG_WIDTH> newPhy;
};
// Decoded pop/push are already stored by the software ROB.
struct RecoveryRasPort {
  Wire<ROB_TAG_WIDTH> tag;
  Wire<32> pc;
  Wire<1> pop, push;
};

// Producer-owned cycle-start recovery cursor. Defaults leave standalone
// non-recovery module fixtures usable; CPU wiring replaces each default once.
struct RecoveryInput {
  Wire<1> active{[]() { return 0u; }};
  Wire<2> beat{[]() { return 0u; }};
  Wire<ROB_TAG_WIDTH> offset{[]() { return 0u; }};
  Wire<ROB_TAG_WIDTH> liveCount{[]() { return 0u; }};
  Wire<ROB_TAG_WIDTH> nextTag{[]() { return 0u; }};
  Wire<ROB_TAG_WIDTH> squashTag{[]() { return 0u; }};
  RecoveryStep sample() const {
    return {static_cast<bool>(active), uint8_t(static_cast<uint32_t>(beat)),
            uint8_t(static_cast<uint32_t>(offset)), uint8_t(static_cast<uint32_t>(liveCount)),
            RobTag(static_cast<uint32_t>(nextTag)), RobTag(static_cast<uint32_t>(squashTag))};
  }
};
