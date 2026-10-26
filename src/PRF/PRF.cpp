#include "../include/PRF.hpp"
#include "ROB.hpp"
#include <cassert>
#include <cstdint>
void PRF::work() {
  // ---- bootDone reset: single-cycle init, overlaps cycle 0 (ROB empty => no
  // issue/squash) ----
  if (!static_cast<bool>(bootDone)) {
    for (int i = 0; i < REGISTER_CAP; ++i)
      PhysicalRegs[i].ready <= true;
    for (int i = 0; i < PRF_CAP - REGISTER_CAP; ++i)
      freeList[i] <= static_cast<uint32_t>(REGISTER_CAP + i);
    tailSeq <= static_cast<uint32_t>(PRF_CAP - REGISTER_CAP);
    headSeq <= static_cast<uint32_t>(0);
    bootDone <= true;
  }
  // Cache frequently used Wire values as local combinational signals
  bool needSquash = static_cast<bool>(squash.needSquash);
  RobTag squashTag = static_cast<uint32_t>(squash.SquashTag);
  bool issueValid[ISSUE_WIDTH];
  bool issueAlloc[ISSUE_WIDTH];
  uint8_t issuePhyVal[ISSUE_WIDTH];
  bool issueIsCtrl[ISSUE_WIDTH];
  uint32_t issuePCVal[ISSUE_WIDTH];
  for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
    issueValid[L] = static_cast<bool>(issue[L].issueValid);
    issueAlloc[L] = issueValid[L] && static_cast<bool>(issue[L].issueAllocDest);
    issuePhyVal[L] = static_cast<uint32_t>(issue[L].issuePhy);
    issueIsCtrl[L] = static_cast<bool>(issue[L].issueIsControl);
    issuePCVal[L] = static_cast<uint32_t>(issue[L].issuePC);
  }

  // Two result write ports. Renaming keeps their destinations distinct.
  bool cdbWriteAlu = false;
  uint32_t cdbPhyAlu = 0;
  uint32_t cdbValAlu = 0;
  if (static_cast<bool>(cdbOfALU.cdbValid)) {
    RobTag cdbTag = static_cast<uint32_t>(cdbOfALU.cdbRobTag);
    if (!needSquash || ROB::isOlder(cdbTag, squashTag)) {
      if (!static_cast<bool>(cdbOfALU.cdbIsControl)) {
        uint32_t newPhy = static_cast<uint32_t>(cdbOfALU.cdbNewPhy);
        if (newPhy != static_cast<uint32_t>(InvalidPhy)) {
          cdbWriteAlu = true;
          cdbPhyAlu = newPhy;
          cdbValAlu = static_cast<uint32_t>(cdbOfALU.cdbValue);
        }
      }
    }
  }
  bool cdbWriteShared = false;
  uint32_t cdbPhyShared = 0;
  uint32_t cdbValShared = 0;
  if (static_cast<bool>(cdbOfShared.cdbValid)) {
    RobTag cdbTag = static_cast<uint32_t>(cdbOfShared.cdbRobTag);
    if (!needSquash || ROB::isOlder(cdbTag, squashTag)) {
      uint32_t newPhy = static_cast<uint32_t>(cdbOfShared.cdbNewPhy);
      if (newPhy != static_cast<uint32_t>(InvalidPhy)) {
        cdbWriteShared = true;
        cdbPhyShared = newPhy;
        cdbValShared = static_cast<uint32_t>(cdbOfShared.cdbValue);
      }
    }
  }

  if (cdbWriteAlu) {
    PhysicalRegs[cdbPhyAlu].ready <= true;
    PhysicalRegs[cdbPhyAlu].value <= cdbValAlu;
  }
  if (cdbWriteShared) {
    PhysicalRegs[cdbPhyShared].ready <= true;
    PhysicalRegs[cdbPhyShared].value <= cdbValShared;
  }

  // ---- Issue: squash owns recovery; only a non-squash cycle can pop. ----
  uint32_t curHead = static_cast<uint32_t>(headSeq);
  uint32_t nextHead = curHead;
  unsigned popCount = 0;
  if (!needSquash && !static_cast<bool>(recovery.active)) {
    for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
      if (issueAlloc[L]) {
        assert(hasFree(popCount + 1u));
        const uint8_t popPhy = static_cast<uint32_t>(
            freeList[prfSlot(static_cast<PrfSeq>(nextHead))]);
        assert(popPhy != InvalidPhy);
        assert(popPhy == issuePhyVal[L]);
        nextHead = prfSeqNext(static_cast<PrfSeq>(nextHead));
        ++popCount;
      }
    }
  }
  if (popCount != 0u) {
    headSeq <= nextHead;
    for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
      if (issueAlloc[L]) {
        if (issueIsCtrl[L]) {
          PhysicalRegs[issuePhyVal[L]].ready <= true;
          PhysicalRegs[issuePhyVal[L]].value <= issuePCVal[L] + 4;
        } else {
          PhysicalRegs[issuePhyVal[L]].ready <= false;
        }
      }
    }
  }

  // ---- Squash recovery + commit release: gather recycled phys, apply once.
  // Register discipline: tailSeq is written at most once per cycle and each
  // freeList element at most once (slots are distinct: at most ROB_CAP+1
  // pushes, well below one PRF ring). Order matches the reference tick:
  // flushed newPhy range (SquashTag, oldNext) first, commit oldPhy last.
  // No liveness check on SquashTag: the broadcast tag is live by
  // construction -- the FlushArbiter queue clears the broadcast tag itself
  // every cycle it fires (strict !isOlder clear), and both detection stages
  // only insert matchesTag-live tags, all read from the same ROB snapshot.
  uint32_t recPhy[ROB_CAP + 1];
  uint32_t nRec = 0;
  const auto step = recovery.sample();
  if (step.active) {
    RobTag tag = step.nextTag;
    for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) {
      const unsigned offset = unsigned(step.offset) + lane;
      if (offset < ROB_CAP && offset < step.liveCount &&
          static_cast<uint32_t>(replay[lane].tag) == tag && ROB::isYounger(tag, step.squashTag)) {
        const uint32_t phy = static_cast<uint32_t>(replay[lane].newPhy);
        if (phy != 0) recPhy[nRec++] = phy;
      }
      tag = robNextTag(tag);
    }
  }

  // ---- Commit: independent of squash when the head is strictly older ----
  // (robWillCommit already folds the age guard, same as the reference
  // willCommit(squash)). Its oldPhy is appended to the same recycle list.
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
   if (bool(commits[lane].valid) && !bool(commits[lane].halt)) {
    uint32_t hType = uint32_t(commits[lane].type);
    if (hType == static_cast<uint32_t>(ROBType::REGISTER) ||
        hType == static_cast<uint32_t>(ROBType::LINK)) {
      uint32_t oldPhy = uint32_t(commits[lane].oldPhy);
      if (oldPhy != static_cast<uint32_t>(InvalidPhy)) {
        recPhy[nRec] = oldPhy;
        nRec = nRec + 1u;
      }
    }
  }
  
  }
  PrfSeq curTail = static_cast<PrfSeq>(static_cast<uint32_t>(tailSeq));
  for (int k = 0; k < ROB_CAP + 1; ++k) {
    if (static_cast<uint32_t>(k) >= nRec)
      continue;
    freeList[prfSlot(curTail)] <= recPhy[k];
    curTail = prfSeqNext(curTail);
  }
  if (nRec != 0u)
    tailSeq <= static_cast<uint32_t>(curTail);
}
