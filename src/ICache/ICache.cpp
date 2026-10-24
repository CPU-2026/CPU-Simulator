#include "../include/ICache.hpp"
#include "common.h"
#include <cassert>
#include <cstdint>

ICache::ICache() {
  fillFire = [this]() {
    return !static_cast<bool>(needSquash) && refillValid();
  };
  readBlocked = [this]() {
    const uint32_t pc = static_cast<uint32_t>(fetchPC);
    return static_cast<bool>(fillFire) && hit(pc) && !refillHit(pc);
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
      requestBuffer[i].valid1 <= false;
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
    const bool replaceHead = popE && static_cast<bool>(fetchValid) &&
                             occ == REQUEST_CAP;
    if (popE) {
      assert(isReturnReady());
      if (!replaceHead) {
        requestBuffer[oh].valid <= false;
        requestBuffer[oh].ready <= false;
        requestBuffer[oh].valid1 <= false;
      }
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
      requestBuffer[slot].raw_inst1 <= static_cast<uint32_t>(
          lineReturn.data[((pc + 4u) >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK]);
      requestBuffer[slot].ready <= true;
    }
    // Refill forwarding completes the new slot at the edge, without another
    // SRAM access. Ordinary hits retain their synchronous-read boundary.
    if (static_cast<bool>(fetchValid)) {
      const uint32_t pc = static_cast<uint32_t>(fetchPC);
      const bool forwarded = refillHit(pc);
      const bool isHit = hit(pc) || forwarded;
      if (isHit && !forwarded) {
        readIndex <= static_cast<uint32_t>((head + occ) & (REQUEST_CAP - 1));
        readLaneIndex <= static_cast<uint32_t>(
            (fetchPC >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK);
      }
      assert(occ < static_cast<uint32_t>(REQUEST_CAP) || replaceHead);
      auto index = (oh + occ) & (REQUEST_CAP - 1);
      requestBuffer[index].raw_inst <= (forwarded ? static_cast<uint32_t>(
          lineReturn.data[(pc >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK]) : 0u);
      requestBuffer[index].raw_inst1 <= (forwarded ? static_cast<uint32_t>(
          lineReturn.data[((pc + 4u) >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK]) : 0u);
      requestBuffer[index].PC <= pc;
      requestBuffer[index].PC1 <= pc + 4u;
      requestBuffer[index].predictPC <= static_cast<uint32_t>(fetchPredictPC);
      requestBuffer[index].predictPC1 <= static_cast<uint32_t>(fetchPredictPC1);
      requestBuffer[index].ckptId <= static_cast<uint32_t>(fetchCkptId);
      requestBuffer[index].valid <= true;
      requestBuffer[index].valid1 <= static_cast<bool>(fetchValid1);
      requestBuffer[index].ready <= forwarded;
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
      if (static_cast<bool>(
              requestBuffer[static_cast<uint32_t>(readIndex)].valid1)) {
        requestBuffer[static_cast<uint32_t>(readIndex)].raw_inst1 <=
            ICacheData.readLane(static_cast<uint32_t>(readLaneIndex) + 1);
      }
    }
    readValid <= (static_cast<bool>(fetchValid) &&
                  hit(static_cast<uint32_t>(fetchPC)) &&
                  static_cast<bool>(ICacheData.enable) &&
                  !static_cast<bool>(ICacheData.writeEnable));
    ICacheData.work();
  }
}
