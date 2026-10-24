#include "../include/BPU.hpp"

#include <cstdint>
#include <sys/types.h>

#include "../include/ROB.hpp"

uint32_t BPU::archWriteLane(unsigned lane) const {
  uint32_t top = static_cast<uint32_t>(tgt.archTopOfRAS);
  for (unsigned i = 0; i < COMMIT_WIDTH; ++i) {
    if (i <= lane && bool(commits[i].valid)) {
      if (bool(commits[i].pop) && top > 0) --top;
      if (i < lane && bool(commits[i].push) && top < RAS_CAP) ++top;
    }
  }
  return top;
}

bool BPU::archWrites(unsigned lane) const {
  return bool(commits[lane].valid) && bool(commits[lane].push) && archWriteLane(lane) < RAS_CAP;
}

bool BPU::archWriteEnabled() const {
  bool write = false;
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) write |= archWrites(lane);
  return write;
}

BPU::BPU() {
  wire_output();
  tgt.archStorage.enable = [this]() { return static_cast<bool>(squash.needSquash) || archWriteEnabled(); };
  tgt.archStorage.writeEnable = [this]() { return archWriteEnabled(); };
  tgt.archStorage.addr = []() { return 0u; };
  for (int i = 0; i < RAS_CAP; ++i) {
    tgt.archStorage.writeMask[i] = [this, i]() {
      bool write = false;
      for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
        write |= archWrites(lane) && archWriteLane(lane) == unsigned(i);
      return write;
    };
    tgt.archStorage.writeData[i] = [this, i]() {
      uint32_t pc = 0;
      for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
        if (archWrites(lane) && archWriteLane(lane) == unsigned(i)) pc = uint32_t(commits[lane].pc) + 4u;
      return pc;
    };
  }
}

PredictInfo BPU::predict(uint32_t pc, uint8_t ghr) const {
  const uint32_t p2 = static_cast<uint32_t>(pc) >> 2;
  const uint32_t localIndex = p2 & (BHT_CAP - 1);
  const uint32_t globalIndex = (p2 ^ ghr) & (BHT_CAP - 1);
  const uint32_t selectorIndex = (p2 ^ ghr) & (SELECTOR_CAP - 1);
  const bool useGlobal =
      static_cast<uint32_t>(dir.selector[selectorIndex]) >= 2;
  const bool directionTaken =
      useGlobal ? static_cast<uint32_t>(dir.globalPHT[globalIndex]) >= 2
                : static_cast<uint32_t>(dir.localPHT[localIndex]) >= 2;
  const auto BTB_index = p2 & (BTB_CAP - 1);
  bool btbHit = static_cast<bool>(tgt.BTB[BTB_index].state) &&
                static_cast<uint32_t>(tgt.BTB[BTB_index].tag) ==
                    static_cast<uint32_t>(pc >> 8);
  bool taken = btbHit && directionTaken;
  if (btbHit && static_cast<uint32_t>(tgt.BTB[BTB_index].state) >= 2)
    taken = true;
  // RET with empty RAS: don't use BTB target 0, treat as not taken (wild fetch
  // fix)
  bool isRet = static_cast<uint32_t>(tgt.BTB[BTB_index].state) == 3;
  bool rasEmpty = static_cast<uint32_t>(tgt.specTopOfRAS) == 0;
  if (isRet && rasEmpty) {
    btbHit = false;
    taken = false;
  }
  // uint32 bit-vector add: signed uint32_t add past the range is host UB.
  uint32_t predictPC = pc + 4u;
  if (taken && btbHit) {
    if (isRet && static_cast<uint32_t>(tgt.specTopOfRAS) > 0)
      predictPC = static_cast<uint32_t>(
          tgt.specRAS[static_cast<uint32_t>(tgt.specTopOfRAS) - 1].retPC);
    else
      predictPC = static_cast<uint32_t>(tgt.BTB[BTB_index].target) << 2;
  }
  PredictInfo out{taken, predictPC};
  out.btbHit = btbHit;
  out.unconditional =
      btbHit && static_cast<uint32_t>(tgt.BTB[BTB_index].state) >= 2;
  out.condSeen = static_cast<bool>(tgt.condSeen[p2 & (CONDSEEN_CAP - 1)]);
  return out;
}

