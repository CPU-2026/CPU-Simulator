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
struct RATInput {
  Wire<1> needSquash;
  Wire<CKPT_ID_WIDTH> SquashCkptId;
  Wire<1> issueValid;
  Wire<PHY_TAG_WIDTH> issuePhy;
  Wire<5> issueDest;
  Wire<1> issueAllocDest;
  Wire<CKPT_ID_WIDTH> issueCkptId;
};
struct RATInner {
  std::array<Register<PHY_TAG_WIDTH>, REGISTER_CAP> RAT_PRF;
  std::array<std::array<Register<PHY_TAG_WIDTH>, REGISTER_CAP>, CKPT_CAP>
      ratCkpt;
  Register<1> bootDone;
};

struct RAT : dark::Module<RATInput, RATInner> {
  uint8_t readRAT_PRF(int regNum) const;
  OperandInfo readOperand(int regNum) const;
  void work() override;
};