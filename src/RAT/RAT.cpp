#include "../include/RAT.hpp"
#include <cstdint>

RAT::RAT() {
  archStorage.enable = [this]() {
    return static_cast<bool>(needSquash) ||
      (!static_cast<bool>(recovery.active) && static_cast<bool>(rob.willCommit) &&
       static_cast<uint32_t>(rob.headDest) != 0 && static_cast<uint32_t>(rob.headNewPhy) != 0);
  };
  archStorage.writeEnable = [this]() { return !static_cast<bool>(needSquash) &&
    !static_cast<bool>(recovery.active) && static_cast<bool>(rob.willCommit) &&
    static_cast<uint32_t>(rob.headDest) != 0 && static_cast<uint32_t>(rob.headNewPhy) != 0; };
  archStorage.addr = []() { return 0u; };
  for (int i = 0; i < REGISTER_CAP; ++i) {
    archStorage.writeMask[i] = [this, i]() { return static_cast<uint32_t>(rob.headDest) == unsigned(i); };
    archStorage.writeData[i] = [this]() { return static_cast<uint32_t>(rob.headNewPhy); };
  }
}

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
  archStorage.work();
  const auto step = recovery.sample();
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
    }
    bootDone <= 1;
  } else if (step.active) {
    uint32_t restored[REGISTER_CAP];
    for (int i = 0; i < REGISTER_CAP; ++i)
      restored[i] = step.first() ? (static_cast<bool>(archValid[i]) ? archStorage.readLane(i) : unsigned(i)) : static_cast<uint32_t>(specRAT[i]);
    bool open = step.first() || static_cast<bool>(replayOpen);
    RobTag tag = step.nextTag;
    for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) {
      const unsigned offset = unsigned(step.offset) + lane;
      if (open && offset < ROB_CAP && offset < step.liveCount) {
        if (static_cast<uint32_t>(replay[lane].tag) == tag) {
          const uint32_t rd = static_cast<uint32_t>(replay[lane].rd);
          const uint32_t phy = static_cast<uint32_t>(replay[lane].newPhy);
          if (rd != 0 && phy != 0) restored[rd] = phy;
          if (tag == step.squashTag) open = false;
        } else open = false;
      }
      tag = robNextTag(tag);
    }
    for (int i = 0; i < REGISTER_CAP; ++i) specRAT[i] <= (i == 0 ? 0u : restored[i]);
    replayOpen <= open;
  } else {
    for (int i = 0; i < REGISTER_CAP; ++i) {
      if (!restore && issueHasDest && static_cast<uint32_t>(i) == issueDestReg) {
        specRAT[i] <= issuePhyReg;
      }

      if (!restore && commit && commitDest != 0 && commitPhy != InvalidPhy &&
          static_cast<uint32_t>(i) == commitDest) {
        archValid[i] <= 1;
      }
    }
  }
}
