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
  if (needSquash) {
    for (int i = 0; i < FETCH_REQUEST_CAP; ++i) {
      requestBuffer[i].valid <= false;
      requestBuffer[i].ready <= false;
      requestBuffer[i].laneCount <= 0;
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
    const bool replaceHead = popE && static_cast<bool>(fetchValid) &&
                             occ == FETCH_REQUEST_CAP;
    if (popE) {
      assert(isReturnReady());
      if (!replaceHead) {
        requestBuffer[oh].valid <= false;
        requestBuffer[oh].ready <= false;
        requestBuffer[oh].laneCount <= 0;
      }
      head <= ((oh + 1) & (FETCH_REQUEST_CAP - 1));
    }
    if (static_cast<bool>(fillFire)) {
      const uint32_t la = static_cast<uint32_t>(lineReturn.lineAddr);
      auto cachelineIndex = (la >> ICACHE_OFFSET_BITS) & ICACHE_INDEX_MASK;
      blocks[cachelineIndex].valid <= true;
      blocks[cachelineIndex].tag <= la >> ICACHE_TAG_SHIFT;
      uint32_t controlMask = 0;
      for (unsigned word = 0; word < ICACHE_WORDS_PER_LINE; ++word)
        if (isControl(uint32_t(lineReturn.data[word]))) controlMask |= 1u << word;
      blocks[cachelineIndex].controlMask <= controlMask;
      const auto slot = refillSlot();
      const uint32_t pc = static_cast<uint32_t>(requestBuffer[slot].PC[0]);
      const unsigned cnt = static_cast<uint32_t>(requestBuffer[slot].laneCount);
      for (unsigned L = 0; L < cnt; ++L)
        requestBuffer[slot].raw_inst[L] <= static_cast<uint32_t>(
            lineReturn.data[((pc + 4u * L) >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK]);
      requestBuffer[slot].ready <= true;
    }
    if (static_cast<bool>(fetchValid)) {
      const uint32_t pc = static_cast<uint32_t>(fetchPC);
      const bool forwarded = refillHit(pc);
      const bool isHit = hit(pc) || forwarded;
      if (isHit && !forwarded) {
        readIndex <= static_cast<uint32_t>((head + occ) & (FETCH_REQUEST_CAP - 1));
        readLaneIndex <= static_cast<uint32_t>(
            (fetchPC >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK);
      }
      assert(occ < static_cast<uint32_t>(FETCH_REQUEST_CAP) || replaceHead);
      auto index = (oh + occ) & (FETCH_REQUEST_CAP - 1);
      for (unsigned L = 0; L < FETCH_WIDTH; ++L) {
        requestBuffer[index].raw_inst[L] <= (forwarded ? static_cast<uint32_t>(
            lineReturn.data[((pc + 4u * L) >> RV32_WORD_BYTE_BITS) & ICACHE_WORD_INDEX_MASK]) : 0u);
        requestBuffer[index].PC[L] <= pc + 4u * L;
        requestBuffer[index].predictPC[L] <= static_cast<uint32_t>(fetchPredictPC[L]);
      }
      requestBuffer[index].ckptId <= static_cast<uint32_t>(fetchCkptId);
      requestBuffer[index].valid <= true;
      requestBuffer[index].laneCount <= static_cast<uint32_t>(fetchCount);
      requestBuffer[index].ready <= forwarded;
      if (isHit)
        ++statHits;
      else
        ++statMisses;
    }
    if (static_cast<bool>(readValid) && !readConsumed) {
      requestBuffer[static_cast<uint32_t>(readIndex)].ready <= true;
      const unsigned cnt = static_cast<uint32_t>(
          requestBuffer[static_cast<uint32_t>(readIndex)].laneCount);
      for (unsigned L = 0; L < cnt; ++L)
        requestBuffer[static_cast<uint32_t>(readIndex)].raw_inst[L] <=
            ICacheData.readLane(static_cast<uint32_t>(readLaneIndex) + L);
    }
    readValid <= (static_cast<bool>(fetchValid) &&
                  hit(static_cast<uint32_t>(fetchPC)) &&
                  static_cast<bool>(ICacheData.enable) &&
                  !static_cast<bool>(ICacheData.writeEnable));
    ICacheData.work();
  }
}