// Fetch-direction guard (retired FetchDecision::build); the squash source is
// flushArbiter.needSquash, as the former arbitResult() bridge read.
bool BPU::fetchAllowed() const {
  return !static_cast<bool>(fetchCtx.squashNeed) &&
         !static_cast<bool>(fetchCtx.haltFetched) &&
         static_cast<bool>(fetchCtx.fqCanAccept) &&
         static_cast<bool>(fetchCtx.memCanAccept);
}

// ---- fetch-stage prediction bundle (retired FetchDecision::build). ----
//      lane0 = fetchCtx.pc, lane1 = pc+4 when ISSUE_WIDTH == 2 and lane0
//      falls through within the same I$ line. Both lanes share one checkpoint;
//      the GHR advances per predicted-taken conditional in the group. ----
BPU::FetchGroup BPU::fetchGroup() const {
  FetchGroup g;
  if (!fetchAllowed())
    return g;
  const uint8_t ghr0 = static_cast<uint8_t>(static_cast<uint32_t>(dir.GHR));
  const uint32_t pc0 = static_cast<uint32_t>(fetchCtx.pc);
  const PredictInfo p0 = predict(pc0, ghr0);
  g.valid0 = true;
  g.pc0 = pc0;
  g.predPC0 = static_cast<uint32_t>(p0.predictPC);
  g.shift0 = p0.btbHit || p0.condSeen;
  g.shiftValue0 = p0.btbHit ? (p0.unconditional ? true : p0.taken)
                            : (p0.condSeen ? p0.taken : false);
  const bool crossLine = (pc0 & 0x3Cu) == 0x3Cu;
  if (ISSUE_WIDTH >= 2 && !g.shift0 && !crossLine &&
      static_cast<bool>(fetchCtx.fqCanAccept2)) {
    const uint32_t pc1 = pc0 + 4u;
    const uint8_t ghr1 = g.shift0
                             ? static_cast<uint8_t>(
                                   (((static_cast<uint32_t>(ghr0) << 1) |
                                     (g.shiftValue0 ? 1u : 0u)) &
                                    HISTORY_MASK))
                             : ghr0;
    const PredictInfo p1 = predict(pc1, ghr1);
    g.valid1 = true;
    g.pc1 = pc1;
    g.predPC1 = static_cast<uint32_t>(p1.predictPC);
    g.shift1 = p1.btbHit || p1.condSeen;
    g.shiftValue1 = p1.btbHit ? (p1.unconditional ? true : p1.taken)
                              : (p1.condSeen ? p1.taken : false);
    g.nextPC = g.predPC1;
  } else {
    g.nextPC = g.predPC0;
  }
  return g;
}

void BPU::wire_output() {
  outValid = [this]() -> uint32_t {
    return fetchGroup().valid0 ? 1u : 0u;
  };
  outValid1 = [this]() -> uint32_t {
    return fetchGroup().valid1 ? 1u : 0u;
  };
  outPC = [this]() -> uint32_t { return fetchGroup().pc0; };
  outPC1 = [this]() -> uint32_t { return fetchGroup().pc1; };
  outPredictedPC = [this]() -> uint32_t { return fetchGroup().predPC0; };
  outPredictedPC1 = [this]() -> uint32_t { return fetchGroup().predPC1; };
  outPredPC = [this]() -> uint32_t { return fetchGroup().nextPC; };
  outShift = [this]() -> uint32_t { return fetchGroup().shift0 ? 1u : 0u; };
  outShiftValue = [this]() -> uint32_t { return fetchGroup().shiftValue0 ? 1u : 0u; };
  outShift1 = [this]() -> uint32_t { return fetchGroup().shift1 ? 1u : 0u; };
  outShiftValue1 = [this]() -> uint32_t { return fetchGroup().shiftValue1 ? 1u : 0u; };
  outCkptId = [this]() -> uint32_t {
    return fetchGroup().valid0 ? (static_cast<uint32_t>(getNextCkptId()) & (CKPT_CAP - 1)) : 0u;
  };
}

BPUSnapshot BPU::snapshotCheckPoint() const {
  BPUSnapshot ckptSnap;
  ckptSnap.GHR_snapshot = getGHR();
  return ckptSnap;
}

// ---- training requests (decoded once at the top of work()) ----
struct TrainReq {
  bool valid = false;
  bool isJump = false; // updateJump vs update
  bool taken = false;
  bool isRet = false;
  uint32_t pc = 0;
  uint32_t target = 0;
  uint8_t ghr = 0;
};

