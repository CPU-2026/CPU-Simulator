#include "../include/DCache.hpp"
#include "ROB.hpp"
#include "Memory.hpp"
#include "common.h"
#include <cassert>
#include <cstdint>

int DCache::decodeNBytes(uint32_t enc) { return enc == 0 ? 1 : (enc == 1 ? 2 : 4); }

uint32_t DCache::headAddr() const {
  return static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].addr);
}
uint32_t DCache::headSet() const { return (headAddr() >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK; }
uint32_t DCache::headWay() const {
  return static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].way);
}
uint32_t DCache::storeWay() const {
  const int way = hitWay();
  if (headReadReady() && way >= 0) return static_cast<uint32_t>(way);
  return headWay();
}
int DCache::hitWay() const {
  if (!static_cast<bool>(readValid)) return -1;
  const uint32_t mask = static_cast<uint32_t>(readWayMask);
  const auto ri = static_cast<uint32_t>(readIndex);
  const uint32_t reqTag =
      (static_cast<uint32_t>(requestBuffer[ri].addr) >> DCACHE_TAG_SHIFT) &
      DCACHE_META_TAG_MASK;
  int way = -1;
  for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
    if (way < 0 && (mask & (1u << static_cast<uint32_t>(w))) &&
        (metaSrams[w].readLane(0) & DCACHE_META_TAG_MASK) == reqTag)
      way = w;
  }
  return way;
}
uint32_t DCache::selectVictim(uint32_t set) const {
  uint32_t victim = dcachePlruVictim(static_cast<uint32_t>(plru[set]));
  bool found = false;
  for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
    if (!found && !static_cast<bool>(validBits[set][w])) {
      victim = static_cast<uint32_t>(w);
      found = true;
    }
  }
  return victim;
}
bool DCache::needsLookup() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  if (static_cast<bool>(readValid)) return false;
  const auto h = static_cast<uint32_t>(head);
  if (static_cast<bool>(requestBuffer[h].valid)) return false;
  if (static_cast<bool>(requestBuffer[h].needReadMemory)) return false;
  return true;
}
bool DCache::readHit() const { return hitWay() >= 0; }
bool DCache::headReadReady() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  const auto h = static_cast<uint32_t>(head);
  if (static_cast<bool>(requestBuffer[h].valid)) return false;
  if (!static_cast<bool>(readValid)) return false;
  if (static_cast<uint32_t>(readIndex) != h) return false;
  return readHit();
}
bool DCache::isReturnReady() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  const auto h = static_cast<uint32_t>(head);
  return static_cast<bool>(requestBuffer[h].valid) || headReadReady();
}
bool DCache::headIsStore() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  const auto h = static_cast<uint32_t>(head);
  return static_cast<uint32_t>(requestBuffer[h].op) == static_cast<uint32_t>(Operation::Store);
}
bool DCache::headNeedMemory() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  return static_cast<bool>(requestBuffer[static_cast<uint32_t>(head)].needReadMemory);
}
bool DCache::headDirty() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  const auto h = static_cast<uint32_t>(head);
  return static_cast<bool>(requestBuffer[h].lineValid) &&
         static_cast<bool>(requestBuffer[h].lineDirty);
}
bool DCache::needsMemRead() const {
  return headNeedMemory() && !static_cast<bool>(memReadSent);
}
bool DCache::needsMemWrite() const {
  return headNeedMemory() && headDirty() && !static_cast<bool>(memWriteSent);
}
bool DCache::canFill() const {
  if (!headNeedMemory()) return false;
  if (!static_cast<bool>(memReadSent)) return false;
  if (!static_cast<bool>(dmemReplyReady)) return false;
  if (!headDirty()) return true;
  return static_cast<bool>(memWriteSent) && !static_cast<bool>(dmemWriteBusy);
}
uint32_t DCache::fillWordIndex() const {
  return static_cast<uint32_t>(fillWord);
}
bool DCache::fillIsLast() const {
  return fillWordIndex() ==
         static_cast<uint32_t>(DCACHE_WORDS_PER_LINE - DCACHE_FILL_WORDS_PER_BEAT);
}
bool DCache::fillActive() const { return canFill(); }
bool DCache::fillDone() const { return canFill() && fillIsLast(); }
bool DCache::storeHit() const { return isReturnReady() && headIsStore(); }
bool DCache::storeTouchesBank(uint32_t bank) const {
  if (!storeHit()) return false;
  const auto h = static_cast<uint32_t>(head);
  const uint32_t addr = static_cast<uint32_t>(requestBuffer[h].addr);
  const int n = decodeNBytes(static_cast<uint32_t>(requestBuffer[h].nEnc));
  const uint32_t off = addr & DCACHE_OFFSET_MASK;
  bool selected = false;
  for (int i = 0; i < 4; ++i) {
    if (i < n && addr + static_cast<uint32_t>(i) < MEM_SIZE &&
        ((off + static_cast<uint32_t>(i)) >> 2) == bank)
      selected = true;
  }
  return selected;
}
uint32_t DCache::victimAddr() const {
  const uint32_t set = headSet();
  const auto h = static_cast<uint32_t>(head);
  const uint32_t vtag = static_cast<uint32_t>(requestBuffer[h].tag);
  return ((vtag << DCACHE_INDEX_BITS) | set) << DCACHE_OFFSET_BITS;
}
bool DCache::sramWantWrite() const {
  if (fillActive()) return true;
  return storeHit();
}
bool DCache::dataBankWantWrite(uint32_t bank) const {
  return dataBankWantWrite(headWay(), bank);
}
bool DCache::dataBankWantWrite(uint32_t way, uint32_t bank) const {
  if (fillActive())
    return way == headWay() && bank >= fillWordIndex() &&
           bank < fillWordIndex() + DCACHE_FILL_WORDS_PER_BEAT;
  return storeHit() && way == storeWay() && storeTouchesBank(bank);
}
bool DCache::metaWantWrite() const {
  if (fillDone()) return true;
  return storeHit();
}
bool DCache::metaWantWrite(uint32_t way) const {
  if (fillDone()) return way == headWay();
  if (storeHit()) return way == storeWay();
  return false;
}
bool DCache::sramWantRead() const {
  if (sramWantWrite()) return false;
  return needsLookup();
}
bool DCache::hitLoadReady() const {
  if (!isReturnReady()) return false;
  const auto h = static_cast<uint32_t>(head);
  return static_cast<uint32_t>(requestBuffer[h].op) == static_cast<uint32_t>(Operation::Load);
}
bool DCache::fillLoadReady() const {
  if (!fillActive() || static_cast<bool>(loadResponded)) return false;
  const auto h = static_cast<uint32_t>(head);
  return static_cast<uint32_t>(requestBuffer[h].op) == static_cast<uint32_t>(Operation::Load);
}
uint32_t DCache::readRaw() const {
  const int way = hitWay();
  assert(way >= 0);
  const int n = decodeNBytes(static_cast<uint32_t>(readNEnc));
  const uint32_t lane0 = static_cast<uint32_t>(readLaneIndex);
  uint32_t raw = 0;
  for (int i = 0; i < 4; ++i) {
    if (i < n) {
      const uint32_t lane = lane0 + static_cast<uint32_t>(i);
      raw |= (dataSrams[way][lane >> 2].readLane(lane & 3) & 0xFFu) << (i << 3);
    }
  }
  return raw;
}
uint32_t DCache::hitRaw() const {
  return headReadReady() ? readRaw()
                        : static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].raw);
}
uint32_t DCache::fillRaw() const {
  const auto h = static_cast<uint32_t>(head);
  const int n = decodeNBytes(static_cast<uint32_t>(requestBuffer[h].nEnc));
  const uint32_t off = headAddr() & DCACHE_OFFSET_MASK;
  uint32_t raw = 0;
  for (int i = 0; i < 4; ++i) {
    if (i < n) raw |= (static_cast<uint32_t>(dmemLineData[off + i]) & 0xFFu) << (i << 3);
  }
  return raw;
}
bool DCache::loadSquashGuard(uint32_t rob) const {
  if (!static_cast<bool>(squashNeed)) return true;
  return ROB::isOlder(rob, static_cast<uint32_t>(squashTag));
}

