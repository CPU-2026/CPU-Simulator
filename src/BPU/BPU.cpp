#include "../include/BPU.hpp"

#include <cstdint>
#include <sys/types.h>

#include "../include/ROB.hpp"

PredictInfo BPU::predict(uint32_t pc) const {
  const uint32_t p2 = static_cast<uint32_t>(pc) >> 2;
  const uint8_t ghr = static_cast<uint32_t>(dir.GHR);
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
         !static_cast<bool>(fetchCtx.fqFull) &&
         !static_cast<bool>(fetchCtx.imemReqFull);
}

// ---- fetch-stage prediction bundle (retired FetchDecision::build).
//      Each out net calls predict() once; re-runs stay bit-identical. ----
void BPU::wire_output() {
  outPredPC = [this]() -> uint32_t {
    if (!fetchAllowed())
      return 0u;
    const PredictInfo p =
        predict(static_cast<uint32_t>(static_cast<uint32_t>(fetchCtx.pc)));
    return p.taken ? static_cast<uint32_t>(p.predictPC)
                   : static_cast<uint32_t>(fetchCtx.pc) + 4u;
  };
  outPacked = [this]() -> uint32_t {
    if (!fetchAllowed())
      return 0u;
    const PredictInfo p =
        predict(static_cast<uint32_t>(static_cast<uint32_t>(fetchCtx.pc)));
    // shift/shiftValue branch structure verbatim from build():
    // btbHit wins over condSeen; unconditional forces shiftValue.
    const bool shift = p.btbHit || p.condSeen;
    const bool shiftValue = p.btbHit ? (p.unconditional ? true : p.taken)
                                     : (p.condSeen ? p.taken : false);
    // ckptId starts at packed bit 2 (shift/shiftValue own bits 0/1).
    uint32_t v = (static_cast<uint32_t>(getNextCkptId()) & (CKPT_CAP - 1)) << 2;
    if (shift)
      v |= 1u << 0;
    if (shiftValue)
      v |= 1u << 1;
    return v;
  };
  outValid = [this]() -> uint32_t { return fetchAllowed() ? 1u : 0u; };
  outPC = [this]() -> uint32_t {
    return fetchAllowed() ? static_cast<uint32_t>(fetchCtx.pc) : 0u;
  };
  outPredictedPC = [this]() -> uint32_t {
    return static_cast<uint32_t>(outPredPC);
  };
  outShift = [this]() -> uint32_t {
    return (static_cast<uint32_t>(outPacked) >> 0) & 0x1u;
  };
  outShiftValue = [this]() -> uint32_t {
    return (static_cast<uint32_t>(outPacked) >> 1) & 0x1u;
  };
  outCkptId = [this]() -> uint32_t {
    return (static_cast<uint32_t>(outPacked) >> 2) & (CKPT_CAP - 1);
  };
}

BPUSnapshot BPU::snapshotCheckPoint() const {
  BPUSnapshot ckptSnap;
  ckptSnap.GHR_snapshot = getGHR();
  return ckptSnap;
}

BPUSnapshot BPU::traceState() const { return snapshotCheckPoint(); }

