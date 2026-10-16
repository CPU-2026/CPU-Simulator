#pragma once
#include "common.h"
#include "module.h"
#include "tools.h"
#include <array>
#include <cstdint>

// Data-side AXI4-Lite adapter. Keeps DMEM's upstream contract -- one line
// fill read port and one line writeback port, run in parallel -- but the
// 64-byte line is moved as DCACHE_WORDS_PER_LINE single-beat AXI transfers
// (reads on AR/R, writebacks on AW/W/B with full byte strobes).
struct AXIDataInput {
  Wire<1> readValid;
  Wire<32> readAddress;
  Wire<1> writeValid;
  Wire<32> writeAddress;
  std::array<Wire<8>, DCACHE_BLOCK_CAP> writeLineData;
  Wire<1> replyConsumed;
  Wire<1> arReady;
  Wire<1> rValid;
  Wire<32> rData;
  Wire<1> awReady;
  Wire<1> wReady;
  Wire<1> bValid;
};
struct AXIDataOutput {
  Wire<1> arValid;
  Wire<32> arAddr;
  Wire<1> rReady;
  Wire<1> awValid;
  Wire<32> awAddr;
  Wire<1> wValid;
  Wire<32> wData;
  Wire<4> wStrb;
  Wire<1> bReady;
};
struct AXIDataInner {
  // Fill port: one outstanding line read plus the reply buffer.
  Register<1> readActive;
  Register<32> readAddr;
  Register<5> readIssued;
  Register<5> readCollected;
  std::array<Register<RV32_WORD_BITS>, DCACHE_WORDS_PER_LINE> readWords;
  Register<1> replyValid;
  // Writeback port: one outstanding line write plus the latched line.
  Register<1> writeActive;
  Register<32> writeAddr;
  Register<5> writeAw;
  Register<5> writeW;
  Register<5> writeB;
  std::array<Register<RV32_WORD_BITS>, DCACHE_WORDS_PER_LINE> writeWords;
};
struct AXIData : dark::Module<AXIDataInput, AXIDataOutput, AXIDataInner> {
  AXIData();
  bool isReadBusy() const { return static_cast<bool>(readActive); }
  bool isWriteBusy() const { return static_cast<bool>(writeActive); }
  bool isReplyReady() const { return static_cast<bool>(replyValid); }
  // One byte of the completed line read (DCache fill path).
  uint8_t lineByte(int i) const;
  void work() override;
};
