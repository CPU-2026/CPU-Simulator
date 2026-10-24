#pragma once
#include "Commit.hpp"
#include "common.h"
#include "module.h"
#include "register.h"
#include "tools.h"
#include "Recovery.hpp"
#include "SRAM.hpp"
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
  Wire<1> bruHeadActualTaken;
};
struct BPUInputROB {
  Wire<1> isROBEmpty;
  Wire<ROB_TAG_WIDTH> robHeadTag;
  Wire<ROB_TAG_WIDTH> robNextTag;
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
  Wire<32> pc;          // FetchUnitModule.programCounter (_M_old)
  Wire<1> squashNeed;   // flushArbiter.needSquash
  Wire<1> haltFetched;  // Latched halt or HALT accepted this cycle.
  Wire<1> fqCanAccept;  // FQ admission for at least one entry.
  Wire<1> fqCanAccept2; // FQ admission for a full 2-entry group.
  Wire<1> memCanAccept; // Request capacity and SRAM port allow a new fetch.
};
struct BPUInputFetchTypeInfo {
  Wire<1> FetchValid;
  Wire<1> isFetchCall;
  Wire<1> isFetchRet;
  Wire<1> FetchJALTargetValid;
  Wire<32> FetchPC;
  Wire<32> FetchJALTarget;
  // Second lane of the fetch group (FETCH_WIDTH == 2). Kept wired to 0 at
  // width 1 so single-width behavior is bit-identical.
  Wire<1> FetchValid1;
  Wire<1> isFetchCall1;
  Wire<1> isFetchRet1;
  Wire<1> FetchJALTargetValid1;
  Wire<32> FetchPC1;
  Wire<32> FetchJALTarget1;
};
struct BPUInput {
  std::array<CommitPort, COMMIT_WIDTH> commits;
  std::array<RecoveryRasPort, RECOVERY_WIDTH> replay;
   RecoveryInput recovery;
  BPUInputBRU bru;
  BPUInputCDB cdb;
  BPUInputROB rob;
  BPUInputSquash squash;
  BPUInputFetchCtx fetchCtx;
  BPUInputFetchTypeInfo fetchInfo;
};

struct BPUOutput {
  Wire<32> outPredPC; // group advance: next PC after the last valid lane
  Wire<1> outValid;   // group valid (lane0 valid)
  Wire<1> outValid1;  // lane1 valid (second instruction of the group)
  Wire<32> outPC;     // lane0 PC
  Wire<32> outPC1;    // lane1 PC
  Wire<32> outPredictedPC;  // lane0 context predictPC
  Wire<32> outPredictedPC1; // lane1 context predictPC
  Wire<1> outShift;       // lane0 GHR shift
  Wire<1> outShiftValue;  // lane0 GHR shift value
  Wire<1> outShift1;      // lane1 GHR shift
  Wire<1> outShiftValue1; // lane1 GHR shift value
  Wire<CKPT_ID_WIDTH> outCkptId; // shared fetch checkpoint
};

struct RASEntry {
  Register<32> retPC;
};
// Register-storage mirror of the comb-domain BTB line.
struct BTBEntryReg {
  Register<BTB_TAG_WIDTH> tag;
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
   SRAM<1, (RAS_CAP << 5), 32> archStorage;
   std::array<Register<1>, RAS_CAP> archValid;
   Register<8> baselineTop;
   Register<1> replayOpen;
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
   BPU();
  uint64_t branchTotal = 0;
  uint64_t branchCorrect = 0;
  uint8_t getGHR() const { return static_cast<uint32_t>(dir.GHR); }
  uint64_t getBranchTotal() const { return branchTotal; }
  uint64_t getBranchCorrect() const { return branchCorrect; }
  PredictInfo predict(uint32_t pc, uint8_t ghr) const;
  BPUSnapshot snapshotCheckPoint() const;
  uint8_t getNextCkptId() const { return static_cast<uint32_t>(nextCkptId); }
  void work() override;
 private:
  bool archWriteEnabled() const;
  uint32_t archWriteLane(unsigned lane) const;
  bool archWrites(unsigned lane) const;
  // Fetch-direction guard, verbatim from the retired FetchDecision::build.
  bool fetchAllowed() const;
  // One-cycle fetch-group prediction: lane0 = fetchCtx.pc, lane1 = pc+4 when
  // FETCH_WIDTH == 2 and lane0 falls through within the same I$ line. The two
  // lanes share one checkpoint; the GHR advances per predicted-taken branch.
  struct FetchGroup {
    bool valid0 = false, valid1 = false;
    uint32_t pc0 = 0, pc1 = 0;
    uint32_t predPC0 = 0, predPC1 = 0;
    bool shift0 = false, shift1 = false;
    bool shiftValue0 = false, shiftValue1 = false;
    uint32_t nextPC = 0;
  };
  FetchGroup fetchGroup() const;
  void wire_output();
};
