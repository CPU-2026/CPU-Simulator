#pragma once
#include "MemoryBus.hpp"
#include "SRAM.hpp"
#include "module.h"
#include "tools.h"

struct DCacheInput {
  Wire<1> squashNeed;
  Wire<ROB_TAG_WIDTH> squashTag;
  Wire<1> decisionValid;
  Wire<5> decisionOp;
  Wire<32> decisionValue, decisionAddr;
  Wire<1> decisionIsSigned;
  Wire<2> decisionNBytes;
  Wire<ROB_TAG_WIDTH> decisionRobTag;
  Wire<7> decisionMemIndex;
  Wire<1> dmemWriteReady, dmemReplyReady, dmemWriteReplyReady, exitReady;
  Wire<5> dmemCredits;
  Wire<3> replyTransaction, writeTransaction;
  Wire<32> replyAddress, writeAddress;
  Wire<1> wordValid;
  Wire<32> wordAddress, wordData;
  Wire<3> wordTransaction;
  std::array<Wire<8>, DCACHE_BLOCK_CAP> dmemLineData;
};
struct DCacheOutput {
  Wire<1> busyOut, loadRespValid;
  Wire<7> loadRespMemIndex;
  Wire<ROB_TAG_WIDTH> loadRespRobTag;
  Wire<32> loadRespValue;
  Wire<1> reqReadValid;
  Wire<32> reqReadAddr;
  Wire<3> reqReadTransaction;
  Wire<DCACHE_WORD_INDEX_BITS> reqReadStartWord;
  Wire<1> reqWriteValid;
  Wire<32> reqWriteAddr;
  Wire<3> reqWriteTransaction;
  std::array<Wire<8>, DCACHE_BLOCK_CAP> reqWriteLineData;
  Wire<1> fillFire, writeConsumed, exitValid;
  Wire<32> exitValue;
};
struct DCacheEntry {
  Register<5> op;
  Register<32> value, address;
  Register<1> isSigned;
  Register<2> nEnc;
  Register<ROB_TAG_WIDTH> robTag;
  Register<7> memIndex;
  Register<2> way;
  Register<3> miss; // 0 = none; MSHR m is encoded m+1 (zero-reset safe).
  Register<1> active, ready, deferred, cancelled;
};
struct DCacheMiss {
  Register<32> line;
  Register<DCACHE_TAG_WIDTH> victimTag;
  Register<DCACHE_REQUEST_BITS> owner;
  Register<2> way;
  Register<std::bit_width(static_cast<uint32_t>(DCACHE_WORDS_PER_LINE))> fillWord;
  Register<1> valid, dirty, readSent, writeSent, writeDone, responded;
};
struct DCachePlan {
  int lookup = -1, response = -1, store = -1, mergeSlot = -1, mergeMiss = -1;
  int lookupCandidate = -1;
  int hitLocationTrain = -1;
  bool storeHintFire = false;
  uint8_t storeHintWay = 0;
  bool acceptLookup = false;
  int allocate = -1, fillMiss = -1, writeAck = -1;
  bool fillBeat = false, fillFire = false, wordReturn = false, loadValid = false, exitValid = false;
  int wordMiss = -1;
  uint8_t wordCapture = 0;
  uint32_t loadValue = 0, exitValue = 0;
  uint32_t readHitRaw = 0; // One read grant, one shared extraction.
  CacheMemoryRequest memoryRequest{};
  std::array<std::array<CacheSRAMPort<4>, DCACHE_WORDS_PER_LINE>, NUM_OF_DCACHE_WAYS> data{};
  std::array<CacheSRAMPort<1>, NUM_OF_DCACHE_WAYS> meta{};
};
// A combinational cache, exactly like Wire's per-cycle memoization. It holds
// events/ports only, never a next-state copy; sync invalidates it after work.
struct DCacheComb {
  mutable bool valid = false;
  mutable DCachePlan events{};
  void sync() { valid = false; }
};
struct DCacheInner {
  std::array<DCacheEntry, REQUEST_CAP> requests;
  std::array<DCacheMiss, DCACHE_MSHR_CAP> misses;
  std::array<Register<32>, REQUEST_CAP> rawPayload;
  std::array<std::array<Register<32>, DCACHE_WORDS_PER_LINE>, DCACHE_MSHR_CAP> victimPayload;
  std::array<Register<REQUEST_CAP>, REQUEST_CAP> olderMask;
  Register<DCACHE_COUNT_BITS> count;
  Register<DCACHE_REQUEST_BITS> cursor, readIndex;
  Register<NUM_OF_DCACHE_WAYS> readWayMask;
  Register<1> readValid;
  Register<DCACHE_OFFSET_BITS> readOffset;
  Register<2> readNEnc;
  std::array<std::array<SRAM<NUM_OF_DCACHE_SETS, 32, 8>, DCACHE_WORDS_PER_LINE>, NUM_OF_DCACHE_WAYS> datas;
  std::array<SRAM<NUM_OF_DCACHE_SETS, DCACHE_META_BITS>, NUM_OF_DCACHE_WAYS> metadata;
  std::array<std::array<Register<1>, NUM_OF_DCACHE_WAYS>, NUM_OF_DCACHE_SETS> validBits;
  std::array<Register<DCACHE_PLRU_BITS>, NUM_OF_DCACHE_SETS> plru;
  // Only exact Load-hit location metadata, submitted with the other old/new state.
  std::array<Register<1>, 4> hitLocationValid;
  std::array<Register<32 - DCACHE_OFFSET_BITS>, 4> hitLocationLine;
  std::array<Register<DCACHE_WAY_BITS>, 4> hitLocationWay;
  Register<2> hitLocationVictim;
  DCacheComb comb;
};
struct DCache : dark::Module<DCacheInput, DCacheOutput, DCacheInner> {
  DCache();
  void work() override;
  bool isEmpty() const { return static_cast<uint32_t>(count) == 0; }
  bool isRequestFull() const { return static_cast<uint32_t>(count) == REQUEST_CAP; }
  uint64_t statHits = 0, statMisses = 0; // host-only reporting
private:
  CacheRequest request(int slot) const;
  int miss(int slot) const;
  uint8_t activeMask() const;
  int oldestRequest(uint8_t mask) const;
  int oldestMiss(uint8_t mask) const;
  int freeRequest() const;
  int freeMiss() const;
  int findMiss(uint32_t line) const;
  bool setLocked(uint32_t addr) const;
  bool storeHazard(int slot, bool allocating) const;
  bool victimHazard(int id) const;
  uint32_t victimAddress(int id) const;
  int hitWay() const;
  uint32_t selectVictim(uint32_t set) const;
  uint32_t sramRaw(int way) const;
  uint32_t memoryRaw(int slot) const;
  uint32_t wordRaw(int slot, uint32_t data) const;
  DCachePlan plan() const;
  const DCachePlan &events() const;
};