BPUSnapshot BPU::traceCheckpoint(uint8_t id) const {
  const auto &checkpoint = bpCkpt[id & (CKPT_CAP - 1)];
  BPUSnapshot snapshot;
  snapshot.GHR_snapshot = static_cast<uint32_t>(checkpoint.GHR);
  return snapshot;
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
  return slot < ROB_CAP && static_cast<uint32_t>(rob.robTag[slot]) == tag;
}
void BPU::work() {
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
  if (!static_cast<bool>(bru.isBRUEmpty) && robTagMatches(rob, brRobTag)) {
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
      trBru.taken = pcResult != pcFrom + 4;
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
    nextCkpt = (ckid + 1) & (CKPT_CAP - 1);
  }

  // ---- Arch + speculative RAS local mirror (mirrors the main tree) ----
  // specRAS is the prediction state; archRAS is the commit-time baseline.
  // The tops are non-wrapping depths in [0, RAS_CAP].
  uint32_t specTop = static_cast<uint32_t>(tgt.specTopOfRAS);
  uint32_t archTop = static_cast<uint32_t>(tgt.archTopOfRAS);
  uint32_t specRAS[RAS_CAP], archRAS[RAS_CAP];
  for (int i = 0; i < RAS_CAP; ++i) {
    specRAS[i] = static_cast<uint32_t>(tgt.specRAS[i].retPC);
    archRAS[i] = static_cast<uint32_t>(tgt.archRAS[i].retPC);
  }

  BTBWriteIntent fetchWriteIntent;
  if (static_cast<bool>(fetchInfo.FetchValid)) {
    const uint32_t ra = static_cast<uint32_t>(fetchInfo.FetchPC) + 4;
    if (static_cast<bool>(fetchInfo.isFetchCall)) {
      specRAS[specTop++] = ra;
    } else if (static_cast<bool>(fetchInfo.isFetchRet)) {
      --specTop;
    }
    // Early training (BRU-port fetch side; same-line conflicts arbitrated at
    // the write ports below, fetch highest).
    const uint32_t fetchPC = static_cast<uint32_t>(fetchInfo.FetchPC);
    const bool isFetchRet = static_cast<bool>(fetchInfo.isFetchRet);
    fetchWriteIntent.lineWrite = true;
    fetchWriteIntent.index = (fetchPC >> 2) & (BTB_CAP - 1);
    fetchWriteIntent.actualPC = fetchPC;
    fetchWriteIntent.unconditional = true;
    fetchWriteIntent.isRet = isFetchRet;
    fetchWriteIntent.targetWrite =
        !isFetchRet && static_cast<bool>(fetchInfo.FetchJALTargetValid);
    fetchWriteIntent.target = static_cast<uint32_t>(fetchInfo.FetchJALTarget);
  }

  // Pre-commit architectural baseline: like the main tree tick() (which reads
  // the comb snapshot `tgt` for recovery), a same-cycle squash must replay
  // from the OLD committed state, ignoring this cycle's head commit.
  uint32_t archTopPreCommit = archTop;
  uint32_t archRASPre[RAS_CAP];
  for (int i = 0; i < RAS_CAP; ++i)
    archRASPre[i] = archRAS[i];

  // Commit is the architectural RAS baseline used for later ROB replay
  // (mirrors the main tree; CDB never participates).
  if (static_cast<bool>(rob.robWillCommit)) {
    const RobTag headTag = static_cast<uint32_t>(rob.robHeadTag);
    const uint32_t headSlot = robSlot(headTag);
    if (headSlot < ROB_CAP &&
        static_cast<uint32_t>(rob.robTag[headSlot]) == headTag) {
      if (static_cast<bool>(rob.robIsCall[headSlot]))
        archRAS[archTop++] = static_cast<uint32_t>(rob.robPC[headSlot]) + 4u;
      else if (static_cast<bool>(rob.robIsRet[headSlot]))
        --archTop;
    }
  }

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
    uint32_t commitTOS = archTopPreCommit;
    for (int i = 0; i < RAS_CAP; ++i)
      specRAS[i] = archRASPre[i];
    bool recovered = false;
    RobTag robTag = static_cast<uint32_t>(rob.robHeadTag);
    for (int i = 0; i < ROB_CAP; ++i) {
      if (!recovered) {
        const uint32_t robIndex = robSlot(robTag);
        if (robIndex < ROB_CAP &&
            static_cast<uint32_t>(rob.robTag[robIndex]) == robTag) {
          if (static_cast<bool>(rob.robIsCall[robIndex]))
            specRAS[commitTOS++] =
                static_cast<uint32_t>(rob.robPC[robIndex]) + 4u;
          else if (static_cast<bool>(rob.robIsRet[robIndex]))
            --commitTOS;
        }
        recovered = robTag == squashTag;
        robTag = robNextTag(robTag);
      }
    }
    dark::debug::assert(recovered, "squash tag not found from ROB head");
    ghrLocal = ckptGHR;
    specTop = commitTOS;
    nextCkpt = (ckid + 1) & (CKPT_CAP - 1);
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
    tgt.archRAS[i].retPC <= archRAS[i];
  }
}
