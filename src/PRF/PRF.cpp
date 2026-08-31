#include "../include/PRF.hpp"
#include "ROB.hpp"
#include "../include/util.hpp"
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
  uint32_t squashTag = static_cast<uint32_t>(squash.SquashTag);
  uint8_t ckptId = static_cast<uint32_t>(squash.CkptId);
  uint32_t lqHeadVal = static_cast<uint32_t>(lq.lqHead);
  uint32_t robHeadVal = static_cast<uint32_t>(rob.robHead);
  bool isRobEmpty = static_cast<bool>(rob.isRobEmpty);
  bool isHeadReady = static_cast<bool>(rob.isRobHeadCommitReady);
  bool issueValid = static_cast<bool>(issue.issueValid);
  bool issueAlloc = issueValid && static_cast<bool>(issue.issueAllocDest);
  uint8_t issuePhyVal = static_cast<uint32_t>(issue.issuePhy);
  uint8_t issueCkpt = static_cast<uint32_t>(issue.issueCkptId);
  bool issueIsCtrl = static_cast<bool>(issue.issueIsControl);
  uint32_t issuePCVal = static_cast<uint32_t>(issue.issuePC);

  // ---- CDB writeback: resolve the single write port first ----
  bool cdbWrite = false;
  uint32_t cdbPhy = 0;
  uint32_t cdbVal = 0;
  if (static_cast<bool>(cdb.cdbValid)) {
    uint32_t cdbTag = static_cast<uint32_t>(cdb.cdbRobTag);
    if (!needSquash || ROB::isOlder(cdbTag, squashTag)) {
      if (!static_cast<bool>(cdb.cdbIsControl)) {
        uint32_t newPhy = static_cast<uint32_t>(cdb.cdbNewPhy);
        if (newPhy != static_cast<uint32_t>(InvalidPhy)) {
          cdbWrite = true;
          cdbPhy = newPhy;
          cdbVal = static_cast<uint32_t>(cdb.cdbValue);
        }
      }
    }
  }

  // ---- LQ load writeback: same commit guard as ROB. A slot the CDB port
  // serves this cycle is skipped: both carry the same value for the same phy
  // (main tree wrote it twice on plain ints; a Register has one write port),
  // so one drive is net-identical.
  for (int k = 0; k < MEMQ_SCAN_WINDOW; ++k) {
    uint8_t i = (lqHeadVal + k) & 0x0F;
    if (!static_cast<bool>(lq.lqActive[i]))
      continue;
    if (!static_cast<bool>(lq.lqReadyToCommit[i]))
      continue;
    if (static_cast<bool>(lq.lqHasOlderUnresolved[i]))
      continue;
    uint32_t lqTag = static_cast<uint32_t>(lq.lqRobTags[i]);
    if (needSquash && !ROB::isOlder(lqTag, squashTag))
      continue;
    if (!isRobEmpty && ROB::isOlder(lqTag, robHeadVal))
      continue;
    uint32_t newPhy = static_cast<uint32_t>(lq.lqNewPhys[i]);
    if (newPhy == static_cast<uint32_t>(InvalidPhy))
      continue;
    if (cdbWrite && newPhy == cdbPhy)
      continue;
    if (debug::enabled(debug::TOPIC_PRF))
      debug::print("PRF write P%d = %d (lq)\n", newPhy,
                   static_cast<uint32_t>(lq.lqValues[i]));
    PhysicalRegs[newPhy].ready <= true;
    PhysicalRegs[newPhy].value <= static_cast<uint32_t>(lq.lqValues[i]);
  }

  if (cdbWrite) {
    if (debug::enabled(debug::TOPIC_PRF))
      debug::print("PRF write P%d = %d (cdb)\n", cdbPhy, cdbVal);
    PhysicalRegs[cdbPhy].ready <= true;
    PhysicalRegs[cdbPhy].value <= cdbVal;
  }

  // ---- Issue: PRFHeadCkpt snapshot + free-list pop ----
  // Single-write-point for headSeq: compute next value, apply once at end.
  uint32_t curHead = static_cast<uint32_t>(headSeq);
  uint32_t nextHead = curHead;
  bool doPop = false;
  uint8_t popPhy = 0;
  if (issueValid) {
    uint32_t headSnap = curHead + (issueAlloc ? 1 : 0);
    PRFHeadCkpt[issueCkpt] <= headSnap;
    if (issueAlloc) {
      popPhy = static_cast<uint32_t>(freeList[curHead & (PRF_CAP - 1)]);
      assert(popPhy != InvalidPhy);
      assert(popPhy == issuePhyVal);
      nextHead = curHead + 1;
      doPop = true;
    }
  }

  // ---- Pop side effects (ready/link value): identical under squash or not,
  // applied once here (reference tick does pop before the squash check) ----
  if (doPop) {
    if (issueIsCtrl) {
      if (debug::enabled(debug::TOPIC_PRF))
        debug::print("PRF link P%d = %d (pc+4)\n", issuePhyVal, issuePCVal + 4);
      PhysicalRegs[issuePhyVal].ready <= true;
      PhysicalRegs[issuePhyVal].value <= issuePCVal + 4;
    } else {
      PhysicalRegs[issuePhyVal].ready <= false;
    }
  }

  // ---- Squash: single-write-point for headSeq, handles PRFHeadCkpt hazard
  // ----
  if (needSquash) {
      uint32_t ckptVal;
      if (issueValid && ckptId == issueCkpt) {
        // Same-cycle write-read hazard: use newly computed headSnap
        ckptVal = curHead + (issueAlloc ? 1 : 0);
      } else {
        ckptVal = static_cast<uint32_t>(PRFHeadCkpt[ckptId]);
      }
      assert(static_cast<uint32_t>(tailSeq) - ckptVal <=
             static_cast<uint32_t>(PRF_CAP));
      nextHead = ckptVal;
    // doPop implies nextHead != curHead; restore overwrites nextHead
    if (nextHead != curHead)
      headSeq <= nextHead;
  } else {
    if (doPop)
      headSeq <= nextHead;

    // ---- Commit: push oldPhy ----
    bool doReturn = false;
    if (isRobEmpty || !isHeadReady)
      doReturn = true;
    if (static_cast<bool>(rob.robHeadIsHalt))
      doReturn = true;
    uint32_t hType = static_cast<uint32_t>(rob.robHeadType);
    if (hType != static_cast<uint32_t>(ROBType::REGISTER) &&
        hType != static_cast<uint32_t>(ROBType::LINK))
      doReturn = true;
    if (!doReturn) {
      uint32_t oldPhy = static_cast<uint32_t>(rob.robHeadOldPhy);
      if (oldPhy != static_cast<uint32_t>(InvalidPhy)) {
        uint32_t tail = static_cast<uint32_t>(tailSeq);
        freeList[tail & (PRF_CAP - 1)] <= oldPhy;
        tailSeq <= tail + 1;
      }
    }
  }
}
