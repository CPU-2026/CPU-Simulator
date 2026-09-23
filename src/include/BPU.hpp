#pragma once
#include "common.h"
#include "module.h"
#include "register.h"
#include "tools.h"
#include <array>
#include <cstdint>
// The common-header CKPT_LIVE_MAX guard covers every checkpoint retained in
// ROB, ICache, FQ, or IQ before an ID can be recycled; CKPT_ID_WIDTH is the
// exact carrier for the logical pool IDs 0..CKPT_CAP-1.
// BP update arbitration input: both table-training sources (BRU branch results
// and CDB JAL/JALR transfers) converge to this single point so that the two
// update calls keep a fixed order (BRU candidate first) regardless of stage
// scheduling order.
struct BPUInputSquash {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
  Wire<CKPT_ID_WIDTH> SquashCkpt;
};
struct BPUInputCDB {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
  Wire<1> cdbIsControl;
};
struct BPUInputBRU {
  Wire<1> isBRUEmpty;
  Wire<ROB_TAG_WIDTH> bruHeadRobTag;
  Wire<32> bruHeadPCResult;
  Wire<32> bruHeadPCFrom;
};
struct BPUInputROB {
  Wire<1> isROBEmpty;
  Wire<ROB_TAG_WIDTH> robHeadTag;
  Wire<1> robWillCommit;
  std::array<Wire<ROB_TAG_WIDTH>, ROB_CAP> robTag;
  std::array<Wire<32>, ROB_CAP> robPredictPC;
  std::array<Wire<32>, ROB_CAP> robPC;
  std::array<Wire<1>, ROB_CAP> robIsCall;
  std::array<Wire<1>, ROB_CAP> robIsRet;
  std::array<Wire<CKPT_ID_WIDTH>, ROB_CAP> robCkptId;
};
// Fetch-context ports feeding the prediction bundle: the fetch stage hands
// the predictor the current PC plus the fetch-stall/squash gates; the
// predictor answers through the BPUOutput out* wires. Hardware counterpart: the
// NPC/redirect combinational cloud inside the predictor unit (fetch context
// in, prediction bundle out).
struct BPUInputFetchCtx {
  Wire<32> pc;         // FetchUnitModule.programCounter (_M_old)
  Wire<1> squashNeed;  // flushArbiter.needSquash
  Wire<1> haltFetched; // FetchUnitModule.haltFetched
  Wire<1> fqFull;      // FQModule.isFull()
  Wire<1> imemReqFull; // ICache.isRequestFull() || IMEM.isRequestFull()
};
struct BPUInputFetchTypeInfo {
  Wire<1> FetchValid;
  Wire<1> isFetchCall;
  Wire<1> isFetchRet;
  Wire<1> FetchJALTargetValid;
  Wire<32> FetchPC;
  Wire<32> FetchJALTarget;
};
struct BPUInput {
  BPUInputBRU bru;
  BPUInputCDB cdb;
  BPUInputROB rob;
  BPUInputSquash squash;
  BPUInputFetchCtx fetchCtx;
  BPUInputFetchTypeInfo fetchInfo;
};

struct BPUOutput {
  Wire<32> outPredPC; // guarded (taken ? predictPC : pc+4)
  Wire<32> outPacked;
  Wire<1> outValid;
  Wire<32> outPC;
  Wire<32> outPredictedPC;
  Wire<1> outShift;
  Wire<1> outShiftValue;
  Wire<CKPT_ID_WIDTH> outCkptId;
};

struct RASEntry {
  Register<32> retPC;
};
// Register-storage mirror of the comb-domain BTB line.
struct BTBEntryReg {
  Register<24> tag;
  Register<30> target; // target[31:2], restored with << 2 on prediction
  Register<2> state; // 0: invalid, 1: conditional, 2: unconditional, 3: return
};
// Register-storage mirror of the plain BPUSnapshot (comb-domain).
struct BPUSnapshotReg {
  Register<8> GHR;
};

// Tournament direction predictor: direct-PC local counters, gshare global
// counters, and a gshare-indexed chooser. Counter value 1 is weakly not-taken
// and 2 is weakly taken.
struct DirectionPred {
  std::array<Register<2>, BHT_CAP> localPHT;
  std::array<Register<2>, BHT_CAP> globalPHT;
  std::array<Register<2>, SELECTOR_CAP> selector;
  Register<8> GHR;
};

// Target prediction ("where to jump"): BTB (targets + jump type) and a
// committed RAS baseline with its speculative copy (mirrors the main tree).
// The tops are non-wrapping depths in [0, RAS_CAP].
struct TargetPred {
  std::array<BTBEntryReg, BTB_CAP> BTB;
  std::array<RASEntry, RAS_CAP> specRAS;
  std::array<RASEntry, RAS_CAP> archRAS;
  Register<8> specTopOfRAS;
  Register<8> archTopOfRAS;
  // Branch-type filter: set when a PC resolves as a conditional (taken or
  // not). Lets the fetch stage shift the GHR for conditionals that are not
  // BTB-resident (never-taken branches never train the BTB), so history
  // membership stops depending on BTB residency churn.
  std::array<Register<1>, CONDSEEN_CAP> condSeen;
};
struct BPUInner {
  DirectionPred dir;
  TargetPred tgt;
  std::array<BPUSnapshotReg, CKPT_CAP> bpCkpt;
  Register<CKPT_ID_WIDTH> nextCkptId;
  Register<1> bootDone; // cycle-0 init: all direction counters = 1
};
struct BPU : dark::Module<BPUInput, BPUOutput, BPUInner> {
  BPU() { wire_output(); }
  uint64_t branchTotal = 0;
  uint64_t branchCorrect = 0;
  uint8_t getGHR() const { return static_cast<uint32_t>(dir.GHR); }
  uint64_t getBranchTotal() const { return branchTotal; }
  uint64_t getBranchCorrect() const { return branchCorrect; }
  PredictInfo predict(uint32_t pc) const;
  BPUSnapshot snapshotCheckPoint() const;
  BPUSnapshot traceState() const;
  BPUSnapshot traceCheckpoint(uint8_t id) const;
  uint8_t getNextCkptId() const { return static_cast<uint32_t>(nextCkptId); }
  void work() override;
private:
  // Fetch-direction guard, verbatim from the retired FetchDecision::build.
  bool fetchAllowed() const;
  void wire_output();
};