void DCache::wire_output() {
  busyOut = [this]() -> uint32_t { return isRequestFull() ? 1u : 0u; };
  loadRespValid = [this]() -> uint32_t {
    const auto h = static_cast<uint32_t>(head);
    const uint32_t rob = static_cast<uint32_t>(requestBuffer[h].robTag);
    const bool any = hitLoadReady() || fillLoadReady();
    if (!any) return 0u;
    if (static_cast<bool>(requestBuffer[h].cancelled)) return 0u;
    return loadSquashGuard(rob) ? 1u : 0u;
  };
  loadRespMemIndex = [this]() -> uint32_t {
    return static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].memIndex);
  };
  loadRespRobTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].robTag);
  };
  loadRespValue = [this]() -> uint32_t {
    if (hitLoadReady()) return hitRaw();
    if (fillLoadReady()) return fillRaw();
    return 0u;
  };
  reqReadValid = [this]() -> uint32_t {
    return (needsMemRead() && !static_cast<bool>(dmemReadBusy) &&
            !static_cast<bool>(dmemReplyReady))
               ? 1u
               : 0u;
  };
  reqReadAddr = [this]() -> uint32_t { return headAddr() & ~DCACHE_OFFSET_MASK; };
  reqWriteValid = [this]() -> uint32_t {
    return (needsMemWrite() && !static_cast<bool>(dmemWriteBusy)) ? 1u : 0u;
  };
  reqWriteAddr = [this]() -> uint32_t { return victimAddr(); };
  for (int i = 0; i < DCACHE_BLOCK_CAP; ++i) {
    reqWriteLineData[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(victimLine[i]);
    };
  }
  fillFire = [this]() -> uint32_t { return fillDone() ? 1u : 0u; };

  // Nested SRAM port arbitration: a refill beat writes two adjacent word banks,
  // a hit Store writes the banks its bytes touch, and lookup reads all banks
  // in parallel. Only the old head touches the SRAMs so young requests never
  // observe stale data.
  for (uint32_t w = 0; w < static_cast<uint32_t>(NUM_OF_DCACHE_WAYS); ++w) {
    for (uint32_t b = 0; b < static_cast<uint32_t>(DCACHE_WORDS_PER_LINE); ++b) {
      dataSrams[w][b].enable = [this, w, b]() -> uint32_t {
        return (dataBankWantWrite(w, b) || sramWantRead()) ? 1u : 0u;
      };
      dataSrams[w][b].writeEnable = [this, w, b]() -> uint32_t {
        return dataBankWantWrite(w, b) ? 1u : 0u;
      };
      dataSrams[w][b].addr = [this]() -> uint32_t { return headSet(); };
    }
    metaSrams[w].enable = [this, w]() -> uint32_t {
      return (metaWantWrite(w) || sramWantRead()) ? 1u : 0u;
    };
    metaSrams[w].writeEnable = [this, w]() -> uint32_t {
      return metaWantWrite(w) ? 1u : 0u;
    };
    metaSrams[w].addr = [this]() -> uint32_t { return headSet(); };
  }
  for (uint32_t w = 0; w < static_cast<uint32_t>(NUM_OF_DCACHE_WAYS); ++w) {
    for (uint32_t b = 0; b < static_cast<uint32_t>(DCACHE_WORDS_PER_LINE); ++b) {
      for (std::size_t lane = 0; lane < 4; ++lane) {
        const uint32_t bank = b;
        const uint32_t way = w;
        const std::size_t ln = lane;
        dataSrams[way][bank].writeMask[ln] = [this, way, bank, ln]() -> uint32_t {
          if (!dataBankWantWrite(way, bank)) return 0u;
          if (fillActive()) return 1u;
          const uint32_t off = headAddr() & DCACHE_OFFSET_MASK;
          const auto h = static_cast<uint32_t>(head);
          const int n = decodeNBytes(static_cast<uint32_t>(requestBuffer[h].nEnc));
          bool selected = false;
          for (int i = 0; i < 4; ++i) {
            if (i < n && headAddr() + static_cast<uint32_t>(i) < MEM_SIZE &&
                ((off + static_cast<uint32_t>(i)) >> 2) == bank &&
                ((off + static_cast<uint32_t>(i)) & 3) == ln)
              selected = true;
          }
          return selected ? 1u : 0u;
        };
        dataSrams[way][bank].writeData[ln] = [this, way, bank, ln]() -> uint32_t {
          const auto h = static_cast<uint32_t>(head);
          const bool isStore = static_cast<uint32_t>(requestBuffer[h].op) ==
                              static_cast<uint32_t>(Operation::Store);
          uint32_t data = 0u;
          if (fillActive() && dataBankWantWrite(way, bank))
            data = static_cast<uint32_t>(
                dmemLineData[(bank << RV32_WORD_BYTE_BITS) + ln]);
          const int n = decodeNBytes(static_cast<uint32_t>(requestBuffer[h].nEnc));
          const uint32_t off = headAddr() & DCACHE_OFFSET_MASK;
          const uint32_t v = static_cast<uint32_t>(requestBuffer[h].value);
          for (int i = 0; i < 4; ++i) {
            if (isStore && i < n && headAddr() + static_cast<uint32_t>(i) < MEM_SIZE &&
                ((off + static_cast<uint32_t>(i)) >> 2) == bank &&
                ((off + static_cast<uint32_t>(i)) & 3) == ln)
              data = (v >> (i << 3)) & 0xFFu;
          }
          return data;
        };
      }
    }
    metaSrams[w].writeMask[0] = [this, w]() -> uint32_t {
      return metaWantWrite(w) ? 1u : 0u;
    };
    metaSrams[w].writeData[0] = [this, w]() -> uint32_t {
      const uint32_t tag = (headAddr() >> DCACHE_TAG_SHIFT) & DCACHE_META_TAG_MASK;
      const auto h = static_cast<uint32_t>(head);
      const bool isStore = static_cast<uint32_t>(requestBuffer[h].op) ==
                          static_cast<uint32_t>(Operation::Store);
      return tag | (isStore ? DCACHE_META_DIRTY : 0u);
    };
  }
}

