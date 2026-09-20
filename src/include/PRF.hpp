#pragma once
#include "common.h"
#include "module.h"
#include <array>
#include <cstdint>
struct PRFEntry {
  Register<32> value;
  Register<1> ready;
};
struct PRFInputSquash {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
  Wire<CKPT_ID_WIDTH> CkptId;
};
// Quad-CDB write ports (mirrors the main tree's
// PRFInput.cdbOfALU/cdbOfLQ/cdbOfMul/cdbOfDiv): the ALU group gates on
// cdbIsControl (control results never write PRF); the LQ/MUL/DIV groups have
// no isControl wire -- loads, multiplies and divides are never control ops,
// saving three ports.
struct PRFInputCDBAlu {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
  Wire<1> cdbIsControl;
  Wire<PHY_TAG_WIDTH> cdbNewPhy;
};
struct PRFInputCDBLq {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
  Wire<PHY_TAG_WIDTH> cdbNewPhy;
};
struct PRFInputCDBMul {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
  Wire<PHY_TAG_WIDTH> cdbNewPhy;
};
struct PRFInputCDBDiv {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
  Wire<PHY_TAG_WIDTH> cdbNewPhy;
};
struct PRFInputIssue {
  Wire<1> issueValid;
  Wire<PHY_TAG_WIDTH> issuePhy;
  Wire<1> issueAllocDest;
  Wire<32> issuePC;
  Wire<1> issueIsControl;
  Wire<CKPT_ID_WIDTH> issueCkptId;
};
struct PRFInputROB {
  Wire<1> robWillCommit;
  Wire<1> robHeadIsHalt;
  Wire<2> robHeadType;
  Wire<PHY_TAG_WIDTH> robHeadOldPhy;
};
struct PRFInput {
  PRFInputSquash squash;
  PRFInputCDBAlu cdbOfALU;
  PRFInputCDBLq cdbOfLQ;
  PRFInputCDBMul cdbOfMUL;
  PRFInputCDBDiv cdbOfDIV;
  PRFInputIssue issue;
  PRFInputROB rob;
};
struct PRFInner {
  std::array<PRFEntry, PRF_CAP> PhysicalRegs;
  std::array<Register<PHY_TAG_WIDTH>, PRF_CAP> freeList;
  std::array<Register<PRF_SEQ_WIDTH>, CKPT_CAP> PRFHeadCkpt;
  Register<PRF_SEQ_WIDTH> headSeq;
  Register<PRF_SEQ_WIDTH> tailSeq;
  Register<1> bootDone;
};
struct PRF : public dark::Module<PRFInput, PRFInner> {
  bool isFreeListEmpty() const { return headSeq == tailSeq; }
  PrfSeq getHeadSeq() const { return static_cast<uint32_t>(headSeq); }
  uint8_t getFreeListSlot(PrfSeq seq) const {
    return static_cast<uint32_t>(freeList[prfSlot(seq)]);
  }
  bool isReady(int index) const {
    return static_cast<bool>(PhysicalRegs[index].ready);
  }
  int32_t getValue(int index) const {
    return static_cast<uint32_t>(PhysicalRegs[index].value);
  }
  bool isOperandReady(const Operand &op) const {
    return op.tag == InvalidPhy || isReady(op.tag);
  }
  int32_t getOperandValue(const Operand &op) const {
    return op.tag == InvalidPhy ? op.imm : getValue(op.tag);
  }
  void work() override;
};
