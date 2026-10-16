#pragma once
#include "common.h"
#include "module.h"
#include "tools.h"
#include "SRAM.hpp"
#include <array>
#include <cstdint>

// Set-associative, write-back/write-allocate DCache mirroring the main tree:
// 4-entry RequestBuffer, head-only SRAM lookup, DMEM dual-port refill,
// tree-PLRU replacement. SRAMs are nested and clocked once per work();
// outputs are combinational Wires over _M_old so no extra stage is added.
struct DCacheInput {
  Wire<1> squashNeed;
  Wire<ROB_TAG_WIDTH> squashTag;
  Wire<1> decisionValid;
  Wire<5> decisionOp;
  Wire<32> decisionValue;
  Wire<32> decisionAddr;
  Wire<1> decisionIsSigned;
  Wire<2> decisionNBytes;
  Wire<ROB_TAG_WIDTH> decisionRobTag;
  Wire<7> decisionMemIndex;
  Wire<1> dmemReadBusy;
  Wire<1> dmemWriteBusy;
  Wire<1> dmemReplyReady;
  std::array<Wire<8>, DCACHE_BLOCK_CAP> dmemLineData;
};
struct DCacheOutput {
  Wire<1> busyOut;
  Wire<1> loadRespValid;
  Wire<7> loadRespMemIndex;
  Wire<ROB_TAG_WIDTH> loadRespRobTag;
  Wire<32> loadRespValue;
  Wire<1> reqReadValid;
  Wire<32> reqReadAddr;
  Wire<1> reqWriteValid;
  Wire<32> reqWriteAddr;
  std::array<Wire<8>, DCACHE_BLOCK_CAP> reqWriteLineData;
  Wire<1> fillFire;
};
struct DCacheEntry {
  Register<5> op;
  Register<32> value;
  Register<32> addr;
  Register<1> isSigned;
  Register<2> nEnc;
  Register<ROB_TAG_WIDTH> robTag;
  Register<7> memIndex;
  Register<32> raw;
  Register<DCACHE_TAG_WIDTH> tag;
  Register<2> way; // Hit way, or the selected victim way on a miss.
  Register<1> lineValid;
  Register<1> lineDirty;
  Register<1> needReadMemory;
  Register<1> cancelled;
  Register<1> valid;
  void sync() {
    dark::Visitor::sync(op);
    dark::Visitor::sync(value);
    dark::Visitor::sync(addr);
    dark::Visitor::sync(isSigned);
    dark::Visitor::sync(nEnc);
    dark::Visitor::sync(robTag);
    dark::Visitor::sync(memIndex);
    dark::Visitor::sync(raw);
    dark::Visitor::sync(tag);
    dark::Visitor::sync(way);
    dark::Visitor::sync(lineValid);
    dark::Visitor::sync(lineDirty);
    dark::Visitor::sync(needReadMemory);
    dark::Visitor::sync(cancelled);
    dark::Visitor::sync(valid);
  }
};
struct DCacheInner {
  Register<DCACHE_REQUEST_BITS> head;
  Register<DCACHE_COUNT_BITS> count;
  Register<DCACHE_REQUEST_BITS> readIndex;
  Register<DCACHE_OFFSET_BITS> readLaneIndex;
  Register<2> readNEnc;
  Register<1> readValid;
  Register<4> readWayMask; // Per-way validity sampled with the synchronous SRAM read.
  Register<1> memReadSent;
  Register<1> memWriteSent;
  Register<1> loadResponded; // One transport response per head, independent of refill retirement.
  Register<DCACHE_WORD_INDEX_BITS> fillWord;
  std::array<DCacheEntry, REQUEST_CAP> requestBuffer;
  std::array<Register<8>, DCACHE_BLOCK_CAP> victimLine;
  std::array<std::array<SRAM<NUM_OF_DCACHE_SETS, 32, 8>, DCACHE_WORDS_PER_LINE>,
             NUM_OF_DCACHE_WAYS>
      dataSrams;
  std::array<SRAM<NUM_OF_DCACHE_SETS, DCACHE_META_BITS>, NUM_OF_DCACHE_WAYS>
      metaSrams;
  std::array<std::array<Register<1>, NUM_OF_DCACHE_WAYS>, NUM_OF_DCACHE_SETS>
      validBits;
  std::array<Register<3>, NUM_OF_DCACHE_SETS> plru; // Tree-PLRU; unused when direct-mapped.
};
struct DCache : dark::Module<DCacheInput, DCacheOutput, DCacheInner> {
  DCache() { wire_output(); }
  void work() override;
  bool isEmpty() const { return static_cast<uint32_t>(count) == 0; }
  bool isRequestFull() const { return static_cast<uint32_t>(count) == REQUEST_CAP; }
  // host-only: reporting counters, never consumed by datapath logic.
  uint64_t statHits = 0;
  uint64_t statMisses = 0;

 private:
  void wire_output();
  static int decodeNBytes(uint32_t enc);
  uint32_t headAddr() const;
  uint32_t headSet() const;
  uint32_t headWay() const;
  uint32_t storeWay() const;
  int hitWay() const;
  uint32_t selectVictim(uint32_t set) const;
  bool needsLookup() const;
  bool readHit() const;
  bool headReadReady() const;
  bool isReturnReady() const;
  bool headIsStore() const;
  bool headNeedMemory() const;
  bool headDirty() const;
  bool needsMemRead() const;
  bool needsMemWrite() const;
  bool canFill() const;
  uint32_t fillWordIndex() const;
  bool fillIsLast() const;
  bool fillActive() const;
  bool fillDone() const;
  bool storeHit() const;
  bool storeTouchesBank(uint32_t bank) const;
  uint32_t victimAddr() const;
  bool hitLoadReady() const;
  bool fillLoadReady() const;
  bool sramWantWrite() const;
  bool dataBankWantWrite(uint32_t bank) const;
  bool dataBankWantWrite(uint32_t way, uint32_t bank) const;
  bool metaWantWrite() const;
  bool metaWantWrite(uint32_t way) const;
  bool sramWantRead() const;
  uint32_t readRaw() const;
  uint32_t hitRaw() const;
  uint32_t fillRaw() const;
  bool loadSquashGuard(uint32_t rob) const;
};
