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
bool DCache::needsLookup() const {
  if (static_cast<uint32_t>(count) == 0) return false;
  if (static_cast<bool>(readValid)) return false;
  const auto h = static_cast<uint32_t>(head);
  if (static_cast<bool>(requestBuffer[h].valid)) return false;
  if (static_cast<bool>(requestBuffer[h].needReadMemory)) return false;
  return true;
}
bool DCache::readHit() const {
  if (!static_cast<bool>(readValid)) return false;
  const uint32_t meta = metaSram.readLane(0);
  if ((meta & DCACHE_META_VALID) == 0) return false;
  const auto ri = static_cast<uint32_t>(readIndex);
  const uint32_t reqAddr = static_cast<uint32_t>(requestBuffer[ri].addr);
  return (reqAddr >> DCACHE_TAG_SHIFT) == (meta & DCACHE_META_TAG_MASK);
}
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
uint32_t DCache::victimAddr() const {
  const uint32_t set = headSet();
  const auto h = static_cast<uint32_t>(head);
  const uint32_t vtag = static_cast<uint32_t>(requestBuffer[h].tag);
  return ((vtag << DCACHE_INDEX_BITS) | set) << DCACHE_OFFSET_BITS;
}
bool DCache::sramWantWrite() const {
  if (canFill()) return true;
  return isReturnReady() && headIsStore();
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
  if (!canFill()) return false;
  const auto h = static_cast<uint32_t>(head);
  return static_cast<uint32_t>(requestBuffer[h].op) == static_cast<uint32_t>(Operation::Load);
}
uint32_t DCache::readRaw() const {
  const int n = decodeNBytes(static_cast<uint32_t>(readNEnc));
  const uint32_t lane0 = static_cast<uint32_t>(readLaneIndex);
  uint32_t raw = 0;
  for (int i = 0; i < 4; ++i) {
    if (i < n) raw |= (dataSram.readLane(lane0 + i) & 0xFFu) << (i << 3);
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
  fillFire = [this]() -> uint32_t { return canFill() ? 1u : 0u; };

  // Nested SRAM port arbitration: fill/store-write > lookup. Only the old
  // head touches the SRAMs so young requests never observe stale data.
  dataSram.enable = [this]() -> uint32_t { return (sramWantWrite() || sramWantRead()) ? 1u : 0u; };
  dataSram.writeEnable = [this]() -> uint32_t { return sramWantWrite() ? 1u : 0u; };
  dataSram.addr = [this]() -> uint32_t { return headSet(); };
  metaSram.enable = [this]() -> uint32_t { return (sramWantWrite() || sramWantRead()) ? 1u : 0u; };
  metaSram.writeEnable = [this]() -> uint32_t { return sramWantWrite() ? 1u : 0u; };
  metaSram.addr = [this]() -> uint32_t { return headSet(); };
  for (std::size_t lane = 0; lane < dataSram.Lanes; ++lane) {
    dataSram.writeMask[lane] = [this, lane]() -> uint32_t {
      if (!sramWantWrite()) return 0u;
      if (canFill()) return 1u;
      const uint32_t off = headAddr() & DCACHE_OFFSET_MASK;
      const auto h = static_cast<uint32_t>(head);
      const int n = decodeNBytes(static_cast<uint32_t>(requestBuffer[h].nEnc));
      bool selected = false;
      for (int i = 0; i < 4; ++i) {
        if (i < n && headAddr() + i < MEM_SIZE &&
            lane == off + static_cast<uint32_t>(i))
          selected = true;
      }
      return selected ? 1u : 0u;
    };
    dataSram.writeData[lane] = [this, lane]() -> uint32_t {
      const auto h = static_cast<uint32_t>(head);
      const bool isStore = static_cast<uint32_t>(requestBuffer[h].op) ==
                          static_cast<uint32_t>(Operation::Store);
      uint32_t data = canFill() ? static_cast<uint32_t>(dmemLineData[lane]) : 0u;
      const int n = decodeNBytes(static_cast<uint32_t>(requestBuffer[h].nEnc));
      const uint32_t off = headAddr() & DCACHE_OFFSET_MASK;
      const uint32_t v = static_cast<uint32_t>(requestBuffer[h].value);
      for (int i = 0; i < 4; ++i) {
        if (isStore && i < n && headAddr() + i < MEM_SIZE &&
            lane == off + static_cast<uint32_t>(i))
          data = (v >> (i << 3)) & 0xFFu;
      }
      return data;
    };
  }
  metaSram.writeMask[0] = [this]() -> uint32_t { return sramWantWrite() ? 1u : 0u; };
  metaSram.writeData[0] = [this]() -> uint32_t {
    const uint32_t tag = (headAddr() >> DCACHE_TAG_SHIFT) & DCACHE_META_TAG_MASK;
    const auto h = static_cast<uint32_t>(head);
    const bool isStore = static_cast<uint32_t>(requestBuffer[h].op) ==
                         static_cast<uint32_t>(Operation::Store);
    return tag | DCACHE_META_VALID | (isStore ? DCACHE_META_DIRTY : 0u);
  };
}

void DCache::work() {
  const uint32_t oldHead = static_cast<uint32_t>(head);
  const uint32_t oldCount = static_cast<uint32_t>(count);
  const bool oldReadValid = static_cast<bool>(readValid);
  const uint32_t oldReadIndex = static_cast<uint32_t>(readIndex);
  const bool oldHit = readHit();
  const bool oldHeadReady = headReadReady();
  const bool oldReturnReady = isReturnReady();
  const bool oldCanFill = canFill();
  const bool doPop = oldReturnReady || oldCanFill;
  const bool readConsumed = doPop && oldHeadReady;
  const bool oldDecValid = static_cast<bool>(decisionValid);
  assert(oldCount <= REQUEST_CAP);
  if (oldReadValid) assert(oldCount > 0 && oldReadIndex == oldHead);

  if (doPop) {
    requestBuffer[oldHead].valid <= 0u;
    requestBuffer[oldHead].needReadMemory <= 0u;
    head <= ((oldHead + 1) & (REQUEST_CAP - 1));
    memReadSent <= 0u;
    memWriteSent <= 0u;
  } else {
    if (static_cast<bool>(reqReadValid) && !static_cast<bool>(dmemReadBusy) &&
        !static_cast<bool>(dmemReplyReady))
      memReadSent <= 1u;
    if (static_cast<bool>(reqWriteValid) && !static_cast<bool>(dmemWriteBusy))
      memWriteSent <= 1u;
  }

  if (oldReadValid) {
    if (oldHit)
      ++statHits;
    else
      ++statMisses;
    if (!readConsumed) {
      const uint32_t meta = metaSram.readLane(0);
      requestBuffer[oldReadIndex].tag <= (meta & DCACHE_META_TAG_MASK);
      requestBuffer[oldReadIndex].lineValid <= ((meta & DCACHE_META_VALID) ? 1u : 0u);
      requestBuffer[oldReadIndex].lineDirty <= ((meta & DCACHE_META_DIRTY) ? 1u : 0u);
      if (oldHit) {
        requestBuffer[oldReadIndex].raw <= readRaw();
        requestBuffer[oldReadIndex].valid <= 1u;
      } else {
        requestBuffer[oldReadIndex].needReadMemory <= 1u;
        for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
          victimLine[i] <= (dataSram.readLane(i) & 0xFFu);
      }
    }
  }

  const bool lookupFire = !doPop && needsLookup() &&
                          static_cast<bool>(dataSram.enable) &&
                          !static_cast<bool>(dataSram.writeEnable);
  if (lookupFire) {
    const auto h = oldHead;
    const uint32_t addr = static_cast<uint32_t>(requestBuffer[h].addr);
    readIndex <= h;
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
    requestBuffer[tail].lineValid <= 0u;
    requestBuffer[tail].lineDirty <= 0u;
    requestBuffer[tail].needReadMemory <= 0u;
    requestBuffer[tail].cancelled <= 0u;
    requestBuffer[tail].valid <= 0u;
  }

  readValid <= (lookupFire ? 1u : 0u);
  count <= (oldCount + (oldDecValid ? 1u : 0u) - (doPop ? 1u : 0u));

  dataSram.work();
  metaSram.work();
}