void DCache::work() {
  const uint32_t oldHead = static_cast<uint32_t>(head);
  const uint32_t oldCount = static_cast<uint32_t>(count);
  const bool oldReadValid = static_cast<bool>(readValid);
  const uint32_t oldReadIndex = static_cast<uint32_t>(readIndex);
  const int oldHitWay = hitWay();
  const bool oldHit = oldHitWay >= 0;
  const bool oldHeadReady = headReadReady();
  const bool oldReturnReady = isReturnReady();
  const bool oldFillActive = fillActive();
  const bool oldFillLast = fillDone();
  const uint32_t oldFillWord = fillWordIndex();
  const uint32_t oldHeadWay = headWay();
  const uint32_t oldHeadSet = headSet();
  const bool doPop = oldReturnReady || oldFillLast;
  const bool readConsumed = doPop && oldHeadReady;
  if (oldFillActive) validBits[oldHeadSet][oldHeadWay] <= (oldFillLast ? 1u : 0u);
  if (doPop && oldFillLast)
    plru[oldHeadSet] <= dcachePlruTouch(static_cast<uint32_t>(plru[oldHeadSet]),
                                        oldHeadWay);
  const bool oldDecValid = static_cast<bool>(decisionValid);
  assert(oldCount <= REQUEST_CAP);
  if (oldReadValid) assert(oldCount > 0 && oldReadIndex == oldHead);
  if (oldFillActive) {
    assert((oldFillWord & (DCACHE_FILL_WORDS_PER_BEAT - 1)) == 0);
    assert(oldFillWord + DCACHE_FILL_WORDS_PER_BEAT <=
           static_cast<uint32_t>(DCACHE_WORDS_PER_LINE));
    for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
      for (int b = 0; b < DCACHE_WORDS_PER_LINE; ++b) {
        const bool selected =
            static_cast<uint32_t>(w) == oldHeadWay &&
            static_cast<uint32_t>(b) >= oldFillWord &&
            static_cast<uint32_t>(b) <
                oldFillWord + DCACHE_FILL_WORDS_PER_BEAT;
        assert(static_cast<bool>(dataSrams[w][b].enable) == selected);
        assert(!selected || static_cast<bool>(dataSrams[w][b].writeEnable));
        for (int lane = 0; lane < RV32_WORD_BYTES; ++lane)
          assert(!selected || static_cast<bool>(dataSrams[w][b].writeMask[lane]));
      }
      const bool metaSelected =
          static_cast<uint32_t>(w) == oldHeadWay && oldFillLast;
      assert(static_cast<bool>(metaSrams[w].enable) == metaSelected);
      assert(!metaSelected || (static_cast<bool>(metaSrams[w].writeEnable) && doPop));
    }
  }

  if (doPop) {
    requestBuffer[oldHead].valid <= 0u;
    requestBuffer[oldHead].needReadMemory <= 0u;
    head <= ((oldHead + 1) & (REQUEST_CAP - 1));
    memReadSent <= 0u;
    memWriteSent <= 0u;
    loadResponded <= 0u;
    fillWord <= 0u;
  } else {
    if (oldFillActive && !headIsStore()) loadResponded <= 1u;
    if (static_cast<bool>(reqReadValid) && !static_cast<bool>(dmemReadBusy) &&
        !static_cast<bool>(dmemReplyReady))
      memReadSent <= 1u;
    if (static_cast<bool>(reqWriteValid) && !static_cast<bool>(dmemWriteBusy))
      memWriteSent <= 1u;
    if (oldFillActive) fillWord <= (oldFillWord + DCACHE_FILL_WORDS_PER_BEAT);
  }

  if (oldReadValid) {
    if (oldHit)
      ++statHits;
    else
      ++statMisses;
    const uint32_t readSet =
        (static_cast<uint32_t>(requestBuffer[oldReadIndex].addr) >>
         DCACHE_OFFSET_BITS) &
        DCACHE_INDEX_MASK;
    // A hit access refreshes replacement state even when its slot retires on
    // the same cycle; the set-level recency outlives the queue entry.
    if (oldHit)
      plru[readSet] <= dcachePlruTouch(static_cast<uint32_t>(plru[readSet]),
                                       static_cast<uint32_t>(oldHitWay));
    if (!readConsumed) {
      if (oldHit) {
        const uint32_t way = static_cast<uint32_t>(oldHitWay);
        const uint32_t meta = metaSrams[way].readLane(0);
        requestBuffer[oldReadIndex].tag <= (meta & DCACHE_META_TAG_MASK);
        requestBuffer[oldReadIndex].way <= way;
        requestBuffer[oldReadIndex].lineValid <= 1u;
        requestBuffer[oldReadIndex].lineDirty <= ((meta & DCACHE_META_DIRTY) ? 1u : 0u);
        requestBuffer[oldReadIndex].raw <= readRaw();
        requestBuffer[oldReadIndex].valid <= 1u;
      } else {
        const uint32_t victim = selectVictim(readSet);
        const uint32_t meta = metaSrams[victim].readLane(0);
        const bool victimValid =
            (static_cast<uint32_t>(readWayMask) & (1u << victim)) != 0;
        requestBuffer[oldReadIndex].tag <= (victimValid ? (meta & DCACHE_META_TAG_MASK) : 0u);
        requestBuffer[oldReadIndex].way <= victim;
        requestBuffer[oldReadIndex].lineValid <= (victimValid ? 1u : 0u);
        requestBuffer[oldReadIndex].lineDirty <= ((victimValid && (meta & DCACHE_META_DIRTY)) ? 1u : 0u);
        requestBuffer[oldReadIndex].needReadMemory <= 1u;
        for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
          victimLine[i] <= (dataSrams[victim][i >> 2].readLane(i & 3) & 0xFFu);
      }
    }
  }

  const bool lookupFire = !doPop && needsLookup() &&
                          static_cast<bool>(metaSrams[0].enable) &&
                          !static_cast<bool>(metaSrams[0].writeEnable);
  if (lookupFire) {
    const auto h = oldHead;
    const uint32_t addr = static_cast<uint32_t>(requestBuffer[h].addr);
    const uint32_t set = (addr >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK;
    readIndex <= h;
    uint32_t mask = 0u;
    for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
      assert(static_cast<bool>(metaSrams[w].enable) &&
             !static_cast<bool>(metaSrams[w].writeEnable));
      if (static_cast<bool>(validBits[set][w])) mask |= (1u << static_cast<uint32_t>(w));
    }
    readWayMask <= mask;
    readLaneIndex <= (addr & DCACHE_OFFSET_MASK);
    readNEnc <= static_cast<uint32_t>(requestBuffer[h].nEnc);
  }

  if (static_cast<bool>(squashNeed)) {
    const uint32_t stag = static_cast<uint32_t>(squashTag);
    for (int age = 0; age < REQUEST_CAP; ++age) {
      const uint32_t slot = (oldHead + age) & (REQUEST_CAP - 1);
      const uint32_t rob = static_cast<uint32_t>(requestBuffer[slot].robTag);
      if (static_cast<uint32_t>(age) < oldCount &&
          !static_cast<bool>(requestBuffer[slot].cancelled) &&
          static_cast<uint32_t>(requestBuffer[slot].op) ==
              static_cast<uint32_t>(Operation::Load) && !ROB::isOlder(rob, stag))
        requestBuffer[slot].cancelled <= 1u;
    }
  }

  if (oldDecValid) {
    // Admission uses old full even when the head completes this cycle.
    assert(oldCount < REQUEST_CAP);
    assert(static_cast<uint32_t>(decisionNBytes) <= 2);
    assert((static_cast<uint32_t>(decisionAddr) & DCACHE_OFFSET_MASK) +
               decodeNBytes(static_cast<uint32_t>(decisionNBytes)) <= DCACHE_BLOCK_CAP);
    assert(static_cast<uint32_t>(decisionOp) == static_cast<uint32_t>(Operation::Load) ||
           static_cast<uint32_t>(decisionOp) == static_cast<uint32_t>(Operation::Store));
    const uint32_t tail = (oldHead + oldCount) & (REQUEST_CAP - 1);
    requestBuffer[tail].op <= static_cast<uint32_t>(decisionOp);
    requestBuffer[tail].value <= static_cast<uint32_t>(decisionValue);
    requestBuffer[tail].addr <= static_cast<uint32_t>(decisionAddr);
    requestBuffer[tail].isSigned <= static_cast<uint32_t>(decisionIsSigned);
    requestBuffer[tail].nEnc <= static_cast<uint32_t>(decisionNBytes);
    requestBuffer[tail].robTag <= static_cast<uint32_t>(decisionRobTag);
    requestBuffer[tail].memIndex <= static_cast<uint32_t>(decisionMemIndex);
    requestBuffer[tail].raw <= 0u;
    requestBuffer[tail].tag <= 0u;
    requestBuffer[tail].way <= 0u;
    requestBuffer[tail].lineValid <= 0u;
    requestBuffer[tail].lineDirty <= 0u;
    requestBuffer[tail].needReadMemory <= 0u;
    requestBuffer[tail].cancelled <= 0u;
    requestBuffer[tail].valid <= 0u;
  }

  readValid <= (lookupFire ? 1u : 0u);
  count <= (oldCount + (oldDecValid ? 1u : 0u) - (doPop ? 1u : 0u));

  for (uint32_t w = 0; w < static_cast<uint32_t>(NUM_OF_DCACHE_WAYS); ++w) {
    for (uint32_t b = 0; b < static_cast<uint32_t>(DCACHE_WORDS_PER_LINE); ++b)
      dataSrams[w][b].work();
    metaSrams[w].work();
  }
}
