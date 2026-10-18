#pragma once
#include "MemoryBus.hpp"
#include "module.h"
#include "tools.h"

struct AXIDataInput {
  Wire<1> readValid;
  Wire<32> readAddress;
  Wire<3> readTransaction;
  Wire<1> writeValid;
  Wire<32> writeAddress;
  Wire<3> writeTransaction;
  std::array<Wire<8>, DCACHE_BLOCK_CAP> writeLineData;
  Wire<1> replyConsumed, writeConsumed, exitValid;
  Wire<32> exitValue;
  Wire<1> arReady, rValid;
  Wire<32> rData;
  Wire<2> rResp;
  Wire<1> awReady, wReady, bValid;
  Wire<2> bResp;
};
struct AXIDataOutput {
  Wire<1> arValid;
  Wire<32> arAddr;
  Wire<1> rReady, awValid;
  Wire<32> awAddr;
  Wire<1> wValid;
  Wire<32> wData;
  Wire<4> wStrb;
  Wire<1> bReady;
};
struct AXIDataEntry {
  Register<32> address;
  Register<3> transaction;
  Register<std::bit_width(static_cast<uint32_t>(DCACHE_WORDS_PER_LINE))> issued, collected, w;
  Register<1> mmio;
};
struct AXIDataInner {
  std::array<AXIDataEntry, AXI_DATA_CAP> entries;
  std::array<Register<4>, AXI_ORDER_CAP> readOrder, writeOrder;
  std::array<std::array<Register<32>, DCACHE_WORDS_PER_LINE>, AXI_DATA_CAP> payload;
  Register<AXI_DATA_CAP> dataBusy;
  Register<4> rh, wh;
  Register<5> rn, wn;
  Register<5> rt, wt, ra, rr, wa, wd, wb;
};
struct AXIData : dark::Module<AXIDataInput, AXIDataOutput, AXIDataInner> {
  AXIData();
  void work() override;
  uint32_t freeCredits() const;
  bool isReplyReady() const;
  bool isWriteReplyReady() const;
  uint8_t lineByte(int byte) const;
  uint32_t replyAddress() const;
  uint32_t replyTransaction() const;
  uint32_t writeReplyAddress() const;
  uint32_t writeReplyTransaction() const;
  bool exitReady() const;
private:
  int readIssue() const;
  int writeIssue(bool address) const;
  int readCollect() const;
  int writeCollect() const;
  const AXIDataEntry &readEntry(int slot) const { return entries[static_cast<uint32_t>(readOrder[slot])]; }
  const AXIDataEntry &writeEntry(int slot) const { return entries[static_cast<uint32_t>(writeOrder[slot])]; }
};