// ---- per-cycle BTB write intents: one fixed-shape port per training source
//      (fetch / cdb / bru), valid-gated; replaces the runtime-length plan ----
struct BTBWriteIntent {
  bool lineWrite = false;   // actualPC + valid + unconditional + isRet
  bool targetWrite = false; // target write enable (fetch w/o static JAL target)
  uint32_t index = 0;       // (pc >> 2) & (BTB_CAP - 1)
  uint32_t actualPC = 0;
  uint32_t target = 0;
  bool unconditional = false;
  bool isRet = false;
};

// Resolved JAL/JALR transfer (CDB port, or the BRU jump path).
BTBWriteIntent jumpWriteIntent(const TrainReq &req) {
  BTBWriteIntent intent;
  intent.lineWrite = true;
  intent.targetWrite = true;
  intent.index = (req.pc >> 2) & (BTB_CAP - 1);
  intent.actualPC = req.pc;
  intent.target = req.target;
  intent.unconditional = true;
  intent.isRet = req.isRet;
  return intent;
}

// True when both ports hit the same BTB line this cycle; the lower-priority
// port yields (fetch > cdb > bru) at the write point.
bool sameBTBLine(const BTBWriteIntent &first, const BTBWriteIntent &second) {
  return first.lineWrite && second.lineWrite && first.index == second.index;
}

