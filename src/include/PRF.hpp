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
  Wire<7> SquashTag;
  Wire<8> CkptId;
};
struct PRFInputLQ {
  Wire<8> lqHead;
  std::array<Wire<1>, LQ_CAP> lqActive;
  std::array<Wire<1>, LQ_CAP> lqReadyToCommit;
  std::array<Wire<7>, LQ_CAP> lqRobTags;
  std::array<Wire<32>, LQ_CAP> lqValues;
  std::array<Wire<1>, LQ_CAP> lqHasOlderUnresolved;
  std::array<Wire<7>, LQ_CAP> lqNewPhys;
};
struct PRFInputCDB {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<7> cdbRobTag;
  Wire<1> cdbIsControl;
  Wire<7> cdbNewPhy;
};
struct PRFInputIssue {
  Wire<1> issueValid;
  Wire<7> issuePhy;
  Wire<1> issueAllocDest;
  Wire<32> issuePC;
  Wire<1> issueIsControl;
  Wire<8> issueCkptId;
};
struct PRFInputROB {
  Wire<8> robHead;
  Wire<1> isRobEmpty;
  Wire<1> isRobHeadCommitReady;
  Wire<1> robHeadIsHalt;
  Wire<2> robHeadType;
  Wire<7> robHeadOldPhy;
};
struct PRFInput {
  PRFInputSquash squash;
  PRFInputLQ lq;
  PRFInputCDB cdb;
  PRFInputIssue issue;
  PRFInputROB rob;
};
struct PRFInner {
  std::array<PRFEntry, PRF_CAP> PhysicalRegs;
  std::array<Register<7>, PRF_CAP> freeList;
  std::array<Register<32>, CKPT_CAP> PRFHeadCkpt;
  Register<32> headSeq;
  Register<32> tailSeq;
  Register<1> bootDone;
};
struct PRF : public dark::Module<PRFInput, PRFInner> {
  bool isFreeListEmpty() const { return headSeq == tailSeq; }
  uint32_t getHeadSeq() const { return static_cast<uint32_t>(headSeq); }
  uint8_t getFreeListSlot(uint32_t seq) const {
    return static_cast<uint32_t>(freeList[seq & (PRF_CAP - 1)]);
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
