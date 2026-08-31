#include "../include/RAT.hpp"
#include <cstdint>

uint8_t RAT::readRAT_PRF(int regNum) const {
  return static_cast<uint32_t>(RAT_PRF[regNum]);
}

OperandInfo RAT::readOperand(int regNum) const {
  if (regNum == 0)
    return {true, 0, InvalidPhy};
  auto phy = static_cast<uint32_t>(RAT_PRF[regNum]);
  return {false, 0, phy};
}

void RAT::work() {
  if (bootDone == 0) {
    for (int i = 1; i < 32; ++i)
      RAT_PRF[i] <= i;
    bootDone <= 1;
  }
  // Single-write-point per Register (progress.md "RAT 写回收敛"):
  // - RAT_PRF[i]: restore base (ckpt _M_old), issue-dest overwrite wins
  //   (reference tick order: restore-then-setRAT_PRF)
  // - ratCkpt[issueCkptId][i]: captures the PRE-restore table (reference
  //   memcpys this->RAT_PRF, the comb-start snapshot) with dest = new mapping
  bool restore = static_cast<bool>(needSquash);
  uint32_t rcIdx = static_cast<uint32_t>(SquashCkptId);
  uint32_t icIdx = static_cast<uint32_t>(issueCkptId);
  uint32_t dest = static_cast<uint32_t>(issueDest);
  for (int i = 0; i < 32; ++i) {
    uint32_t pre = static_cast<uint32_t>(RAT_PRF[i]);
    uint32_t rest = static_cast<uint32_t>(ratCkpt[rcIdx][i]);
    bool destHit = static_cast<bool>(issueValid) &&
                   static_cast<bool>(issueAllocDest) && i == dest;
    if (destHit) {
      RAT_PRF[i] <= static_cast<uint32_t>(issuePhy);
    } else if (restore) {
      RAT_PRF[i] <= rest;
    }
    if (static_cast<bool>(issueValid)) {
      ratCkpt[icIdx][i] <=
          (destHit ? static_cast<uint32_t>(issuePhy) : pre);
    }
  }
}