bool robTagMatches(const BPUInputROB &rob, RobTag tag) {
  if (static_cast<bool>(rob.isROBEmpty))
    return false;
  const uint32_t slot = robSlot(tag);
  const uint32_t head = static_cast<uint32_t>(rob.robHeadTag);
  const uint32_t span = (static_cast<uint32_t>(rob.robNextTag) - head) & ROB_TAG_MASK;
  const uint32_t offset = (uint32_t(tag) - head) & ROB_TAG_MASK;
  return slot < ROB_CAP && offset < span &&
         static_cast<uint32_t>(rob.robTag[slot]) == tag;
}
void BPU::work() {
  tgt.archStorage.work();
  const auto step = recovery.sample();
  // Cycle-0 boot: direction counters need non-zero init. Runs in parallel with
  // normal logic -- cycle 0 has ROB/BRU empty, so no training can race it.
  bool boot = !static_cast<bool>(bootDone);
  if (boot) {
    for (int i = 0; i < BHT_CAP; ++i) {
      dir.localPHT[i] <= 1;
      dir.globalPHT[i] <= 1;
    }
    for (int i = 0; i < SELECTOR_CAP; ++i)
      dir.selector[i] <= 1;
    bootDone <= true;
  }

  bool needSquash = static_cast<bool>(squash.needSquash);
  RobTag squashTag = static_cast<uint32_t>(squash.SquashTag);
  uint32_t squashCkpt = static_cast<uint32_t>(squash.SquashCkpt);

  // ---- decode both train requests (once per cycle) ----
  TrainReq trBru, trCdb;
  const RobTag brRobTag = static_cast<uint32_t>(bru.bruHeadRobTag);
  if (!step.active && !static_cast<bool>(bru.isBRUEmpty) && robTagMatches(rob, brRobTag)) {
    auto pcResult = static_cast<uint32_t>(bru.bruHeadPCResult);
    auto pcFrom = static_cast<uint32_t>(bru.bruHeadPCFrom);
    if (!needSquash || ROB::isOlder(brRobTag, squashTag)) {
      ++branchTotal;
      bool correct =
          pcResult == static_cast<uint32_t>(rob.robPredictPC[robSlot(brRobTag)]);
      if (correct)
        ++branchCorrect;
      trBru.valid = true;
      trBru.isJump = false;
      trBru.pc = pcFrom;
      trBru.taken = static_cast<bool>(bru.bruHeadActualTaken);
      trBru.target = pcResult;
      auto cid = static_cast<uint32_t>(rob.robCkptId[robSlot(brRobTag)]);
      // Checkpoints are written only by fetch allocation and read by squash;
      // same-cycle allocation and rollback use distinct IDs.
      trBru.ghr = static_cast<uint8_t>(static_cast<uint32_t>(bpCkpt[cid].GHR));
    }
  }
  const RobTag cdbRobTag = static_cast<uint32_t>(cdb.cdbRobTag);
  if (static_cast<bool>(cdb.cdbValid) && static_cast<bool>(cdb.cdbIsControl) &&
      robTagMatches(rob, cdbRobTag)) {
    auto robIdx = robSlot(cdbRobTag);
    auto pc = static_cast<uint32_t>(cdb.cdbValue);
    if (!needSquash ||
        ROB::isOlder(static_cast<uint32_t>(cdb.cdbRobTag), squashTag)) {
      ++branchTotal;
      bool correct = pc == static_cast<uint32_t>(rob.robPredictPC[robIdx]);
      if (correct)
        ++branchCorrect;
      trCdb.valid = true;
      trCdb.isJump = true;
      trCdb.pc = static_cast<uint32_t>(rob.robPC[robIdx]);
      trCdb.target = pc;
      trCdb.isRet = static_cast<bool>(rob.robIsRet[robIdx]);
    }
  }

  // ---- write intents from the same cycle-start register state (Register
  //      reads return _M_old, so ports cannot bypass each other) ----
  BTBWriteIntent bruWriteIntent, cdbWriteIntent;
  if (trBru.valid) {
    if (trBru.isJump) {
      bruWriteIntent = jumpWriteIntent(trBru);
    } else {
      const uint32_t p2 = trBru.pc >> 2;
      const uint32_t localIndex = p2 & (BHT_CAP - 1);
      const uint32_t globalIndex = (p2 ^ trBru.ghr) & (BHT_CAP - 1);
      const uint32_t selectorIndex = (p2 ^ trBru.ghr) & (SELECTOR_CAP - 1);
      uint32_t local = static_cast<uint32_t>(dir.localPHT[localIndex]);
      uint32_t global = static_cast<uint32_t>(dir.globalPHT[globalIndex]);
      const bool localPred = local >= 2;
      const bool globalPred = global >= 2;
      if (trBru.taken) {
        if (local < 3)
          ++local;
        if (global < 3)
          ++global;
      } else {
        if (local > 0)
          --local;
        if (global > 0)
          --global;
      }
      dir.localPHT[localIndex] <= local;
      dir.globalPHT[globalIndex] <= global;

      if (globalPred == trBru.taken && localPred != trBru.taken) {
        uint32_t choice = static_cast<uint32_t>(dir.selector[selectorIndex]);
        if (choice < 3)
          ++choice;
        dir.selector[selectorIndex] <= choice;
      } else if (localPred == trBru.taken && globalPred != trBru.taken) {
        uint32_t choice = static_cast<uint32_t>(dir.selector[selectorIndex]);
        if (choice > 0)
          --choice;
        dir.selector[selectorIndex] <= choice;
      }

      tgt.condSeen[p2 & (CONDSEEN_CAP - 1)] <= 1;

      if (trBru.taken) {
        const uint32_t btbIndex = p2 & (BTB_CAP - 1);
        bruWriteIntent.lineWrite = true;
        bruWriteIntent.targetWrite = true;
        bruWriteIntent.index = btbIndex;
        bruWriteIntent.actualPC = trBru.pc;
        bruWriteIntent.target = trBru.target;
        bruWriteIntent.unconditional = false;
        bruWriteIntent.isRet = false;
      }
    }
  }
  if (trCdb.valid)
    cdbWriteIntent = jumpWriteIntent(trCdb);

  // ---- fetch allocation (BRU-port speculative: bpCkpt/GHR/nextCkptId) ----
  uint8_t ghrLocal = static_cast<uint32_t>(dir.GHR);
  uint32_t nextCkpt = static_cast<uint32_t>(nextCkptId);
  if (static_cast<bool>(outValid)) {
    auto ckid = static_cast<uint32_t>(outCkptId);
    BPUSnapshot ckptSnap = snapshotCheckPoint();
    bpCkpt[ckid].GHR <= ckptSnap.GHR_snapshot;
    if (static_cast<bool>(outShift))
      ghrLocal =
          static_cast<uint8_t>(((static_cast<uint32_t>(ghrLocal) << 1) |
                                (static_cast<bool>(outShiftValue) ? 1u : 0u)) &
                               HISTORY_MASK);
    if (static_cast<bool>(outValid1) && static_cast<bool>(outShift1))
      ghrLocal =
          static_cast<uint8_t>(((static_cast<uint32_t>(ghrLocal) << 1) |
                                (static_cast<bool>(outShiftValue1) ? 1u : 0u)) &
                               HISTORY_MASK);
    nextCkpt = (ckid + 1) & (CKPT_CAP - 1);
  }

  // ---- Arch + speculative RAS local mirror (mirrors the main tree) ----
  // specRAS is the prediction state; archRAS is the commit-time baseline.
  // The tops are non-wrapping depths in [0, RAS_CAP].
  uint32_t specTop = static_cast<uint32_t>(tgt.specTopOfRAS);
  uint32_t archTop = static_cast<uint32_t>(tgt.archTopOfRAS);
  uint32_t specRAS[RAS_CAP];
  for (int i = 0; i < RAS_CAP; ++i) {
    specRAS[i] = static_cast<uint32_t>(tgt.specRAS[i].retPC);
  }

  BTBWriteIntent fetchWriteIntent;
  if (!step.active) {
    // RAS maintenance runs for every fetched jump in the group, in program
    // order (lane0 then lane1) so return prediction stays exact. BTB early
    // training uses the single fetch port (lane0 only): a lane1 jump is still
    // trained at resolve time, costing at most one extra mispredict.
    auto rasFetch = [&](bool valid, bool isRet, bool isCall, uint32_t pc) {
      if (!valid)
        return;
      if (!needSquash && isRet && specTop > 0)
        --specTop;
      if (!needSquash && isCall && specTop < RAS_CAP)
        specRAS[specTop++] = pc + 4u;
    };
    rasFetch(static_cast<bool>(fetchInfo.FetchValid),
             static_cast<bool>(fetchInfo.isFetchRet),
             static_cast<bool>(fetchInfo.isFetchCall),
             static_cast<uint32_t>(fetchInfo.FetchPC));
    if (ISSUE_WIDTH >= 2)
      rasFetch(static_cast<bool>(fetchInfo.FetchValid1),
               static_cast<bool>(fetchInfo.isFetchRet1),
               static_cast<bool>(fetchInfo.isFetchCall1),
               static_cast<uint32_t>(fetchInfo.FetchPC1));

    // BTB early training: with !shift0, at most one lane per group is a jump
    // (a lane0 jump degrades lane1), so a single fetch write port suffices.
    // Prefer lane0; a lane1 jump is trained here only when lane0 is not.
    auto btbFetch = [&](bool valid, bool isRet, bool jalTargetValid,
                        uint32_t pc, uint32_t jalTarget) {
      if (!valid)
        return;
      fetchWriteIntent.lineWrite = true;
      fetchWriteIntent.index = (pc >> 2) & (BTB_CAP - 1);
      fetchWriteIntent.actualPC = pc;
      fetchWriteIntent.unconditional = true;
      fetchWriteIntent.isRet = isRet;
      fetchWriteIntent.targetWrite = !isRet && jalTargetValid;
      fetchWriteIntent.target = jalTarget;
    };
    if (static_cast<bool>(fetchInfo.FetchValid))
      btbFetch(true, static_cast<bool>(fetchInfo.isFetchRet),
               static_cast<bool>(fetchInfo.FetchJALTargetValid),
               static_cast<uint32_t>(fetchInfo.FetchPC),
               static_cast<uint32_t>(fetchInfo.FetchJALTarget));
    else if (ISSUE_WIDTH >= 2)
      btbFetch(static_cast<bool>(fetchInfo.FetchValid1),
               static_cast<bool>(fetchInfo.isFetchRet1),
               static_cast<bool>(fetchInfo.FetchJALTargetValid1),
               static_cast<uint32_t>(fetchInfo.FetchPC1),
               static_cast<uint32_t>(fetchInfo.FetchJALTarget1));
  }

  // Pre-commit architectural baseline: like the main tree tick() (which reads
  // the comb snapshot `tgt` for recovery), a same-cycle squash must replay
  // from the OLD committed state, ignoring this cycle's head commit.

  // Commit is the architectural RAS baseline used for later ROB replay
  // (mirrors the main tree; CDB never participates).
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
    if (bool(commits[lane].valid)) {
      if (bool(commits[lane].pop) && archTop > 0) --archTop;
      if (bool(commits[lane].push) && archTop < RAS_CAP) ++archTop;
    }
  }
  for (unsigned row = 0; row < RAS_CAP; ++row)
    if (bool(tgt.archStorage.writeEnable) && bool(tgt.archStorage.writeMask[row])) tgt.archValid[row] <= 1;

  // Checkpoint allocation and squash rollback never coincide: the allocated
  // ckptId is always newer (ring distance >= 1), so bpCkpt reads never race.
  dark::debug::assert(!(static_cast<bool>(outValid) && needSquash),
                      "fetch-alloc and squash-rollback in the same cycle");
  // ---- squash restore (highest priority over all speculative state) ----
  // Restore the old committed baseline, then replay every surviving ROB
  // entry through the squash instruction itself (mirrors the main tree).
  if (needSquash) {
    uint32_t ckid = squashCkpt & (CKPT_CAP - 1);
    uint8_t ckptGHR = static_cast<uint32_t>(bpCkpt[ckid].GHR);
    dark::debug::assert(robTagMatches(rob, squashTag),
                        "squash tag must be live in the ROB");
    tgt.baselineTop <= static_cast<uint32_t>(tgt.archTopOfRAS);
    ghrLocal = ckptGHR;
    nextCkpt = (ckid + 1) & (CKPT_CAP - 1);
  }
  if (step.active) {
    specTop = step.first() ? static_cast<uint32_t>(tgt.baselineTop) : specTop;
    if (step.first()) for (int i = 0; i < RAS_CAP; ++i)
      specRAS[i] = static_cast<bool>(tgt.archValid[i]) ? tgt.archStorage.readLane(i) : 0u;
    bool open = step.first() || static_cast<bool>(tgt.replayOpen);
    RobTag tag = step.nextTag;
    for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) {
      const unsigned offset = unsigned(step.offset) + lane;
      if (open && offset < ROB_CAP && offset < step.liveCount) {
        if (static_cast<uint32_t>(replay[lane].tag) == tag) {
          if (static_cast<bool>(replay[lane].pop) && specTop > 0) --specTop;
          if (static_cast<bool>(replay[lane].push) && specTop < RAS_CAP)
            specRAS[specTop++] = static_cast<uint32_t>(replay[lane].pc) + 4u;
          if (tag == step.squashTag) open = false;
        } else open = false;
      }
      tag = robNextTag(tag);
    }
    tgt.replayOpen <= open;
  }

  // ---- BTB write-port arbitration: fetch > cdb > bru. The line fields and
  //      target are arbitrated separately: a fetch write without a static JAL
  //      target must not shadow a same-line target trained by cdb/bru. ----
  auto writeBTBLine = [&](const BTBWriteIntent &intent) {
    tgt.BTB[intent.index].tag <= (intent.actualPC >> 8);
    bool isUnconditional = intent.unconditional;
    bool isRet = intent.isRet;
    tgt.BTB[intent.index].state <= (isRet ? 3 : (isUnconditional ? 2 : 1));
  };
  const bool fetchCdbSameLine = sameBTBLine(fetchWriteIntent, cdbWriteIntent);
  const bool fetchBruSameLine = sameBTBLine(fetchWriteIntent, bruWriteIntent);
  const bool cdbBruSameLine = sameBTBLine(cdbWriteIntent, bruWriteIntent);
  const bool fetchTargetBlocksCdb = fetchWriteIntent.lineWrite &&
                                    fetchWriteIntent.targetWrite &&
                                    fetchCdbSameLine;
  const bool fetchTargetBlocksBru = fetchWriteIntent.lineWrite &&
                                    fetchWriteIntent.targetWrite &&
                                    fetchBruSameLine;

  if (fetchWriteIntent.lineWrite)
    writeBTBLine(fetchWriteIntent);
  if (cdbWriteIntent.lineWrite && !fetchCdbSameLine)
    writeBTBLine(cdbWriteIntent);
  if (bruWriteIntent.lineWrite && !fetchBruSameLine && !cdbBruSameLine)
    writeBTBLine(bruWriteIntent);

  if (fetchWriteIntent.lineWrite && fetchWriteIntent.targetWrite)
    tgt.BTB[fetchWriteIntent.index].target <=
        (fetchWriteIntent.target >> 2);
  if (cdbWriteIntent.lineWrite && !fetchTargetBlocksCdb)
    tgt.BTB[cdbWriteIntent.index].target <= (cdbWriteIntent.target >> 2);
  if (bruWriteIntent.lineWrite && !fetchTargetBlocksBru && !cdbBruSameLine)
    tgt.BTB[bruWriteIntent.index].target <= (bruWriteIntent.target >> 2);

  // Speculative-state writeback (squash > BRU; CDB never participates).
  // archRAS/archTop advance only on commit above; squash replays into spec.
  dir.GHR <= ghrLocal;
  nextCkptId <= nextCkpt;
  tgt.specTopOfRAS <= specTop;
  tgt.archTopOfRAS <= archTop;

  for (int i = 0; i < RAS_CAP; ++i) {
    tgt.specRAS[i].retPC <= specRAS[i];
  }
}
