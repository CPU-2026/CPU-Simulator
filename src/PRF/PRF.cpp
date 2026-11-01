#include "PRF.hpp"
#include "ROB.hpp"
#include <cassert>

PRF::PRF() {
  for (unsigned lane = 0; lane < ISSUE_WIDTH; ++lane) {
    valuePorts[lane].valid = [this, lane]() {
      return bool(bootDone) && !bool(freeze) && !bool(squash.needSquash) &&
          !bool(recovery.active) && bool(issue[lane].issueValid) &&
          bool(issue[lane].issueAllocDest) && uint32_t(issue[lane].issuePhy) != 0;
    };
    valuePorts[lane].phy = [this, lane]() { return uint32_t(issue[lane].issuePhy); };
    valuePorts[lane].value = [this, lane]() { return uint32_t(issue[lane].issuePC) + 4u; };
    valuePorts[lane].ready = [this, lane]() { return bool(issue[lane].issueIsControl); };
  }
  for (unsigned port = 0; port < ALU_CDB_PORTS; ++port) {
    const auto *cdb = port == 0 ? &cdbOfALU : &cdbOfALUSecond;
    auto &write = valuePorts[ISSUE_WIDTH + port];
    write.valid = [this, cdb]() {
      return bool(bootDone) && !bool(freeze) && bool(cdb->cdbValid) &&
          !bool(cdb->cdbIsControl) && uint32_t(cdb->cdbNewPhy) != 0 &&
          (!bool(squash.needSquash) || ROB::isOlder(uint32_t(cdb->cdbRobTag), uint32_t(squash.SquashTag)));
    };
    write.phy = [cdb]() { return uint32_t(cdb->cdbNewPhy); };
    write.value = [cdb]() { return uint32_t(cdb->cdbValue); };
    write.ready = []() { return true; };
  }
  auto &shared = valuePorts[PRF_VALUE_WRITERS - 1];
  shared.valid = [this]() {
    return bool(bootDone) && !bool(freeze) && bool(cdbOfShared.cdbValid) &&
        uint32_t(cdbOfShared.cdbNewPhy) != 0 &&
        (!bool(squash.needSquash) || ROB::isOlder(uint32_t(cdbOfShared.cdbRobTag), uint32_t(squash.SquashTag)));
  };
  shared.phy = [this]() { return uint32_t(cdbOfShared.cdbNewPhy); };
  shared.value = [this]() { return uint32_t(cdbOfShared.cdbValue); };
  shared.ready = []() { return true; };
  for (unsigned bank = 0; bank < PRF_CAP - 1; ++bank) {
    payload[bank].enable = [this]() { return bool(bootDone); };
    payload[bank].addr = []() { return 0u; };
    payload[bank].writeMask[0] = []() { return true; };
    payload[bank].writeEnable = [this, bank]() { return writeFor(bank + 1).valid; };
    payload[bank].writeData[0] = [this, bank]() { return writeFor(bank + 1).value; };
  }
}

PRFValueWrite PRF::writeFor(unsigned phy) const {
  PRFValueWrite result{};
  // Younger allocation wins; all WB ports follow allocations, highest WB wins.
  for (const auto &port : valuePorts)
    if (bool(port.valid) && uint32_t(port.phy) == phy)
      result = {true, bool(port.ready), uint32_t(port.value)};
  return result;
}

void PRF::work() {
  if (!bool(bootDone)) {
    for (int i = 0; i < REGISTER_CAP; ++i) PhysicalRegs[i].ready <= true;
    for (int i = 0; i < PRF_CAP - REGISTER_CAP; ++i) freeList[i] <= uint32_t(REGISTER_CAP + i);
    tailSeq <= uint32_t(PRF_CAP - REGISTER_CAP);
    headSeq <= 0u;
    bootDone <= true;
  } else if (!bool(freeze)) {
    const bool needSquash = bool(squash.needSquash);
    const PrfSeq curHead = uint32_t(headSeq);
    PrfSeq nextHead = curHead;
    unsigned popCount = 0;
    if (!needSquash && !bool(recovery.active)) {
      for (unsigned lane = 0; lane < ISSUE_WIDTH; ++lane) {
        if (bool(issue[lane].issueValid) && bool(issue[lane].issueAllocDest)) {
          assert(hasFree(popCount + 1u));
          const uint8_t phy = uint32_t(freeList[prfSlot(nextHead)]);
          assert(phy != InvalidPhy && phy == uint32_t(issue[lane].issuePhy));
          nextHead = prfSeqNext(nextHead);
          ++popCount;
        }
      }
    }
    if (popCount != 0u) headSeq <= nextHead;

    // Recovery destinations first, then committed old mappings, exactly once.
    uint32_t recPhy[ROB_CAP + 1];
    uint32_t nRec = 0;
    const auto step = recovery.sample();
    if (step.active) {
      RobTag tag = step.nextTag;
      for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) {
        const unsigned offset = unsigned(step.offset) + lane;
        if (offset < ROB_CAP && offset < step.liveCount &&
            uint32_t(replay[lane].tag) == tag && ROB::isYounger(tag, step.squashTag)) {
          const uint32_t phy = uint32_t(replay[lane].newPhy);
          if (phy != 0) recPhy[nRec++] = phy;
        }
        tag = robNextTag(tag);
      }
    }
    for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
      if (bool(commits[lane].valid) && !bool(commits[lane].halt)) {
        const uint32_t type = uint32_t(commits[lane].type);
        if (type == uint32_t(ROBType::REGISTER) || type == uint32_t(ROBType::LINK)) {
          const uint32_t phy = uint32_t(commits[lane].oldPhy);
          if (phy != 0) recPhy[nRec++] = phy;
        }
      }
    }
    PrfSeq curTail = uint32_t(tailSeq);
    for (int k = 0; k < ROB_CAP + 1; ++k) {
      if (uint32_t(k) < nRec) {
        freeList[prfSlot(curTail)] <= recPhy[k];
        curTail = prfSeqNext(curTail);
      }
    }
    if (nRec != 0u) tailSeq <= curTail;
  }

  // These are previous-edge records, never same-cycle CDB-to-read forwarding.
  for (unsigned writer = 0; writer < PRF_VALUE_WRITERS; ++writer) {
    priorWrites[writer].valid <= bool(valuePorts[writer].valid);
    priorWrites[writer].phy <= uint32_t(valuePorts[writer].phy);
    priorWrites[writer].value <= uint32_t(valuePorts[writer].value);
  }
  for (unsigned bank = 0; bank < PRF_CAP - 1; ++bank) {
    const auto write = writeFor(bank + 1);
    if (write.valid) {
      PhysicalRegs[bank + 1].ready <= write.ready;
      PhysicalRegs[bank + 1].everWritten <= true;
    }
    payload[bank].work(); // Idle/frozen banks refresh their single row.
  }
}
