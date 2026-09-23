#pragma once
#include "common.h"
#include "module.h"
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
  std::array<Wire<5>, ROB_CAP> dest;
  std::array<Wire<PHY_TAG_WIDTH>, ROB_CAP> newPhy;
};
struct RATInput {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
  Wire<1> issueValid;
  Wire<PHY_TAG_WIDTH> issuePhy;
  Wire<5> issueDest;
  Wire<1> issueAllocDest;
  RATInputROB rob;
};
struct RATInner {
  std::array<Register<PHY_TAG_WIDTH>, REGISTER_CAP> specRAT;
  std::array<Register<PHY_TAG_WIDTH>, REGISTER_CAP> archRAT;
  Register<1> bootDone;
};

struct RAT : dark::Module<RATInput, RATInner> {
  uint8_t readRAT_PRF(int regNum) const;
  OperandInfo readOperand(int regNum) const;
  void work() override;
};
