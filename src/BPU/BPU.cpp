#include "../include/BPU.hpp"

#include <cstdint>

#include "../include/ROB.hpp"

namespace {
struct Snap {
  const BPUInner *st;
  // Reset-value presentation: boot commits at end of cycle 0, but cycle-0
  // prediction already reads these tables -- present boot constants until then.
  uint32_t local(uint32_t i) const {
    return static_cast<bool>(st->bootDone)
               ? static_cast<uint32_t>(st->dir.localPHT[i])
               : 1u;
  }
  uint32_t global(uint32_t i) const {
    return static_cast<bool>(st->bootDone)
               ? static_cast<uint32_t>(st->dir.globalPHT[i])
               : 1u;
  }
  uint32_t selector(uint32_t i) const {
    return static_cast<bool>(st->bootDone)
               ? static_cast<uint32_t>(st->dir.selector[i])
               : 1u;
  }
  bool condSeen(uint32_t i) const {
    return static_cast<bool>(st->tgt.condSeen[i]);
  }
  uint32_t BHT(uint32_t i) const {
    return static_cast<uint32_t>(st->tgt.BHT[i]);
  }
  bool BTBValid(uint32_t i) const {
    return static_cast<bool>(st->tgt.BTB[i].valid);
  }
  uint32_t BTBActualPC(uint32_t i) const {
    return static_cast<uint32_t>(st->tgt.BTB[i].actualPC);
  }
  uint32_t BTBTarget(uint32_t i) const {
    return static_cast<uint32_t>(st->tgt.BTB[i].target);
  }
  bool BTBUncond(uint32_t i) const {
    return static_cast<bool>(st->tgt.BTB[i].unconditional);
  }
  bool BTBCall(uint32_t i) const {
    return static_cast<bool>(st->tgt.BTB[i].isCall);
  }
  bool BTBRet(uint32_t i) const {
    return static_cast<bool>(st->tgt.BTB[i].isRet);
  }
  bool BTBIndirect(uint32_t i) const {
    return static_cast<bool>(st->tgt.BTB[i].isIndirect);
  }
  bool TargetValid(uint32_t i) const {
    return static_cast<bool>(st->tgt.TargetValid[i]);
  }
  uint32_t TargetCache(uint32_t i) const {
    return static_cast<uint32_t>(st->tgt.TargetCache[i]);
  }
  uint16_t ghr() const { return static_cast<uint32_t>(st->dir.GHR); }
};

// ---- training requests (decoded once at the top of work()) ----
struct TrainReq {
  bool valid = false;
  bool isJump = false;  // updateJump vs update
  bool taken = false;
  bool isCall = false;
  bool isRet = false;
  bool isIndirect = false;  // main-tree Cand never sets it on either port
  uint32_t pc = 0;
  uint32_t target = 0;
  uint16_t ghr = 0;
};

// ---- per-cycle write intents (fixed capacity, no per-cycle heap use) ----
enum TabKind : uint8_t {
  T_LOCAL,
  T_GLOBAL,
  T_SELECTOR,
  T_COND,
  T_BTB_APC,
  T_BTB_TGT,
  T_BTB_V,
  T_BTB_UN,
  T_BTB_CALL,
  T_BTB_RET,
  T_BTB_IND,
  T_BHT,
  T_TC,
  T_TCV,
};
struct TabEntry {
  uint8_t kind;
  uint16_t idx;
  uint32_t val;
};
struct Plan {
  TabEntry tab[32];
  uint32_t nTab = 0;
  bool bht_we = false;
  uint16_t bht_idx = 0;
  uint32_t bht_val = 0;
  uint8_t bht_tabIdx = 0;  // tab slot of this port's T_BHT (RMW fixup)
  void put(uint8_t kind, uint32_t idx, uint32_t val) {
    dark::debug::assert(nTab < 32, "plan overflow: raise Plan::tab");
    if (nTab >= 32) return;
    if (kind == T_BHT) {
      // Mirror for the per-port BHT collision check (each port puts one
      // T_BHT). `merged` may hold two, so no uniqueness assert here.
      bht_we = true;
      bht_idx = idx;
      bht_val = val;
      bht_tabIdx = static_cast<uint8_t>(nTab);  // slot this entry occupies
    }
    tab[nTab++] = {kind, static_cast<uint16_t>(idx), val};
  }
};
// Per-port bounds: fetch-info <=6, conditional BRU <=12, jump CDB <=10.
static_assert(6 + 12 + 10 <= 32, "merged plan worst case must fit Plan::tab");

// ---- update: pure function, reads snap, fills a plan, zero `<=`.
//      BRU port may set ghr/ras; CDB port is commit (tables only). ----
Plan updatePlan(const Snap &snap, const TrainReq &req) {
  Plan p;
  const uint32_t p2 = req.pc >> 2;
  const uint32_t localIndex = p2 & (BHT_CAP - 1);
  const uint32_t globalIndex = (p2 ^ req.ghr) & (BHT_CAP - 1);
  const uint32_t selectorIndex = (p2 ^ req.ghr) & (SELECTOR_CAP - 1);
  uint32_t local = snap.local(localIndex);
  uint32_t global = snap.global(globalIndex);
  const bool localPred = local >= 2;
  const bool globalPred = global >= 2;
  if (req.taken) {
    if (local < 3) ++local;
    if (global < 3) ++global;
  } else {
    if (local > 0) --local;
    if (global > 0) --global;
  }
  p.put(T_LOCAL, localIndex, local);
  p.put(T_GLOBAL, globalIndex, global);

  if (globalPred == req.taken && localPred != req.taken) {
    uint32_t choice = snap.selector(selectorIndex);
    if (choice < 3) ++choice;
    p.put(T_SELECTOR, selectorIndex, choice);
  } else if (localPred == req.taken && globalPred != req.taken) {
    uint32_t choice = snap.selector(selectorIndex);
    if (choice > 0) --choice;
    p.put(T_SELECTOR, selectorIndex, choice);
  }

  p.put(T_COND, p2 & (CONDSEEN_CAP - 1), 1);

  auto BTB_index = p2 & (BTB_CAP - 1);
  if (req.taken) {
    p.put(T_BTB_APC, BTB_index, req.pc);
    p.put(T_BTB_TGT, BTB_index, req.target);
    p.put(T_BTB_V, BTB_index, 1);
    p.put(T_BTB_UN, BTB_index, 0);
    p.put(T_BTB_CALL, BTB_index, 0);
    p.put(T_BTB_RET, BTB_index, 0);
    p.put(T_BTB_IND, BTB_index, 0);
  }
  uint32_t bhr = snap.BHT(p2 & (BHT_CAP - 1));
  p.put(T_BHT, p2 & (BHT_CAP - 1), ((bhr << 1) | (req.taken ? 1 : 0)) & 0xFF);
  return p;
}

Plan updateJumpPlan(const Snap &snap, const TrainReq &req) {
  Plan p;
  const uint32_t p2 = req.pc >> 2;
  auto BTB_index = p2 & (BTB_CAP - 1);
  p.put(T_BTB_APC, BTB_index, req.pc);
  p.put(T_BTB_TGT, BTB_index, req.target);
  p.put(T_BTB_V, BTB_index, 1);
  p.put(T_BTB_UN, BTB_index, 1);
  p.put(T_BTB_CALL, BTB_index, req.isCall ? 1 : 0);
  p.put(T_BTB_RET, BTB_index, req.isRet ? 1 : 0);
  // main tree writes the isIndirect PARAM (the CDB candidate never sets it,
  // so committed jumps always train ind=0); the hardcoded 1 diverged hanoi.
  p.put(T_BTB_IND, BTB_index, req.isIndirect ? 1u : 0u);

  const uint32_t bhr = snap.BHT(p2 & (BHT_CAP - 1));
  // main tree gates Target-Cache training on isIndirect too (never set on the
  // CDB candidate -> never trained); verbatim equivalence.
  if (req.isIndirect && req.isCall == false &&
      req.isRet == false) {  // true indirect
    const uint32_t tcHash = (p2 ^ bhr) & (TARGETCACHE_CAP - 1);
    p.put(T_TC, tcHash, req.target);
    p.put(T_TCV, tcHash, 1);
  }
  p.put(T_BHT, p2 & (BHT_CAP - 1), ((bhr << 1) | 1) & 0xFF);
  return p;
}

bool robTagMatches(const BPUInputROB &rob, RobTag tag) {
  if (static_cast<bool>(rob.isROBEmpty))
    return false;
  const uint32_t slot = robSlot(tag);
  return slot < ROB_CAP && static_cast<uint32_t>(rob.robTag[slot]) == tag;
}
}  // namespace

