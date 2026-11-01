#pragma once
#include "Commit.hpp"
#include "common.h"
#include "module.h"
#include "Recovery.hpp"
#include "SRAM.hpp"
#include <array>
#include <cstdint>
struct PRFEntry {
  Register<1> ready;
  Register<1> everWritten;
};
constexpr unsigned PRF_VALUE_WRITERS = ISSUE_WIDTH + WRITEBACK_PORTS;
struct PRFWriteRecord {
  Register<1> valid;
  Register<PHY_TAG_WIDTH> phy;
  Register<32> value;
};
struct PRFValuePort {
  Wire<1> valid, ready;
  Wire<PHY_TAG_WIDTH> phy;
  Wire<32> value;
};
struct PRFValueWrite {
  bool valid = false, ready = false;
  uint32_t value = 0;
};
struct PRFInputSquash {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
};
// Dedicated ALU ports and one age-selected MUL/DIV/LQ port.
struct PRFInputCDBAlu {
  Wire<1> cdbValid;
  Wire<32> cdbValue;
  Wire<ROB_TAG_WIDTH> cdbRobTag;
  Wire<1> cdbIsControl;
  Wire<PHY_TAG_WIDTH> cdbNewPhy;
};
struct PRFInputCDBShared {
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
  Wire<ROB_TAG_WIDTH> robNextTag;
  Wire<PHY_TAG_WIDTH> robHeadOldPhy;
};
struct PRFInput {
  Wire<1> freeze;
  std::array<CommitPort, COMMIT_WIDTH> commits;
  std::array<RecoveryNarrowPort, RECOVERY_WIDTH> replay;
  RecoveryInput recovery;
  PRFInputSquash squash;
  PRFInputCDBAlu cdbOfALU;
  PRFInputCDBAlu cdbOfALUSecond;
  PRFInputCDBShared cdbOfShared;
  std::array<PRFInputIssue, ISSUE_WIDTH> issue;
  PRFInputROB rob;
};
struct PRFInner {
  std::array<PRFEntry, PRF_CAP> PhysicalRegs;
  std::array<SRAM<1, 32>, PRF_CAP - 1> payload;
  std::array<PRFWriteRecord, PRF_VALUE_WRITERS> priorWrites;
  std::array<PRFValuePort, PRF_VALUE_WRITERS> valuePorts;
  std::array<Register<PHY_TAG_WIDTH>, PRF_CAP> freeList;
  Register<PRF_SEQ_WIDTH> headSeq;
  Register<PRF_SEQ_WIDTH> tailSeq;
  Register<1> bootDone;
};
struct PRF : public dark::Module<PRFInput, PRFInner> {
  PRF();
  bool isFreeListEmpty() const { return headSeq == tailSeq; }
  PrfSeq getHeadSeq() const { return static_cast<uint32_t>(headSeq); }
  PrfSeq getTailSeq() const { return static_cast<uint32_t>(tailSeq); }
  bool hasFree(unsigned n) const {
    return prfSeqDistance(static_cast<uint32_t>(headSeq),
                          static_cast<uint32_t>(tailSeq)) >= n;
  }
  uint8_t getFreeListSlot(PrfSeq seq) const {
    return static_cast<uint32_t>(freeList[prfSlot(seq)]);
  }
  bool isReady(int index) const {
    return index == 0 || static_cast<bool>(PhysicalRegs[index].ready);
  }
  int32_t getValue(int index) const {
    uint32_t value = 0;
    if (index != 0) {
      if (bool(PhysicalRegs[index].everWritten)) value = payload[index - 1].readLane(0);
      for (const auto &port : priorWrites)
        if (bool(port.valid) && uint32_t(port.phy) == uint32_t(index)) value = uint32_t(port.value);
    }
    return static_cast<int32_t>(value);
  }
  bool isOperandReady(const Operand &op) const {
    return op.tag == InvalidPhy || isReady(op.tag);
  }
  int32_t getOperandValue(const Operand &op) const {
    return op.tag == InvalidPhy ? op.imm : getValue(op.tag);
  }
  bool isOperandReady(const Operand &op, const OperandWriteback &ports) const {
    bool ready = isOperandReady(op);
    for (const auto &port : ports)
      ready |= op.tag != InvalidPhy && port.valid && port.tag == op.tag;
    return ready;
  }
  int32_t getOperandValue(const Operand &op, const OperandWriteback &ports) const {
    if (isOperandReady(op)) return getOperandValue(op);
    for (const auto &port : ports)
      if (port.valid && port.tag == op.tag && op.tag != InvalidPhy)
        return port.value;
    return getOperandValue(op);
  }
  void work() override;
private:
  PRFValueWrite writeFor(unsigned phy) const;
};
