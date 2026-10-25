#include "../include/RAT.hpp"
#include <cstdint>

RAT::RAT() {
  archStorage.enable = [this]() {
    return bool(needSquash) || bool(archStorage.writeEnable);
  };
  archStorage.writeEnable = [this]() {
    bool write = false;
    for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
      write |= bool(commits[lane].valid) && uint32_t(commits[lane].rd) != 0 && uint32_t(commits[lane].newPhy) != 0;
    return write;
  };
  archStorage.addr = []() { return 0u; };
  for (int i = 0; i < REGISTER_CAP; ++i) {
    archStorage.writeMask[i] = [this, i]() {
      bool write = false;
      for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
        write |= bool(commits[lane].valid) && uint32_t(commits[lane].rd) == unsigned(i) && i != 0 && uint32_t(commits[lane].newPhy) != 0;
      return write;
    };
    archStorage.writeData[i] = [this, i]() {
      uint32_t phy = 0;
      for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
        if (bool(commits[lane].valid) && uint32_t(commits[lane].rd) == unsigned(i) && uint32_t(commits[lane].newPhy) != 0)
          phy = uint32_t(commits[lane].newPhy);
      return phy;
    };
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

  bool issueHasDest[ISSUE_WIDTH];
  uint32_t issueDestReg[ISSUE_WIDTH];
  uint32_t issuePhyReg[ISSUE_WIDTH];
  for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
    issueHasDest[L] = static_cast<bool>(issue[L].valid) &&
                      static_cast<bool>(issue[L].allocDest);
    issueDestReg[L] = static_cast<uint32_t>(issue[L].dest);
    issuePhyReg[L] = static_cast<uint32_t>(issue[L].phy);
  }

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
      if (!restore) {
        // Merge the ISSUE_WIDTH rename ports: the younger lane wins on a same
        // destination, and each specRAT entry is written at most once.
        uint32_t finalPhy = 0;
        bool hasWrite = false;
        for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
          if (issueHasDest[L] && static_cast<uint32_t>(i) == issueDestReg[L]) {
            finalPhy = issuePhyReg[L];
            hasWrite = true;
          }
        }
        if (hasWrite)
          specRAT[i] <= finalPhy;
      }

      if (bool(archStorage.writeEnable) && bool(archStorage.writeMask[i])) {
        archValid[i] <= 1;
      }
    }
  }
}
