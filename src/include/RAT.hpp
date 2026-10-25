#pragma once
#include "Commit.hpp"
#include "common.h"
#include "module.h"
#include "Recovery.hpp"
#include "SRAM.hpp"
#include <array>
#include <cstdint>

struct OperandInfo {
  bool ready;
  int32_t value;
  uint32_t phyRegIndex;
};
struct RATInputROB {
  Wire<1> isEmpty;
  Wire<1> willCommit;
  Wire<ROB_TAG_WIDTH> head;
  Wire<ROB_TAG_WIDTH> next;
  Wire<5> headDest;
  Wire<PHY_TAG_WIDTH> headNewPhy;
  std::array<Wire<ROB_TAG_WIDTH>, ROB_CAP> tag;
};
struct RATIssuePort {
  Wire<1> valid;
  Wire<PHY_TAG_WIDTH> phy;
  Wire<5> dest;
  Wire<1> allocDest;
};
struct RATInput {
  std::array<CommitPort, COMMIT_WIDTH> commits;
  std::array<RecoveryNarrowPort, RECOVERY_WIDTH> replay;
  RecoveryInput recovery;
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
  std::array<RATIssuePort, ISSUE_WIDTH> issue;
  RATInputROB rob;
};
struct RATInner {
  std::array<Register<PHY_TAG_WIDTH>, REGISTER_CAP> specRAT;
  SRAM<1, 192, 6> archStorage;
  std::array<Register<1>, REGISTER_CAP> archValid;
  Register<1> replayOpen;
  Register<1> bootDone;
};

struct RAT : dark::Module<RATInput, RATInner> {
  RAT();
  uint8_t readRAT_PRF(int regNum) const;
  OperandInfo readOperand(int regNum) const;
  void work() override;
};
