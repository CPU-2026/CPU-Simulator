#pragma once
#include "Commit.hpp"
#include "common.h"
#include "concept.h"
#include "module.h"
#include "Recovery.hpp"
#include "tools.h"
#include <array>
#include <cstdint>
#include <cstring>

enum class ROBType : dark::max_size_t {
  REGISTER,
  BRANCH,
  STORE,
  LINK,
};
// ---- Input ----
struct ROBInputSquash {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
};
struct ROBInputEntry {
  Wire<2> type;
  Wire<1> isCommitReady;
  Wire<5> dest;
  Wire<1> halt;
  Wire<1> exception;
  Wire<1> isCall;
  Wire<1> isRet;
  Wire<CKPT_ID_WIDTH> ckptId;
  Wire<32> predictedPC;
  Wire<32> pc;
  Wire<LQ_SEQ_WIDTH> lqTailSnapshot;
  Wire<SQ_SEQ_WIDTH> sqTailSnapshot;
  Wire<PHY_TAG_WIDTH> newPhy;
  Wire<PHY_TAG_WIDTH> oldPhy;
};
struct ROBInputIssue {
  Wire<1> issueValid;
  ROBInputEntry entry;
};
// ALU and shared MUL/DIV/LQ completion ports (valid + ROB identity only).
struct ROBInputCDBAlu {
  Wire<1> cdbValid;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
};
struct ROBInputCDBShared {
  Wire<1> cdbValid;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
};
struct ROBInputBRU {
  Wire<1> isBRUEmpty;
  Wire<ROB_TAG_WIDTH> bruHeadRobTag;
};
struct ROBInputSQ {
  Wire<ROB_CAP> readyMask;
  Wire<ROB_CAP> epochMask;
};
struct ROBInput {
  std::array<Wire<1>, COMMIT_WIDTH> commitExit;
  std::array<Wire<1>, COMMIT_WIDTH> commitStoreReady;
  Wire<ROB_TAG_WIDTH> replayOffset{[]() { return 0u; }};
  Wire<1> retirementBlocked{[]() { return 0u; }};
  ROBInputSquash squash;
  std::array<ROBInputIssue, ISSUE_WIDTH> issue;
  ROBInputCDBAlu cdbOfALU;
  ROBInputCDBShared cdbOfShared;
  ROBInputBRU bru;
  ROBInputSQ sq;
};

// ---- Output: 10-scalar view (group name headView avoids base-class name-hiding
// with bridge accessor methods isEmpty/isFull/isHeadCommitReady/isHeadHalt/headType/headDest) + flat entry arrays ----
struct ROBOutputHeadView {
  Wire<ROB_TAG_WIDTH> head;
  Wire<1> isEmpty;
  Wire<1> isHeadCommitReady;
  Wire<1> isHeadHalt;
  Wire<2> headType;
};
struct ROBOutput {
  std::array<CommitPort, COMMIT_WIDTH> retirement;
  ROBOutputHeadView headView;
  // Per-entry views consumed outside the ROB.
  struct Entry {
     std::array<Wire<ROB_TAG_WIDTH>, ROB_CAP> tag;
     std::array<Wire<1>, ROB_CAP> isCommitReady;
     std::array<Wire<1>, ROB_CAP> isCall;
     std::array<Wire<1>, ROB_CAP> isRet;
     std::array<Wire<5>, ROB_CAP> dest;
     std::array<Wire<CKPT_ID_WIDTH>, ROB_CAP> ckptId;
    std::array<Wire<32>, ROB_CAP> predictedPC;
    std::array<Wire<32>, ROB_CAP> pc;
    std::array<Wire<LQ_SEQ_WIDTH>, ROB_CAP> lqTailSnapshot;
    std::array<Wire<SQ_SEQ_WIDTH>, ROB_CAP> sqTailSnapshot;
    std::array<Wire<PHY_TAG_WIDTH>, ROB_CAP> newPhy;
    std::array<Wire<PHY_TAG_WIDTH>, ROB_CAP> oldPhy;
  };
  Entry entry;
  std::array<RecoveryNarrowPort, RECOVERY_WIDTH> replayNarrow;
  std::array<RecoveryRasPort, RECOVERY_WIDTH> replayRas;
};

// Inner: flat ROBEntryReg array + state — keep in sync
struct ROBEntryReg {
  Register<ROB_TAG_WIDTH> tag;
  Register<2> type;
  Register<1> isCommitReady;
  Register<5> dest;
  Register<1> halt;
  Register<1> exception;
  Register<1> isCall;
  Register<1> isRet;
  Register<CKPT_ID_WIDTH> ckptId;
  Register<32> predictedPC;
  Register<32> pc;
  Register<LQ_SEQ_WIDTH> lqTailSnapshot;
  Register<SQ_SEQ_WIDTH> sqTailSnapshot;
  Register<PHY_TAG_WIDTH> newPhy;
  Register<PHY_TAG_WIDTH> oldPhy;
};
struct ROBInner {
  std::array<ROBEntryReg, ROB_CAP> ROBqueue;
  Register<ROB_TAG_WIDTH> robHead;
  Register<ROB_TAG_WIDTH> next;
  Register<1> robHaltCommitted;
  Register<1> robTrapCommitted;
  Register<5> robHaltRd;
};

struct ROB : dark::Module<ROBInput, ROBOutput, ROBInner> {
private:
  void wire_output();
  unsigned replaySlot(unsigned lane) const;
public:
  ROB();
  static bool isOlder(RobTag tag_a, RobTag tag_b);
  static bool isYounger(RobTag tag_a, RobTag tag_b);
  bool isFull() const;
  bool isEmpty() const;
  unsigned count() const;
  bool canAllocate(unsigned n) const; // at least n free slots
  bool matchesTag(RobTag tag) const;
  bool willCommit() const;
  unsigned commitCount() const;
  RobTag commitTag(unsigned lane) const;
  bool storeWillCommit() const;
  bool isHaltCommitted() const;
  bool isTrapCommitted() const { return bool(robTrapCommitted); }
  uint32_t getHaltRd() const;
  RobTag getNextTag() const;
  void work() override;
};
