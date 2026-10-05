#include "../include/ICache.hpp"
#include "common.h"
#include <cassert>
#include <cstdint>

ICache::ICache() {
  fillFire = [this]() {
    return !static_cast<bool>(needSquash) && refillValid();
  };
  readBlocked = [this]() {
    return static_cast<bool>(fillFire) && hit(static_cast<uint32_t>(fetchPC));
  };
  // The nested SRAM owns its port. ICache::work() clocks it once; the parent
  // Module::sync() commits readData and clears every input/output Wire cache.
  ICacheData.enable = [this]() {
    return !static_cast<bool>(needSquash) &&
           (static_cast<bool>(fillFire) || static_cast<bool>(fetchValid));
  };
  ICacheData.writeEnable = [this]() { return static_cast<bool>(fillFire); };
  ICacheData.addr = [this]() {
    const uint32_t addr = static_cast<bool>(fillFire)
                              ? static_cast<uint32_t>(lineReturn.lineAddr)
                              : static_cast<uint32_t>(fetchPC);
    return (addr >> ICACHE_OFFSET_BITS) & ICACHE_INDEX_MASK;
  };
  for (int w = 0; w < ICACHE_WORDS_PER_LINE; ++w) {
    ICacheData.writeMask[w] = []() { return 1u; };
    ICacheData.writeData[w] = [this, w]() {
      return static_cast<uint32_t>(lineReturn.data[w]);
    };
  }
}

void ICache::work() {
  // stage 3 flush: clear the speculative request queue (cache lines kept)
  if (needSquash) {
    for (int i = 0; i < REQUEST_CAP; ++i) {
      requestBuffer[i].valid <= false;
      requestBuffer[i].ready <= false;
    }
    head <= 0;
    readIndex <= 0;
    readLaneIndex <= 0;
    readValid <= 0;
  } else {
    const bool popE = static_cast<bool>(popConsume);
    const bool readConsumed = popE && headReadReady();
    const uint32_t oh = static_cast<uint32_t>(head);
    const uint32_t occ = occupancy();
    // stage 2 pop: self-release once FQ consumed the ICache head
    // (write-own-only)
    if (popE) {
      assert(isReturnReady());
      requestBuffer[oh].valid <= false;
      requestBuffer[oh].ready <= false;
      head <= ((oh + 1) & (REQUEST_CAP - 1));
    }
    // Complete exactly the live miss selected from old state. The same
    // fillFire consumes IMEM's return and grants the SRAM write port.
    if (static_cast<bool>(fillFire)) {
      const uint32_t la = static_cast<uint32_t>(lineReturn.lineAddr);
      auto cachelineIndex = (la >> ICACHE_OFFSET_BITS) & ICACHE_INDEX_MASK;
      blocks[cachelineIndex].valid <= true;
      blocks[cachelineIndex].tag <= la >> ICACHE_TAG_SHIFT;
      const auto slot = refillSlot();
      const uint32_t pc = static_cast<uint32_t>(requestBuffer[slot].PC);
      requestBuffer[slot].raw_inst <= static_cast<uint32_t>(
          lineReturn.data[(pc >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK]);
      requestBuffer[slot].ready <= true;
    }
    // Both hits and misses occupy a placeholder. A hit can bypass at the
    // head next cycle; otherwise the previous SRAM output lands in its slot.
    if (static_cast<bool>(fetchValid)) {
      const uint32_t pc = static_cast<uint32_t>(fetchPC);
      bool isHit = hit(pc);
      // host-only: accepted-fetch classification for cross-tree trace diagnosis.
      if (debug::enabled(debug::TOPIC_MEM))
        debug::print("IC_ACCEPT idx=%llu pc=%08x hit=%u\n",
                     static_cast<unsigned long long>(statHits + statMisses), pc,
                     static_cast<unsigned>(isHit));
      if (isHit) {
        readIndex <= static_cast<uint32_t>((head + occ) & (REQUEST_CAP - 1));
        readLaneIndex <= static_cast<uint32_t>(
            (fetchPC >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK);
      }
      assert(occ < static_cast<uint32_t>(REQUEST_CAP));
      auto index = (oh + occ) & (REQUEST_CAP - 1);
      requestBuffer[index].raw_inst <= 0;
      requestBuffer[index].PC <= pc;
      requestBuffer[index].predictPC <= static_cast<uint32_t>(fetchPredictPC);
      requestBuffer[index].ckptId <= static_cast<uint32_t>(fetchCkptId);
      requestBuffer[index].valid <= true;
      requestBuffer[index].ready <= false;
      // host-only profile counters (never read by the datapath)
      if (isHit)
        ++statHits;
      else
        ++statMisses;
    }
    // A bypassed head read is already popped. Skip its landing to avoid a
    // second ready write; backpressured/younger reads still need preservation.
    if (static_cast<bool>(readValid) && !readConsumed) {
      requestBuffer[static_cast<uint32_t>(readIndex)].ready <= true;
      requestBuffer[static_cast<uint32_t>(readIndex)].raw_inst <=
          ICacheData.readLane(static_cast<uint32_t>(readLaneIndex));
    }
    readValid <= (static_cast<bool>(fetchValid) &&
                  hit(static_cast<uint32_t>(fetchPC)) &&
                  static_cast<bool>(ICacheData.enable) &&
                  !static_cast<bool>(ICacheData.writeEnable));
    ICacheData.work();
  }
}