PredictInfo BPU::predict(int32_t pc) const {
  Snap snap(this);
  const uint32_t p2 = static_cast<uint32_t>(pc) >> 2;
  const uint16_t ghr = snap.ghr();
  const uint32_t localIndex = p2 & (BHT_CAP - 1);
  const uint32_t globalIndex = (p2 ^ ghr) & (BHT_CAP - 1);
  const uint32_t selectorIndex = (p2 ^ ghr) & (SELECTOR_CAP - 1);
  const bool useGlobal = snap.selector(selectorIndex) >= 2;
  const bool directionTaken =
      useGlobal ? snap.global(globalIndex) >= 2 : snap.local(localIndex) >= 2;
  const auto BTB_index = p2 & (BTB_CAP - 1);
  bool btbHit = snap.BTBValid(BTB_index) &&
                 snap.BTBActualPC(BTB_index) == static_cast<uint32_t>(pc);
  bool taken = btbHit && directionTaken;
  if (btbHit && snap.BTBUncond(BTB_index)) taken = true;
  const uint32_t bhr = snap.BHT(p2 & (BHT_CAP - 1));
  const uint32_t tcHash = (p2 ^ bhr) & (TARGETCACHE_CAP - 1);
  const bool tcUsable = btbHit && snap.BTBIndirect(BTB_index) &&
                        !snap.BTBCall(BTB_index) && !snap.BTBRet(BTB_index) &&
                        snap.TargetValid(tcHash);
  // RET with empty RAS: don't use BTB target 0, treat as not taken (wild fetch
  // fix)
  bool isRet = snap.BTBRet(BTB_index);
  bool rasEmpty = static_cast<uint32_t>(tgt.RAS_top) == 0;
  if (isRet && rasEmpty) {
    btbHit = false;
    taken = false;
  }
  int32_t predictPC = pc + 4;
  if (taken && btbHit) {
    if (isRet && static_cast<uint32_t>(tgt.RAS_top) > 0)
      predictPC = static_cast<int32_t>(static_cast<uint32_t>(
          tgt.RAS[(static_cast<uint32_t>(tgt.RAS_top) - 1) & (RAS_CAP - 1)]
              .retPC));
    else if (tcUsable)
      predictPC = static_cast<int32_t>(snap.TargetCache(tcHash));
    else
      predictPC = static_cast<int32_t>(snap.BTBTarget(BTB_index));
  }
  PredictInfo out{taken, predictPC};
  out.btbHit = btbHit;
  out.unconditional = btbHit && snap.BTBUncond(BTB_index);
  out.condSeen = snap.condSeen(p2 & (CONDSEEN_CAP - 1));
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
//      Each mid net calls predict() once; re-runs stay bit-identical. ----
void BPU::wire_output() {
  mid.predPC = [this]() -> uint32_t {
    if (!fetchAllowed()) return 0u;
    const PredictInfo p =
        predict(static_cast<int32_t>(static_cast<uint32_t>(fetchCtx.pc)));
    return p.taken ? static_cast<uint32_t>(p.predictPC)
                   : static_cast<uint32_t>(fetchCtx.pc) + 4u;
  };
  mid.packed = [this]() -> uint32_t {
    if (!fetchAllowed()) return 0u;
    const PredictInfo p =
        predict(static_cast<int32_t>(static_cast<uint32_t>(fetchCtx.pc)));
    // shift/shiftValue branch structure verbatim from build():
    // btbHit wins over condSeen; unconditional forces shiftValue.
    const bool shift = p.btbHit || p.condSeen;
    const bool shiftValue = p.btbHit ? (p.unconditional ? true : p.taken)
                                     : (p.condSeen ? p.taken : false);
    // ckptId starts at packed bit 2 (shift/shiftValue own bits 0/1).
    uint32_t v =
        (static_cast<uint32_t>(getNextCkptId()) & (CKPT_CAP - 1)) << 2;
    if (shift) v |= 1u << 0;
    if (shiftValue) v |= 1u << 1;
    return v;
  };
  fetchOut.valid = [this]() -> uint32_t { return fetchAllowed() ? 1u : 0u; };
  fetchOut.pc = [this]() -> uint32_t {
    return fetchAllowed() ? static_cast<uint32_t>(fetchCtx.pc) : 0u;
  };
  fetchOut.predictedPC = [this]() -> uint32_t {
    return static_cast<uint32_t>(mid.predPC);
  };
  fetchOut.shift = [this]() -> uint32_t {
    return (static_cast<uint32_t>(mid.packed) >> 0) & 0x1u;
  };
  fetchOut.shiftValue = [this]() -> uint32_t {
    return (static_cast<uint32_t>(mid.packed) >> 1) & 0x1u;
  };
  fetchOut.ckptId = [this]() -> uint32_t {
    return (static_cast<uint32_t>(mid.packed) >> 2) & (CKPT_CAP - 1);
  };
}

BPUSnapshot BPU::snapshotCheckPoint() const {
  BPUSnapshot ckptSnap;
  ckptSnap.GHR_snapshot = getGHR();
  ckptSnap.alignHead = static_cast<uint32_t>(tgt.alignHead);
  ckptSnap.alignTail = static_cast<uint32_t>(tgt.alignTail);
  ckptSnap.RAS_top = static_cast<uint32_t>(tgt.RAS_top);
  return ckptSnap;
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
    for (int i = 0; i < SELECTOR_CAP; ++i) dir.selector[i] <= 1;
    bootDone <= true;
  }

  Snap snap(this);
  bool needSquash = static_cast<bool>(squash.needSquash);
  RobTag squashTag = static_cast<uint32_t>(squash.SquashTag);
  uint32_t squashCkpt = static_cast<uint32_t>(squash.SquashCkpt);

  // ---- decode both train requests (once per cycle) ----
  TrainReq trBru, trCdb;
  const RobTag brRobTag = static_cast<uint32_t>(bru.bruHeadRobTag);
  if (!static_cast<bool>(bru.isBRUEmpty) && robTagMatches(rob, brRobTag)) {
    auto pcResult = static_cast<uint32_t>(bru.bruHeadPCResult);
    auto pcFrom = static_cast<uint32_t>(bru.bruHeadPCFrom);
    ++branchTotal;
    bool correct =
        pcResult == static_cast<uint32_t>(rob.robPredictPC[robSlot(brRobTag)]);
    if (correct) ++branchCorrect;
    if (!needSquash || ROB::isOlder(brRobTag, squashTag)) {
      trBru.valid = true;
      trBru.isJump = false;
      trBru.pc = pcFrom;
      trBru.taken = pcResult != pcFrom + 4;
      trBru.target = pcResult;
      auto cid = static_cast<uint32_t>(rob.robCkptId[robSlot(brRobTag)]);
      // Checkpoints are written only by fetch allocation and read by squash;
      // same-cycle allocation and rollback use distinct IDs.
      trBru.ghr = static_cast<uint32_t>(bpCkpt[cid].GHR);
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
      if (correct) ++branchCorrect;
      trCdb.valid = true;
      trCdb.isJump = true;
      trCdb.pc = static_cast<uint32_t>(rob.robPC[robIdx]);
      trCdb.target = pc;
      trCdb.isCall = static_cast<bool>(rob.robIsCall[robIdx]);
      trCdb.isRet = static_cast<bool>(rob.robIsRet[robIdx]);
    }
  }

  // ---- two plans from the same snap, no bypass between ports ----
  Plan p_bru, p_cdb;
  if (trBru.valid)
    p_bru =
        trBru.isJump ? updateJumpPlan(snap, trBru) : updatePlan(snap, trBru);
  if (trCdb.valid) p_cdb = updateJumpPlan(snap, trCdb);

  // ---- fetch allocation (BRU-port speculative: bpCkpt/GHR/nextCkptId) ----
  uint16_t ghrLocal = snap.ghr();
  uint32_t nextCkpt = static_cast<uint32_t>(nextCkptId);
  if (static_cast<bool>(fetchOut.valid)) {
    auto ckid = static_cast<uint32_t>(fetchOut.ckptId);
    BPUSnapshot ckptSnap = snapshotCheckPoint();
    bpCkpt[ckid].GHR <= ckptSnap.GHR_snapshot;
    bpCkpt[ckid].alignHead <= static_cast<uint32_t>(ckptSnap.alignHead);
    bpCkpt[ckid].alignTail <= static_cast<uint32_t>(ckptSnap.alignTail);
    bpCkpt[ckid].RAS_top <= static_cast<uint32_t>(ckptSnap.RAS_top);
    if (static_cast<bool>(fetchOut.shift))
      ghrLocal = static_cast<uint16_t>(
          ((static_cast<uint32_t>(ghrLocal) << 1) |
           (static_cast<bool>(fetchOut.shiftValue) ? 1u : 0u)) &
          HISTORY_MASK);
    nextCkpt = (ckid + 1) & (CKPT_CAP - 1);
  }

  // ---- RAS / alignQueue local mirror (BRU-port fetchInfo + squash rewind
  // override) ----
  uint32_t rasTop = static_cast<uint32_t>(tgt.RAS_top);
  uint32_t alignTail = static_cast<uint32_t>(tgt.alignTail);
  uint32_t alignHead = static_cast<uint32_t>(tgt.alignHead);
  // Pre-fi-block base (main-tree semantics): the squash rewind range comes
  // from the comb snapshot, so this tick's journal entries survive.
  uint32_t alignTailPreFi = alignTail;
  uint32_t rasRetPC[RAS_CAP], rasTimes[RAS_CAP];
  uint32_t alAddr[ALIGNQ_CAP], alIndex[ALIGNQ_CAP], alTimes[ALIGNQ_CAP];
  for (int i = 0; i < RAS_CAP; ++i) {
    rasRetPC[i] = static_cast<uint32_t>(tgt.RAS[i].retPC);
    rasTimes[i] = static_cast<uint32_t>(tgt.RAS[i].times);
  }
  for (int i = 0; i < ALIGNQ_CAP; ++i) {
    alAddr[i] = static_cast<uint32_t>(tgt.alignQueue[i].addr);
    alIndex[i] = static_cast<uint32_t>(tgt.alignQueue[i].index);
    alTimes[i] = static_cast<uint32_t>(tgt.alignQueue[i].times);
  }

  Plan p_fi;
  if (static_cast<bool>(fetchInfo.FetchValid)) {
    const uint32_t ra = static_cast<uint32_t>(fetchInfo.FetchPC) + 4;
    if (static_cast<bool>(fetchInfo.isFetchCall)) {
      uint32_t topIdx = rasTop & (RAS_CAP - 1);
      if (rasTop > 0 && rasRetPC[(rasTop - 1) & (RAS_CAP - 1)] == ra) {
        alAddr[alignTail & (ALIGNQ_CAP - 1)] =
            rasRetPC[(rasTop - 1) & (RAS_CAP - 1)];
        alIndex[alignTail & (ALIGNQ_CAP - 1)] = (rasTop - 1) & (RAS_CAP - 1);
        alTimes[alignTail & (ALIGNQ_CAP - 1)] =
            rasTimes[(rasTop - 1) & (RAS_CAP - 1)];
        ++alignTail;
        ++rasTimes[(rasTop - 1) & (RAS_CAP - 1)];
      } else {
        rasRetPC[topIdx] = ra;
        rasTimes[topIdx] = 1;
        ++rasTop;
      }
    } else if (static_cast<bool>(fetchInfo.isFetchRet) && rasTop > 0) {
      uint32_t topIdx = (rasTop - 1) & (RAS_CAP - 1);
      alAddr[alignTail & (ALIGNQ_CAP - 1)] = rasRetPC[topIdx];
      alIndex[alignTail & (ALIGNQ_CAP - 1)] = topIdx;
      alTimes[alignTail & (ALIGNQ_CAP - 1)] = rasTimes[topIdx];
      ++alignTail;
      if (rasTimes[topIdx] > 1)
        --rasTimes[topIdx];
      else
        --rasTop;
    }
    // Early training (BRU-port fetch side; same-slot conflicts arbitrated in
    // commit, fetch highest)
    if (static_cast<bool>(fetchInfo.isFetchCall) ||
        (!static_cast<bool>(fetchInfo.isFetchCall) &&
         !static_cast<bool>(fetchInfo.isFetchRet))) {
      auto BTB_index =
          (static_cast<uint32_t>(fetchInfo.FetchPC) >> 2) & (BTB_CAP - 1);
      p_fi.put(T_BTB_APC, BTB_index, static_cast<uint32_t>(fetchInfo.FetchPC));
      p_fi.put(T_BTB_V, BTB_index, 1);
      p_fi.put(T_BTB_UN, BTB_index, 1);
      p_fi.put(T_BTB_CALL, BTB_index,
               static_cast<bool>(fetchInfo.isFetchCall) ? 1 : 0);
      p_fi.put(T_BTB_RET, BTB_index, 0);
      if (static_cast<bool>(fetchInfo.FetchJALTargetValid))
        p_fi.put(T_BTB_TGT, BTB_index,
                 static_cast<uint32_t>(fetchInfo.FetchJALTarget));
      else
        p_fi.put(T_BTB_IND, BTB_index, 1);
    }
    if (static_cast<bool>(fetchInfo.isFetchRet)) {
      auto BTB_index =
          (static_cast<uint32_t>(fetchInfo.FetchPC) >> 2) & (BTB_CAP - 1);
      p_fi.put(T_BTB_APC, BTB_index, static_cast<uint32_t>(fetchInfo.FetchPC));
      p_fi.put(T_BTB_V, BTB_index, 1);
      p_fi.put(T_BTB_UN, BTB_index, 1);
      p_fi.put(T_BTB_CALL, BTB_index, 0);
      p_fi.put(T_BTB_RET, BTB_index, 1);
    }
  }

  // Checkpoint allocation and squash rollback never coincide: the allocated
  // ckptId is always newer (ring distance >= 1), so bpCkpt reads never race.
  dark::debug::assert(!(static_cast<bool>(fetchOut.valid) && needSquash),
                      "fetch-alloc and squash-rollback in the same cycle");
  // ---- squash restore (highest priority over all speculative state) ----
  if (needSquash) {
    uint32_t ckid = squashCkpt & (CKPT_CAP - 1);
    uint16_t ckptGHR = static_cast<uint32_t>(bpCkpt[ckid].GHR);
    uint32_t ckptAlignTail = static_cast<uint32_t>(bpCkpt[ckid].alignTail);
    uint32_t ckptAlignHead = static_cast<uint32_t>(bpCkpt[ckid].alignHead);
    uint32_t ckptRasTop = static_cast<uint32_t>(bpCkpt[ckid].RAS_top);
    uint32_t curTail = alignTailPreFi;
    uint32_t base = ckptAlignTail;
    // mod-256 ring distance (alignTail is 8-bit): the main tree computes this
    // in uint8_t, so a uint32 subtraction would underflow and replay garbage.
    uint32_t dist = (curTail - base) & 0xFF;
    for (int k = 0; k < ALIGNQ_CAP; ++k) {
      if (static_cast<uint32_t>(k) >= dist) continue;
      uint32_t pos = curTail - 1 - static_cast<uint32_t>(k);
      uint32_t idx = alIndex[pos & (ALIGNQ_CAP - 1)] & (RAS_CAP - 1);
      rasRetPC[idx] = alAddr[pos & (ALIGNQ_CAP - 1)];
      rasTimes[idx] = alTimes[pos & (ALIGNQ_CAP - 1)];
    }
    ghrLocal = ckptGHR;
    alignTail = ckptAlignTail;
    alignHead = ckptAlignHead;
    rasTop = ckptRasTop;
    nextCkpt = (ckid + 1) & (CKPT_CAP - 1);
  }

  // ---- commit: resource-typed arbitration, explicit next-state mux ----
  {
    // BHT dual-port collision: bru then cdb -> ((bruVal << 1) | 1). Rewrite
    // the cdb tab entry (mergeIn/apply read tab[], not the bht_val mirror).
    if (p_bru.bht_we && p_cdb.bht_we && p_bru.bht_idx == p_cdb.bht_idx)
      p_cdb.tab[p_cdb.bht_tabIdx].val = ((p_bru.bht_val << 1) | 1u) & 0xFFu;

    // Merge fi > cdb > bru (first-in wins) so every physical Register is
    // assigned at most once per cycle.
    Plan merged;
    auto mergeIn = [&](const Plan &src) {
      for (uint32_t q = 0; q < src.nTab; ++q) {
        const auto &e = src.tab[q];
        bool dup = false;
        for (uint32_t m = 0; m < merged.nTab; ++m)
          if (merged.tab[m].kind == e.kind && merged.tab[m].idx == e.idx)
            dup = true;
        if (!dup) merged.put(e.kind, e.idx, e.val);
      }
    };
    mergeIn(p_fi);
    mergeIn(p_cdb);
    mergeIn(p_bru);

    // Tables: single write per plan entry (merged already deduplicated).
    auto apply = [&](const Plan &src) {
      for (uint32_t k = 0; k < src.nTab; ++k) {
        const auto &e = src.tab[k];
        switch (e.kind) {
          case T_LOCAL:
            dir.localPHT[e.idx] <= e.val;
            break;
          case T_GLOBAL:
            dir.globalPHT[e.idx] <= e.val;
            break;
          case T_SELECTOR:
            dir.selector[e.idx] <= e.val;
            break;
          case T_COND:
            tgt.condSeen[e.idx] <= e.val;
            break;
          case T_BTB_APC:
            tgt.BTB[e.idx].actualPC <= e.val;
            break;
          case T_BTB_TGT:
            tgt.BTB[e.idx].target <= e.val;
            break;
          case T_BTB_V:
            tgt.BTB[e.idx].valid <= e.val;
            break;
          case T_BTB_UN:
            tgt.BTB[e.idx].unconditional <= e.val;
            break;
          case T_BTB_CALL:
            tgt.BTB[e.idx].isCall <= e.val;
            break;
          case T_BTB_RET:
            tgt.BTB[e.idx].isRet <= e.val;
            break;
          case T_BTB_IND:
            tgt.BTB[e.idx].isIndirect <= e.val;
            break;
          case T_BHT:
            tgt.BHT[e.idx] <= e.val;
            break;
          case T_TC:
            tgt.TargetCache[e.idx] <= e.val;
            break;
          case T_TCV:
            tgt.TargetValid[e.idx] <= e.val;
            break;
          default:
            break;
        }
      }
    };
    apply(merged);
  }

  // Speculative-state writeback (squash > BRU; CDB never participates)
  dir.GHR <= ghrLocal;
  nextCkptId <= nextCkpt;
  tgt.RAS_top <= rasTop;
  tgt.alignHead <= alignHead;
  tgt.alignTail <= alignTail;

  for (int i = 0; i < RAS_CAP; ++i) {
    tgt.RAS[i].retPC <= rasRetPC[i];
    tgt.RAS[i].times <= rasTimes[i];
  }
  for (int i = 0; i < ALIGNQ_CAP; ++i) {
    tgt.alignQueue[i].addr <= alAddr[i];
    tgt.alignQueue[i].index <= alIndex[i];
    tgt.alignQueue[i].times <= alTimes[i];
  }
}
