#pragma once
#include "common.h"
#include "concept.h"
#include "module.h"
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
  Wire<1> isRet;
  Wire<CKPT_ID_WIDTH> ckptId;
  Wire<32> predictedPC;
  Wire<32> pc;
  Wire<LQ_PTR_WIDTH> lqTailSnapshot;
  Wire<SQ_PTR_WIDTH> sqTailSnapshot;
  Wire<PHY_TAG_WIDTH> newPhy;
  Wire<PHY_TAG_WIDTH> oldPhy;
};
struct ROBInputIssue {
  Wire<1> issueValid;
  ROBInputEntry entry;
};
// Quad-CDB commit-ready ports (mirrors the main tree's
// ROBInput.cdbOfALU/cdbOfLQ/cdbOfMul/cdbOfDiv): each bus marks its result's
// ROB entry commit-ready; only valid+tag are needed (the ROB reads no
// payload).
struct ROBInputCDBAlu {
  Wire<1> cdbValid;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
};
struct ROBInputCDBLq {
  Wire<1> cdbValid;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
};
struct ROBInputCDBMul {
  Wire<1> cdbValid;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
};
struct ROBInputCDBDiv {
  Wire<1> cdbValid;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
};
struct ROBInputBRU {
  Wire<1> isBRUEmpty;
  Wire<ROB_TAG_WIDTH> bruHeadRobTag;
};
struct ROBInputSQ {
  std::array<Wire<1>, SQ_CAP> sqValid;
  std::array<Wire<1>, SQ_CAP> sqReadyToCommit;
  std::array<Wire<1>, SQ_CAP> sqCommitted;
  std::array<Wire<ROB_TAG_WIDTH>, SQ_CAP> sqRobTag;
  Wire<SQ_PTR_WIDTH> sqHead;
};
struct ROBInput {
  ROBInputSquash squash;
  ROBInputIssue issue;
  ROBInputCDBAlu cdbOfALU;
  ROBInputCDBLq cdbOfLQ;
  ROBInputCDBMul cdbOfMUL;
  ROBInputCDBDiv cdbOfDIV;
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
  ROBOutputHeadView headView;
  // Per-entry views consumed outside the ROB.
  struct Entry {
    std::array<Wire<ROB_TAG_WIDTH>, ROB_CAP> tag;
    std::array<Wire<1>, ROB_CAP> isCommitReady;
    std::array<Wire<1>, ROB_CAP> isRet;
    std::array<Wire<CKPT_ID_WIDTH>, ROB_CAP> ckptId;
    std::array<Wire<32>, ROB_CAP> predictedPC;
    std::array<Wire<32>, ROB_CAP> pc;
    std::array<Wire<LQ_PTR_WIDTH>, ROB_CAP> lqTailSnapshot;
    std::array<Wire<SQ_PTR_WIDTH>, ROB_CAP> sqTailSnapshot;
    std::array<Wire<PHY_TAG_WIDTH>, ROB_CAP> newPhy;
    std::array<Wire<PHY_TAG_WIDTH>, ROB_CAP> oldPhy;
  };
  Entry entry;
};

// Inner: flat ROBEntryReg array + state — keep in sync
struct ROBEntryReg {
  Register<ROB_TAG_WIDTH> tag;
  Register<2> type;
  Register<1> isCommitReady;
  Register<5> dest;
  Register<1> halt;
  Register<1> isRet;
  Register<CKPT_ID_WIDTH> ckptId;
  Register<32> predictedPC;
  Register<32> pc;
  Register<LQ_PTR_WIDTH> lqTailSnapshot;
  Register<SQ_PTR_WIDTH> sqTailSnapshot;
  Register<PHY_TAG_WIDTH> newPhy;
  Register<PHY_TAG_WIDTH> oldPhy;
};
struct ROBInner {
  std::array<ROBEntryReg, ROB_CAP> ROBqueue;
  Register<ROB_TAG_WIDTH> robHead;
  Register<ROB_TAG_WIDTH> next;
  Register<1> robHaltCommitted;
  Register<5> robHaltRd;
};

struct ROB : dark::Module<ROBInput, ROBOutput, ROBInner> {
  friend struct ReorderTester;
private:
  void wire_output();
public:
  ROB();
  static bool isOlder(RobTag tag_a, RobTag tag_b);
  static bool isYounger(RobTag tag_a, RobTag tag_b);
  bool isFull() const;
  bool isEmpty() const;
  bool matchesTag(RobTag tag) const;
  bool willCommit() const;
  bool storeWillCommit() const;
  bool isHaltCommitted() const;
  uint32_t getHaltRd() const;
  RobTag getNextTag() const;
  void work() override;
};
