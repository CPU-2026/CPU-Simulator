#include "../include/PRF.hpp"
#include "../include/util.hpp"
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
  bool robWillCommit = static_cast<bool>(rob.robWillCommit);
  bool issueValid = static_cast<bool>(issue.issueValid);
  bool issueAlloc = issueValid && static_cast<bool>(issue.issueAllocDest);
  uint8_t issuePhyVal = static_cast<uint32_t>(issue.issuePhy);
  bool issueIsCtrl = static_cast<bool>(issue.issueIsControl);
  uint32_t issuePCVal = static_cast<uint32_t>(issue.issuePC);

  // ---- Quad-CDB writeback: four independent write ports
  // (aluCDB / lqCDB / mulCDB / divCDB). Register renaming guarantees a distinct
  // newPhy per in-flight instruction, so same-cycle writebacks always hit
  // different physical registers (no WAW; the Register array's per-element
  // single write port is naturally satisfied). Each port keeps its own
  // squash guard, verbatim from the single-CDB era.
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
  bool cdbWriteLq = false;
  uint32_t cdbPhyLq = 0;
  uint32_t cdbValLq = 0;
  if (static_cast<bool>(cdbOfLQ.cdbValid)) {
    RobTag cdbTag = static_cast<uint32_t>(cdbOfLQ.cdbRobTag);
    if (!needSquash || ROB::isOlder(cdbTag, squashTag)) {
      uint32_t newPhy = static_cast<uint32_t>(cdbOfLQ.cdbNewPhy);
      if (newPhy != static_cast<uint32_t>(InvalidPhy)) {
        cdbWriteLq = true;
        cdbPhyLq = newPhy;
        cdbValLq = static_cast<uint32_t>(cdbOfLQ.cdbValue);
      }
    }
  }
  bool cdbWriteMul = false;
  uint32_t cdbPhyMul = 0;
  uint32_t cdbValMul = 0;
  RobTag cdbTagMul = 0;
  if (static_cast<bool>(cdbOfMUL.cdbValid)) {
    RobTag cdbTag = static_cast<uint32_t>(cdbOfMUL.cdbRobTag);
    if (!needSquash || ROB::isOlder(cdbTag, squashTag)) {
      uint32_t newPhy = static_cast<uint32_t>(cdbOfMUL.cdbNewPhy);
      if (newPhy != static_cast<uint32_t>(InvalidPhy)) {
        cdbWriteMul = true;
        cdbPhyMul = newPhy;
        cdbValMul = static_cast<uint32_t>(cdbOfMUL.cdbValue);
        cdbTagMul = cdbTag;
      }
    }
  }

  bool cdbWriteDiv = false;
  uint32_t cdbPhyDiv = 0;
  uint32_t cdbValDiv = 0;
  RobTag cdbTagDiv = 0;
  if (static_cast<bool>(cdbOfDIV.cdbValid)) {
    RobTag cdbTag = static_cast<uint32_t>(cdbOfDIV.cdbRobTag);
    if (!needSquash || ROB::isOlder(cdbTag, squashTag)) {
      uint32_t newPhy = static_cast<uint32_t>(cdbOfDIV.cdbNewPhy);
      if (newPhy != static_cast<uint32_t>(InvalidPhy)) {
        cdbWriteDiv = true;
        cdbPhyDiv = newPhy;
        cdbValDiv = static_cast<uint32_t>(cdbOfDIV.cdbValue);
        cdbTagDiv = cdbTag;
      }
    }
  }

  if (cdbWriteAlu) {
    if (debug::enabled(debug::TOPIC_PRF))
      debug::print("PRF write P%d = %d (aluCDB)\n", cdbPhyAlu, cdbValAlu);
    PhysicalRegs[cdbPhyAlu].ready <= true;
    PhysicalRegs[cdbPhyAlu].value <= cdbValAlu;
  }
  if (cdbWriteLq) {
    if (debug::enabled(debug::TOPIC_PRF))
      debug::print("PRF write P%d = %d (lqCDB)\n", cdbPhyLq, cdbValLq);
    PhysicalRegs[cdbPhyLq].ready <= true;
    PhysicalRegs[cdbPhyLq].value <= cdbValLq;
  }
  if (cdbWriteMul) {
    if (debug::enabled(debug::TOPIC_EXEC))
      debug::print("prf mul-write rob=%u phy=%d val=%08x\n", cdbTagMul,
                   cdbPhyMul, cdbValMul);
    PhysicalRegs[cdbPhyMul].ready <= true;
    PhysicalRegs[cdbPhyMul].value <= cdbValMul;
  }

  if (cdbWriteDiv) {
    if (debug::enabled(debug::TOPIC_EXEC))
      debug::print("prf div-write rob=%u phy=%d val=%08x\n", cdbTagDiv,
                   cdbPhyDiv, cdbValDiv);
    PhysicalRegs[cdbPhyDiv].ready <= true;
    PhysicalRegs[cdbPhyDiv].value <= cdbValDiv;
  }

  // ---- Issue: squash owns recovery; only a non-squash cycle can pop. ----
  uint32_t curHead = static_cast<uint32_t>(headSeq);
  uint32_t nextHead = curHead;
  bool doPop = false;
  uint8_t popPhy = 0;
  if (!needSquash) {
    if (issueValid) {
      if (issueAlloc) {
        assert(curHead != static_cast<uint32_t>(tailSeq));
        popPhy = static_cast<uint32_t>(
            freeList[prfSlot(static_cast<PrfSeq>(curHead))]);
        assert(popPhy != InvalidPhy);
        assert(popPhy == issuePhyVal);
        nextHead = prfSeqNext(static_cast<PrfSeq>(curHead));
        doPop = true;
      }
    }
  }
  if (doPop) {
    headSeq <= nextHead;
    if (issueIsCtrl) {
      if (debug::enabled(debug::TOPIC_PRF))
        debug::print("PRF link P%d = %d (pc+4)\n", issuePhyVal, issuePCVal + 4);
      PhysicalRegs[issuePhyVal].ready <= true;
      PhysicalRegs[issuePhyVal].value <= issuePCVal + 4;
    } else {
      PhysicalRegs[issuePhyVal].ready <= false;
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
  if (needSquash) {
    const RobTag oldNext = static_cast<uint32_t>(rob.robNextTag);
    RobTag tag = robNextTag(squashTag);
    bool scanDone = (tag == oldNext);
    for (int k = 0; k < ROB_CAP; ++k) {
      if (scanDone)
        continue;
      uint32_t recoverPRF = static_cast<uint32_t>(rob.robNewPhy[robSlot(tag)]);
      if (recoverPRF != static_cast<uint32_t>(InvalidPhy)) {
        recPhy[nRec] = recoverPRF;
        nRec = nRec + 1u;
      }
      tag = robNextTag(tag);
      if (tag == oldNext)
        scanDone = true;
    }
  }

  // ---- Commit: independent of squash when the head is strictly older ----
  // (robWillCommit already folds the age guard, same as the reference
  // willCommit(squash)). Its oldPhy is appended to the same recycle list.
  if (robWillCommit && !static_cast<bool>(rob.robHeadIsHalt)) {
    uint32_t hType = static_cast<uint32_t>(rob.robHeadType);
    if (hType == static_cast<uint32_t>(ROBType::REGISTER) ||
        hType == static_cast<uint32_t>(ROBType::LINK)) {
      uint32_t oldPhy = static_cast<uint32_t>(rob.robHeadOldPhy);
      if (oldPhy != static_cast<uint32_t>(InvalidPhy)) {
        recPhy[nRec] = oldPhy;
        nRec = nRec + 1u;
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
