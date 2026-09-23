#include "../include/RAT.hpp"
#include <cstdint>

uint8_t RAT::readRAT_PRF(int regNum) const {
  return static_cast<uint32_t>(specRAT[regNum]);
}

OperandInfo RAT::readOperand(int regNum) const {
  if (regNum == 0)
    return {true, 0, InvalidPhy};
  auto phy = static_cast<uint32_t>(specRAT[regNum]);
  return {false, 0, phy};
}

void RAT::work() {
  const bool boot = bootDone == 0;
  const bool restore = static_cast<bool>(needSquash);
  const RobTag squashTag = static_cast<uint32_t>(SquashTag);
  const RobTag robHead = static_cast<uint32_t>(rob.head);
  const RobTag robNext = static_cast<uint32_t>(rob.next);

  // host-only: squash-window check verifies the upstream FlushArbiter
  // guarantee without adding replay-time hardware.
#ifdef _DEBUG
  bool squashInWindow = false;
  if (restore) {
    RobTag cursor = robHead;
    bool windowOpen = !static_cast<bool>(rob.isEmpty);
    for (int k = 0; k < ROB_CAP; ++k) {
      if (windowOpen) {
        if (cursor == squashTag)
          squashInWindow = true;
        cursor = robNextTag(cursor);
        if (cursor == robNext)
          windowOpen = false;
      }
    }
  }
  bool squashRowMatches = true;
  if (restore) {
    squashRowMatches =
        static_cast<uint32_t>(rob.tag[robSlot(squashTag)]) == squashTag;
  }
  const bool validSquash = !restore || (squashInWindow && squashRowMatches);
  dark::debug::assert(validSquash,
                      "RAT squash tag is not in the live ROB window");
#endif

  const bool issueHasDest = static_cast<bool>(issueValid) &&
                            static_cast<bool>(issueAllocDest);
  const uint32_t issueDestReg = static_cast<uint32_t>(issueDest);
  const uint32_t issuePhyReg = static_cast<uint32_t>(issuePhy);
  const bool commit = static_cast<bool>(rob.willCommit);
  const uint32_t commitDest = static_cast<uint32_t>(rob.headDest);
  const uint32_t commitPhy = static_cast<uint32_t>(rob.headNewPhy);

  if (boot) {
    for (int i = 1; i < REGISTER_CAP; ++i) {
      specRAT[i] <= i;
      archRAT[i] <= i;
    }
    bootDone <= 1;
  } else {
    for (int i = 0; i < REGISTER_CAP; ++i) {
      if (restore) {
        // The squash instruction remains live and its rename is replayed.
        uint32_t restored = static_cast<uint32_t>(archRAT[i]);
        RobTag cursor = robHead;
        bool replaying = true;
        for (int k = 0; k < ROB_CAP; ++k) {
          if (replaying) {
            const uint32_t slot = robSlot(cursor);
            const uint32_t dest = static_cast<uint32_t>(rob.dest[slot]);
            const uint32_t newPhy =
                static_cast<uint32_t>(rob.newPhy[slot]);
            if (dest != 0 && newPhy != InvalidPhy &&
                dest == static_cast<uint32_t>(i)) {
              restored = newPhy;
            }
            if (cursor == squashTag) {
              replaying = false;
            } else {
              cursor = robNextTag(cursor);
            }
          }
        }
        specRAT[i] <= restored;
      } else if (issueHasDest && static_cast<uint32_t>(i) == issueDestReg) {
        specRAT[i] <= issuePhyReg;
      }

      if (commit && commitDest != 0 && commitPhy != InvalidPhy &&
          static_cast<uint32_t>(i) == commitDest) {
        archRAT[i] <= commitPhy;
      }
    }
  }
}
