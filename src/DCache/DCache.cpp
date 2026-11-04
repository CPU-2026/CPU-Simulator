#include "DCache.hpp"
#include "ROB.hpp"
#include <bit>
#include <cassert>

namespace {
template<class T> uint32_t u(const T &value) { return static_cast<uint32_t>(value); }
}

DCache::DCache() {
  busyOut = [this]() { return isRequestFull(); };
  loadRespValid = [this]() { return events().loadValid; };
  loadRespValue = [this]() { return events().loadValue; };
  loadRespRobTag = [this]() { return events().response >= 0 ? request(events().response).robTag : 0; };
  loadRespMemIndex = [this]() { return events().response >= 0 ? request(events().response).memIndex : 0; };
  reqReadValid = [this]() { return events().memoryRequest.readValid; };
  reqReadAddr = [this]() { return events().memoryRequest.read.address; };
  reqReadTransaction = [this]() { return events().memoryRequest.read.transaction; };
  reqReadStartWord = [this]() { return (request(u(misses[events().memoryRequest.read.transaction].owner)).address & DCACHE_OFFSET_MASK) >> 2; };
  reqWriteValid = [this]() { return events().memoryRequest.writeValid; };
  reqWriteAddr = [this]() { return events().memoryRequest.write.address; };
  reqWriteTransaction = [this]() { return events().memoryRequest.write.transaction; };
  for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
    reqWriteLineData[i] = [this, i]() { return events().memoryRequest.write.lineData[i]; };
  fillFire = [this]() { return events().fillFire; };
  writeConsumed = [this]() { return events().writeAck >= 0; };
  exitValid = [this]() { return events().exitValid; };
  exitValue = [this]() { return events().exitValue; };
  for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
    metadata[w].enable = [this,w]() { return events().meta[w].enable; };
    metadata[w].writeEnable = [this,w]() { return events().meta[w].writeEnable; };
    metadata[w].addr = [this,w]() { return events().meta[w].addr; };
    metadata[w].writeMask[0] = [this,w]() { return events().meta[w].writeMask[0]; };
    metadata[w].writeData[0] = [this,w]() { return events().meta[w].writeData[0]; };
    for (int b = 0; b < DCACHE_WORDS_PER_LINE; ++b) {
      datas[w][b].enable = [this,w,b]() { return events().data[w][b].enable; };
      datas[w][b].writeEnable = [this,w,b]() { return events().data[w][b].writeEnable; };
      datas[w][b].addr = [this,w,b]() { return events().data[w][b].addr; };
      for (int l = 0; l < 4; ++l) {
        datas[w][b].writeMask[l] = [this,w,b,l]() { return events().data[w][b].writeMask[l]; };
        datas[w][b].writeData[l] = [this,w,b,l]() { return events().data[w][b].writeData[l]; };
      }
    }
  }
}
const DCachePlan &DCache::events() const {
  if (!comb.valid) { comb.events = plan(); comb.valid = true; }
  return comb.events;
}
CacheRequest DCache::request(int s) const {
  const auto &r = requests[s];
  return {static_cast<Operation>(u(r.op)), int32_t(u(r.value)), u(r.address), bool(u(r.isSigned)),
          int(1u << u(r.nEnc)), uint8_t(u(r.robTag)), uint8_t(u(r.memIndex))};
}
int DCache::miss(int s) const { return int(u(requests[s].miss)) - 1; }
uint8_t DCache::activeMask() const {
  uint8_t mask = 0;
  for (int s = 0; s < REQUEST_CAP; ++s) if (u(requests[s].active)) mask |= 1u << s;
  return mask;
}
int DCache::oldestRequest(uint8_t mask) const {
  int selected = -1;
  for (int i = 0; i < REQUEST_CAP; ++i) {
    bool prior = false;
    for (int j = 0; j < REQUEST_CAP; ++j)
      if ((mask & (1u << j)) && (u(olderMask[j]) & (1u << i))) prior = true;
    if ((mask & (1u << i)) && !prior) selected = i;
  }
  return selected;
}
int DCache::oldestMiss(uint8_t mask) const {
  uint8_t owners = 0;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m) if (mask & (1u << m)) owners |= 1u << u(misses[m].owner);
  const int owner = oldestRequest(owners);
  int selected = -1;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m)
    if ((mask & (1u << m)) && int(u(misses[m].owner)) == owner) selected = m;
  return selected;
}
int DCache::freeRequest() const {
  int selected = -1;
  for (int n = 0; n < REQUEST_CAP; ++n) {
    const int s = (u(cursor) + n) & (REQUEST_CAP - 1);
    if (selected < 0 && !u(requests[s].active)) selected = s;
  }
  return selected;
}
int DCache::freeMiss() const {
  int selected = -1;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m) if (selected < 0 && !u(misses[m].valid)) selected = m;
  return selected;
}
int DCache::findMiss(uint32_t line) const {
  int selected = -1;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m) if (u(misses[m].valid) && u(misses[m].line) == line) selected = m;
  return selected;
}
bool DCache::setLocked(uint32_t addr) const {
  bool locked = false;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m)
    if (u(misses[m].valid) && ((u(misses[m].line) ^ addr) & (DCACHE_INDEX_MASK << DCACHE_OFFSET_BITS)) == 0) locked = true;
  return locked;
}
bool DCache::storeHazard(int s, bool allocating) const {
  const auto load = request(s);
  bool blocked = false;
  for (int j = 0; j < REQUEST_CAP; ++j) {
    const auto store = request(j);
    if (u(requests[j].active) && store.op == Operation::Store && (u(olderMask[j]) & (1u << s))) {
      const uint32_t lo = load.address & DCACHE_OFFSET_MASK, so = store.address & DCACHE_OFFSET_MASK;
      const bool overlap = (load.address >> DCACHE_OFFSET_BITS) == (store.address >> DCACHE_OFFSET_BITS) &&
          lo <= so + uint32_t(store.nBytes) - 1u && so <= lo + uint32_t(load.nBytes) - 1u;
      if (overlap || (allocating && ((load.address ^ store.address) & (DCACHE_INDEX_MASK << DCACHE_OFFSET_BITS)) == 0)) blocked = true;
    }
  }
  return blocked;
}
uint32_t DCache::victimAddress(int m) const {
  return ((u(misses[m].victimTag) << DCACHE_INDEX_BITS) |
      ((u(misses[m].line) >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK)) << DCACHE_OFFSET_BITS;
}
bool DCache::victimHazard(int m) const {
  bool blocked = false;
  for (int n = 0; n < DCACHE_MSHR_CAP; ++n)
    if (n != m && u(misses[n].valid) && u(misses[n].dirty) && !u(misses[n].writeDone) && victimAddress(n) == u(misses[m].line)) blocked = true;
  return blocked;
}
int DCache::hitWay() const {
  int selected = -1;
  if (u(readValid)) for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w)
    if (selected < 0 && (u(readWayMask) & (1u << w)) &&
        (metadata[w].readLane(0) & DCACHE_META_TAG_MASK) == (request(u(readIndex)).address >> DCACHE_TAG_SHIFT)) selected = w;
  return selected;
}
uint32_t DCache::selectVictim(uint32_t set) const {
  uint32_t selected = dcachePlruVictim(u(plru[set]));
  bool found = false;
  for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) if (!found && !u(validBits[set][w])) { selected = w; found = true; }
  return selected;
}
uint32_t DCache::sramRaw(int w) const {
  const unsigned bank = u(readOffset) >> 2;
  uint32_t low = 0, high = 0;
  for (int lane = 0; lane < 4; ++lane) {
    low |= datas[w][bank].readLane(lane) << (lane << 3);
    if (bank + 1 < DCACHE_WORDS_PER_LINE)
      high |= datas[w][bank + 1].readLane(lane) << (lane << 3);
  }
  return extractLoadWord(low, high, u(readOffset), 1u << u(readNEnc));
}
uint32_t DCache::memoryRaw(int s) const {
  const auto r = request(s);
  uint32_t raw = 0;
  for (int i = 0; i < 4; ++i) if (i < r.nBytes) raw |= u(dmemLineData[(r.address & DCACHE_OFFSET_MASK) + i]) << (i << 3);
  return raw;
}
uint32_t DCache::wordRaw(int s, uint32_t data) const {
  const auto r = request(s);
  const uint32_t shifted = data >> ((r.address & 3u) << 3);
  if (r.nBytes == 1) return shifted & 0xffu;
  if (r.nBytes == 2) return shifted & 0xffffu;
  return shifted;
}
DCachePlan DCache::plan() const {
  DCachePlan p{};
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m) p.prefetchMissCount += unsigned(u(misses[m].valid) != 0);
  bool seen = false;
  for (int h = 0; h < 4; ++h)
    if (u(prefetchSeenValid[h]) && u(prefetchSeenLine[h]) == (u(decisionAddr) & ~DCACHE_OFFSET_MASK)) seen = true;
  p.prefetchNextLine = (u(decisionAddr) & ~DCACHE_OFFSET_MASK) + DCACHE_BLOCK_CAP;
  p.prefetchTrigger = !u(prefetchStopped) && u(decisionValid) && static_cast<Operation>(u(decisionOp)) == Operation::Load &&
      (u(decisionAddr) >> 28) == 0 && (p.prefetchNextLine >> 28) == 0 && !seen;
  for (int s = 0; s < REQUEST_CAP; ++s) {
    const auto r = request(s);
    if (u(requests[s].active) && u(requests[s].prefetch)) p.prefetchMask |= static_cast<uint8_t>(1u << s);
    if (u(requests[s].active) && (r.address & ~DCACHE_OFFSET_MASK) == u(prefetchTarget)) p.prefetchDuplicate = true;
    if (u(requests[s].active) && r.op == Operation::Store &&
        ((r.address ^ u(prefetchTarget)) & (DCACHE_INDEX_MASK << DCACHE_OFFSET_BITS)) == 0) p.prefetchStoreConflict = true;
    if (u(requests[s].active) && u(requests[s].prefetch) && miss(s) < 0) {
      bool abort = u(prefetchStopped) || p.prefetchMissCount >= 3 || findMiss(r.address & ~DCACHE_OFFSET_MASK) >= 0;
      for (int st = 0; st < REQUEST_CAP; ++st)
        if (u(requests[st].active) && request(st).op == Operation::Store &&
            ((request(st).address ^ r.address) & (DCACHE_INDEX_MASK << DCACHE_OFFSET_BITS)) == 0) abort = true;
      if (u(decisionValid) && static_cast<Operation>(u(decisionOp)) == Operation::Store &&
          ((u(decisionAddr) ^ r.address) & (DCACHE_INDEX_MASK << DCACHE_OFFSET_BITS)) == 0) abort = true;
      if (abort) p.prefetchAbort |= static_cast<uint8_t>(1u << s);
    }
  }
  p.prefetchEnqueue = !u(prefetchStopped) && u(prefetchPending) && !u(decisionValid) &&
      p.prefetchMask == 0 && u(count) <= REQUEST_CAP - 2 && p.prefetchMissCount <= 2 &&
      !p.prefetchDuplicate && !p.prefetchStoreConflict && findMiss(u(prefetchTarget)) < 0;
  const int first = oldestRequest(activeMask() & ~p.prefetchMask), way = hitWay();
  p.readHitRaw = u(readValid) && way >= 0 ? sramRaw(way) : 0;
  if (u(readValid) && !u(requests[u(readIndex)].prefetch) && way >= 0 && request(u(readIndex)).op == Operation::Load) {
    uint8_t selected = u(hitLocationVictim);
    // Lowest invalid row first, then cyclic replacement after training.
    for (int h = 3; h >= 0; --h)
      if (!u(hitLocationValid[h])) selected = h;
    for (int h = 0; h < 4; ++h)
      if (u(hitLocationValid[h]) && u(hitLocationLine[h]) ==
          (request(u(readIndex)).address >> DCACHE_OFFSET_BITS)) selected = h;
    p.hitLocationTrain = selected;
  }
  bool storeHintMatch = false;
  for (int h = 0; h < 4; ++h)
    if (first >= 0 && u(hitLocationValid[h]) && u(hitLocationLine[h]) ==
        (request(first).address >> DCACHE_OFFSET_BITS)) {
      storeHintMatch = true;
      p.storeHintWay = u(hitLocationWay[h]);
    }
  if (first >= 0 && request(first).address == AXI_EXIT_ADDR && request(first).op == Operation::Store && u(exitReady)) {
    assert(request(first).nBytes == 4);
    p.exitValid = true; p.exitValue = uint32_t(request(first).value);
  }
  if (u(dmemWriteReplyReady)) {
    const int m = u(writeTransaction);
    assert(m < DCACHE_MSHR_CAP && u(misses[m].valid) && u(misses[m].dirty) && u(misses[m].writeSent) && !u(misses[m].writeDone) && victimAddress(m) == u(writeAddress));
    p.writeAck = m;
  }
  if (u(dmemReplyReady)) {
    const int m = u(replyTransaction);
    assert(m < DCACHE_MSHR_CAP && u(misses[m].valid) && u(misses[m].readSent) && u(misses[m].line) == u(replyAddress));
    if (!u(misses[m].dirty) || u(misses[m].writeDone) || p.writeAck == m) {
      p.fillMiss = m; p.fillBeat = true;
      p.fillFire = u(misses[m].fillWord) == DCACHE_WORDS_PER_LINE - DCACHE_FILL_WORDS_PER_BEAT;
    }
  }
  const int fillOwner = p.fillMiss >= 0 ? int(u(misses[p.fillMiss].owner)) : -1;
  const bool early = p.fillBeat && request(fillOwner).op == Operation::Load && !u(misses[p.fillMiss].responded) && !u(requests[fillOwner].prefetch);
  if (early) p.response = fillOwner;
  else if (u(wordValid)) {
    p.wordMiss = u(wordTransaction);
    const auto &m = misses[p.wordMiss];
    const int owner = u(m.owner);
    const auto r = request(owner);
    p.wordReturn = u(m.valid) && u(m.readSent) && !u(m.responded) &&
        u(requests[owner].active) && !u(requests[owner].prefetch) && r.op == Operation::Load && !storeHazard(owner, false) &&
        (u(wordAddress) >> DCACHE_OFFSET_BITS) == (u(m.line) >> DCACHE_OFFSET_BITS) &&
        ((u(wordAddress) & DCACHE_OFFSET_MASK) >> 2) == ((r.address & DCACHE_OFFSET_MASK) >> 2) &&
        (r.address & 3u) + r.nBytes <= 4;
    if (p.wordReturn) p.response = owner;
  }
  if (u(wordValid)) {
    const auto &m = misses[u(wordTransaction)];
    const int owner = u(m.owner);
    const bool captureValid = u(m.valid) && u(m.readSent) &&
        request(owner).op == Operation::Load &&
        (u(wordAddress) >> DCACHE_OFFSET_BITS) == (u(m.line) >> DCACHE_OFFSET_BITS);
    for (int s = 0; s < REQUEST_CAP; ++s) {
      const auto r = request(s);
      if (captureValid && s != owner && u(requests[s].active) && !u(requests[s].prefetch) && !u(requests[s].ready) &&
          r.op == Operation::Load && !storeHazard(s, true) &&
          (r.address >> 2) == (u(wordAddress) >> 2) && (r.address & 3u) + r.nBytes <= 4)
        p.wordCapture |= static_cast<uint8_t>(1u << s);
    }
  }
  if (p.response < 0) {
    uint8_t ready = 0;
    for (int s = 0; s < REQUEST_CAP; ++s)
      if (u(requests[s].active) && !u(requests[s].prefetch) && request(s).op == Operation::Load && !storeHazard(s, false) &&
          (u(requests[s].ready) || (u(readValid) && u(readIndex) == uint32_t(s) && way >= 0))) ready |= 1u << s;
    p.response = oldestRequest(ready);
  }
  if (p.response >= 0) {
    const int s = p.response;
    p.loadValue = early ? memoryRaw(s) : p.wordReturn ? wordRaw(s, u(wordData)) :
        u(readValid) && u(readIndex) == uint32_t(s) && way >= 0 ? p.readHitRaw : u(rawPayload[s]);
    p.loadValid = !u(requests[s].cancelled) && (!u(squashNeed) || ROB::isOlder(request(s).robTag, u(squashTag)));
  }
  p.storeHintFire = !p.fillBeat && first >= 0 && request(first).op == Operation::Store &&
      request(first).address != AXI_EXIT_ADDR && !u(requests[first].ready) && miss(first) < 0 &&
      !u(requests[first].deferred) && storeHintMatch && !setLocked(request(first).address) &&
      (!u(readValid) || (way >= 0 && request(u(readIndex)).op == Operation::Load &&
                        p.response == int(u(readIndex))));
  if (!p.fillBeat && first >= 0 && request(first).op == Operation::Store && request(first).address != AXI_EXIT_ADDR &&
      !setLocked(request(first).address) && (p.storeHintFire || u(requests[first].ready) || (u(readValid) && int(u(readIndex)) == first && way >= 0))) p.store = first;
  if (u(readValid) && way < 0 && !(p.prefetchAbort & (1u << u(readIndex))) && !storeHazard(u(readIndex), true)) {
    const uint32_t line = request(u(readIndex)).address & ~DCACHE_OFFSET_MASK;
    if (findMiss(line) < 0 && !setLocked(line)) p.allocate = freeMiss();
  }
  if (!u(readValid)) {
    uint8_t mergeable = 0;
    for (int s = 0; s < REQUEST_CAP; ++s) {
      const int m = findMiss(request(s).address & ~DCACHE_OFFSET_MASK);
      if (u(requests[s].active) && !u(requests[s].prefetch) && !u(requests[s].ready) && miss(s) < 0 && request(s).op == Operation::Load &&
          !storeHazard(s, true) && m >= 0 && request(u(misses[m].owner)).op == Operation::Load &&
          u(misses[m].fillWord) == 0 && !u(misses[m].responded) && p.fillMiss != m) mergeable |= 1u << s;
    }
    p.mergeSlot = oldestRequest(mergeable);
    if (p.mergeSlot >= 0) p.mergeMiss = findMiss(request(p.mergeSlot).address & ~DCACHE_OFFSET_MASK);
  }
  {
    uint8_t eligible = 0;
    const bool continueHit = u(readValid) && way >= 0 && !p.fillBeat &&
        request(u(readIndex)).op == Operation::Load && p.response == int(u(readIndex));
    uint8_t lookupActive = activeMask();
    if (!p.fillBeat && p.response >= 0)
      lookupActive &= static_cast<uint8_t>(~(1u << p.response));
    const int lookupFirst = oldestRequest(lookupActive & ~p.prefetchMask);
    bool anyMiss = false;
    for (int m = 0; m < DCACHE_MSHR_CAP; ++m) anyMiss = anyMiss || u(misses[m].valid);
    for (int s = 0; s < REQUEST_CAP; ++s) {
      const auto r = request(s);
      const bool blocked = !u(requests[s].active) || (p.prefetchAbort & (1u << s)) || (u(readValid) && u(readIndex) == uint32_t(s)) ||
          u(requests[s].ready) || miss(s) >= 0 || s == p.mergeSlot ||
           r.address == AXI_EXIT_ADDR || (r.op == Operation::Store && setLocked(r.address)) ||
           storeHazard(s, false) || findMiss(r.address & ~DCACHE_OFFSET_MASK) >= 0 ||
           (r.op == Operation::Store && s != first) ||
           (u(requests[s].deferred) && (s != first || freeMiss() < 0 || setLocked(r.address))) ||
          (!anyMiss && s != first && s != lookupFirst && !u(requests[s].prefetch));
      if (!blocked) eligible |= 1u << s;
    }
    const uint8_t demand = eligible & ~p.prefetchMask;
    if (demand) eligible = demand;
    else if (u(proposalValid)) eligible = 0;
    p.lookupCandidate = oldestRequest(eligible);
    if ((!u(readValid) || continueHit) && !p.fillBeat && p.store < 0) p.lookup = p.lookupCandidate;
  }
  // OLD lookup/fill/Store work retains SRAM priority. The accepted Load still
  // occupies an ordinary slot and cannot see its SRAM result until next cycle.
  bool queuedStoreConflict = false;
  for (int s = 0; s < REQUEST_CAP; ++s) {
    const auto r = request(s);
    if (u(requests[s].active) && r.op == Operation::Store &&
        ((r.address >> DCACHE_OFFSET_BITS) == (u(decisionAddr) >> DCACHE_OFFSET_BITS) ||
         r.address == AXI_EXIT_ADDR)) queuedStoreConflict = true;
  }
  const bool readEdgeAvailable = !u(readValid) ||
      (way >= 0 && request(u(readIndex)).op == Operation::Load &&
       p.response == int(u(readIndex)));
  p.acceptLookup = u(decisionValid) && u(count) < REQUEST_CAP &&
      !queuedStoreConflict && readEdgeAvailable && p.lookup < 0 &&
      !p.fillBeat && p.store < 0 &&
      static_cast<Operation>(u(decisionOp)) == Operation::Load &&
      u(decisionAddr) != AXI_EXIT_ADDR && !setLocked(u(decisionAddr));

  uint8_t readable = 0, writable = 0;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m) if (u(misses[m].valid)) {
    if (!u(misses[m].readSent) && !victimHazard(m) && !storeHazard(u(misses[m].owner), true)) readable |= 1u << m;
    if (u(misses[m].dirty) && !u(misses[m].writeSent)) writable |= 1u << m;
  }
  uint8_t demandReadable = 0;
  for (int m = 0; m < DCACHE_MSHR_CAP; ++m)
    if ((readable & (1u << m)) && !u(requests[u(misses[m].owner)].prefetch)) demandReadable |= static_cast<uint8_t>(1u << m);
  if (demandReadable) readable = demandReadable;
  const int rd = oldestMiss(readable), wr = oldestMiss(writable);
  if (rd >= 0 && u(dmemCredits) > unsigned(wr >= 0)) { p.memoryRequest.readValid = true; p.memoryRequest.read = {u(misses[rd].line), uint8_t(rd)}; }
  if (wr >= 0 && u(dmemWriteReady)) {
    p.memoryRequest.writeValid = true;
    p.memoryRequest.write.address = victimAddress(wr); p.memoryRequest.write.transaction = wr;
    for (int b = 0; b < DCACHE_BLOCK_CAP; ++b) p.memoryRequest.write.lineData[b] = u(victimPayload[wr][b >> 2]) >> ((b & 3) << 3);
  }
  // Only a granted accepted lookup selects the producer's SRAM address.
  const uint32_t lookupAddress = p.acceptLookup ? u(decisionAddr) :
      request(unsigned(p.lookupCandidate) & (REQUEST_CAP - 1)).address;
  const uint32_t lookupSet = (lookupAddress >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK;
  for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
    p.meta[w].addr = lookupSet;
    for (int b = 0; b < DCACHE_WORDS_PER_LINE; ++b) p.data[w][b].addr = lookupSet;
  }
  if (p.fillBeat) {
    const auto &m = misses[p.fillMiss];
    const auto r = request(u(m.owner));
    const uint32_t set = (u(m.line) >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK;
    for (int n = 0; n < DCACHE_FILL_WORDS_PER_BEAT; ++n) {
      const int bank = u(m.fillWord) + n;
      auto &port = p.data[u(m.way)][bank];
      port.enable = port.writeEnable = true; port.addr = set;
      for (int lane = 0; lane < 4; ++lane) {
        const uint32_t b = (bank << 2) + lane;
        uint32_t v = u(dmemLineData[b]);
        for (int j = 0; j < 4; ++j) if (r.op == Operation::Store && j < r.nBytes && r.address + j < AXI_RAM_BYTES && b == (r.address & DCACHE_OFFSET_MASK) + uint32_t(j)) v = uint32_t(r.value) >> (j << 3);
        port.writeData[lane] = v; port.writeMask[lane] = true;
      }
    }
    if (p.fillFire) {
      auto &port = p.meta[u(m.way)];
      port.enable = port.writeEnable = port.writeMask[0] = true; port.addr = set;
      port.writeData[0] = (u(m.line) >> DCACHE_TAG_SHIFT) | (r.op == Operation::Store ? DCACHE_META_DIRTY : 0u);
    }
  } else if (p.store >= 0) {
    const auto r = request(p.store);
    const int w = p.storeHintFire ? p.storeHintWay :
        u(readValid) && int(u(readIndex)) == p.store && way >= 0 ? way : int(u(requests[p.store].way));
    const uint32_t set = (r.address >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK;
    for (int b = 0; b < 4; ++b) if (b < r.nBytes && r.address + b < AXI_RAM_BYTES) {
      const uint32_t lane = (r.address & DCACHE_OFFSET_MASK) + b;
      auto &port = p.data[w][lane >> 2];
      port.enable = port.writeEnable = true; port.addr = set;
      port.writeMask[lane & 3] = true; port.writeData[lane & 3] = uint32_t(r.value) >> (b << 3);
    }
    auto &port = p.meta[w];
    port.enable = port.writeEnable = port.writeMask[0] = true; port.addr = set;
    port.writeData[0] = (r.address >> DCACHE_TAG_SHIFT) | DCACHE_META_DIRTY;
  } else if (p.lookup >= 0 || p.acceptLookup) {
    const uint32_t address = p.acceptLookup ? u(decisionAddr) : request(p.lookup).address;
    const uint32_t set = (address >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK;
    for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
      p.meta[w].enable = true; p.meta[w].addr = set;
      for (auto &port : p.data[w]) { port.enable = true; port.addr = set; }
    }
  }
  return p;
}
void DCache::work() {
  const auto &p = events();
  const uint8_t active = activeMask();
  assert(std::popcount(active) == int(u(count)) && u(count) <= REQUEST_CAP);
  const int fillOwner = p.fillMiss >= 0 ? int(u(misses[p.fillMiss].owner)) : -1;
  assert(std::popcount(p.prefetchMask) <= 1);
  assert(!p.prefetchEnqueue || (!u(decisionValid) && u(count) <= REQUEST_CAP - 2 && p.prefetchMissCount <= 2));
  assert(p.response < 0 || !u(requests[p.response].prefetch));
  uint8_t released = p.prefetchAbort;
  if (u(readValid) && u(requests[u(readIndex)].prefetch) && hitWay() >= 0) released |= static_cast<uint8_t>(1u << u(readIndex));
  if (p.response >= 0 && p.response != fillOwner && !p.wordReturn) released |= 1u << p.response;
  if (p.store >= 0) released |= 1u << p.store;
  if (p.fillFire) released |= 1u << fillOwner;
  if (p.exitValid) released |= 1u << oldestRequest(active);
  const int allocation = u(decisionValid) || p.prefetchEnqueue ? freeRequest() : -1;
  assert(!u(decisionValid) || (allocation >= 0 && u(count) < REQUEST_CAP));
  const bool firstFill = p.fillBeat && u(misses[p.fillMiss].fillWord) == 0;
  const int hit = hitWay();
  const uint32_t readSet = u(readValid) ? (request(u(readIndex)).address >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK : 0;
  const uint32_t victim = u(readValid) && hit < 0 ? selectVictim(readSet) : 0;
  bool pending = u(prefetchPending) != 0;
  if (p.prefetchEnqueue || p.prefetchDuplicate || p.prefetchStoreConflict ||
      p.prefetchMissCount >= 3 || u(count) > REQUEST_CAP - 2) pending = false;
  if (p.prefetchTrigger) pending = true;
  const bool stop = u(decisionValid) && static_cast<Operation>(u(decisionOp)) == Operation::Store && u(decisionAddr) == AXI_EXIT_ADDR;
  if (stop) pending = false;
  prefetchPending <= pending;
  if (stop) prefetchStopped <= 1;
  if (p.prefetchTrigger) {
    prefetchTarget <= p.prefetchNextLine;
    prefetchSeenVictim <= ((u(prefetchSeenVictim) + 1) & 3);
  }
  for (int h = 0; h < 4; ++h) {
    if (p.prefetchTrigger && u(prefetchSeenVictim) == unsigned(h)) {
      prefetchSeenValid[h] <= 1;
      prefetchSeenLine[h] <= (u(decisionAddr) & ~DCACHE_OFFSET_MASK);
    }
  }
  for (int h = 0; h < 4; ++h) {
    if (p.hitLocationTrain == h) {
      hitLocationValid[h] <= 1;
      hitLocationLine[h] <= (request(u(readIndex)).address >> DCACHE_OFFSET_BITS);
      hitLocationWay[h] <= hit;
    } else if (p.allocate >= 0 && (u(hitLocationLine[h]) & DCACHE_INDEX_MASK) == readSet) {
      hitLocationValid[h] <= 0;
    }
  }
  if (p.hitLocationTrain >= 0) hitLocationVictim <= ((p.hitLocationTrain + 1) & 3);
  for (int s = 0; s < REQUEST_CAP; ++s) {
    auto &r = requests[s];
    if (s == allocation) {
      r.op <= (p.prefetchEnqueue ? unsigned(Operation::Load) : u(decisionOp));
      r.value <= (p.prefetchEnqueue ? 0 : u(decisionValue));
      r.address <= (p.prefetchEnqueue ? u(prefetchTarget) : u(decisionAddr));
      r.isSigned <= (p.prefetchEnqueue ? 0 : u(decisionIsSigned));
      r.nEnc <= (p.prefetchEnqueue ? 2 : u(decisionNBytes));
      r.robTag <= (p.prefetchEnqueue ? 0 : u(decisionRobTag));
      r.memIndex <= (p.prefetchEnqueue ? 0 : u(decisionMemIndex));
      r.prefetch <= p.prefetchEnqueue;
      r.way <= 0; r.miss <= 0;
      r.active <= 1; r.ready <= 0; r.deferred <= 0; r.cancelled <= 0;
      assert(p.prefetchEnqueue || (u(decisionAddr) & DCACHE_OFFSET_MASK) + (1u << u(decisionNBytes)) <= DCACHE_BLOCK_CAP);
    } else {
      r.active <= (u(r.active) && !(released & (1u << s)));
      if (released & (1u << s)) { r.ready <= 0; r.deferred <= 0; r.miss <= 0; }
      else if (u(r.active)) {
        const bool merged = firstFill && request(fillOwner).op == Operation::Load && miss(s) == p.fillMiss && s != fillOwner;
         if (p.wordCapture & (1u << s)) {
           rawPayload[s] <= wordRaw(s, u(wordData)); r.ready <= 1; r.deferred <= 0; r.miss <= 0;
         } else if (merged) { rawPayload[s] <= memoryRaw(s); r.ready <= 1; r.deferred <= 0; r.miss <= 0; }
        else if (u(readValid) && int(u(readIndex)) == s) {
          if (hit >= 0) { rawPayload[s] <= p.readHitRaw; r.ready <= 1; r.way <= hit; }
          else { r.miss <= p.allocate + 1; r.deferred <= (p.allocate < 0); }
        } else if (p.mergeSlot == s) { r.miss <= p.mergeMiss + 1; r.deferred <= 0; }
      }
      if (u(r.active) && !u(r.prefetch) && request(s).op == Operation::Load && u(squashNeed) && !ROB::isOlder(request(s).robTag, u(squashTag))) r.cancelled <= 1;
    }
    uint8_t relation = 0;
    if ((active & ~released) & (1u << s)) {
      relation = u(olderMask[s]) & active & ~released;
      if (allocation >= 0) relation |= 1u << allocation;
    }
    olderMask[s] <= relation;
  }
  for (int id = 0; id < DCACHE_MSHR_CAP; ++id) {
    auto &m = misses[id];
    if (p.allocate == id) {
      assert(u(readValid) && hit < 0 && !u(m.valid) && !setLocked(request(u(readIndex)).address));
      const uint32_t meta = metadata[victim].readLane(0);
      const bool valid = (u(readWayMask) & (1u << victim)) != 0;
      m.valid <= 1; m.line <= (request(u(readIndex)).address & ~DCACHE_OFFSET_MASK);
      m.owner <= u(readIndex); m.way <= victim; m.victimTag <= (valid ? meta & DCACHE_META_TAG_MASK : 0);
      m.dirty <= (valid && (meta & DCACHE_META_DIRTY));
      m.readSent <= 0; m.writeSent <= 0; m.writeDone <= 0; m.responded <= 0; m.fillWord <= 0;
      for (int word = 0; word < DCACHE_WORDS_PER_LINE; ++word) {
        uint32_t value = 0;
        for (int lane = 0; lane < 4; ++lane) value |= datas[victim][word].readLane(lane) << (lane << 3);
        victimPayload[id][word] <= value;
      }
    } else if (p.fillFire && p.fillMiss == id) m.valid <= 0;
    else if (u(m.valid)) {
      assert(u(requests[u(m.owner)].active) && miss(u(m.owner)) == id);
      if (p.memoryRequest.readValid && p.memoryRequest.read.transaction == id) m.readSent <= 1;
      if (p.memoryRequest.writeValid && p.memoryRequest.write.transaction == id) m.writeSent <= 1;
      if (p.writeAck == id) m.writeDone <= 1;
      if (p.fillMiss == id && p.fillBeat) m.fillWord <= u(m.fillWord) + DCACHE_FILL_WORDS_PER_BEAT;
      if (p.fillMiss == id && p.response == int(u(m.owner))) m.responded <= 1;
      if (p.wordReturn && p.wordMiss == id) m.responded <= 1;
    }
  }
  if (u(readValid)) { if (hit >= 0) ++statHits; else ++statMisses; }
  for (int set = 0; set < NUM_OF_DCACHE_SETS; ++set) {
    if (p.fillFire && set == int((u(misses[p.fillMiss].line) >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK)) plru[set] <= dcachePlruTouch(u(plru[set]), u(misses[p.fillMiss].way));
    else if (p.storeHintFire && set == int((request(p.store).address >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK)) plru[set] <= dcachePlruTouch(u(plru[set]), p.storeHintWay);
    else if (u(readValid) && hit >= 0 && set == int(readSet)) plru[set] <= dcachePlruTouch(u(plru[set]), hit);
    for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
      if (p.allocate >= 0 && set == int(readSet) && uint32_t(w) == victim) validBits[set][w] <= 0;
      else if (p.fillFire && set == int((u(misses[p.fillMiss].line) >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK) && uint32_t(w) == u(misses[p.fillMiss].way)) validBits[set][w] <= 1;
    }
  }
  if (p.acceptLookup) {
    assert(allocation >= 0 && !p.fillBeat && p.store < 0 && p.lookup < 0 &&
           !setLocked(u(decisionAddr)));
    assert(!u(readValid) || (hit >= 0 && request(u(readIndex)).op == Operation::Load &&
                           p.response == int(u(readIndex))));
    for (int s = 0; s < REQUEST_CAP; ++s)
      if (u(requests[s].active) && request(s).op == Operation::Store)
        assert((request(s).address >> DCACHE_OFFSET_BITS) != (u(decisionAddr) >> DCACHE_OFFSET_BITS) &&
               request(s).address != AXI_EXIT_ADDR);
  }
  readValid <= (p.lookup >= 0 || p.acceptLookup);
  if (p.lookup >= 0 || p.acceptLookup) {
    const uint32_t address = p.acceptLookup ? u(decisionAddr) : request(p.lookup).address;
    const uint32_t set = (address >> DCACHE_OFFSET_BITS) & DCACHE_INDEX_MASK;
    uint32_t mask = 0;
    for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) if (u(validBits[set][w])) mask |= 1u << w;
    readIndex <= (p.acceptLookup ? allocation : p.lookup); readWayMask <= mask;
    readOffset <= (address & DCACHE_OFFSET_MASK);
    readNEnc <= (p.acceptLookup ? u(decisionNBytes) : u(requests[p.lookup].nEnc));
  }
  if (allocation >= 0) cursor <= ((allocation + 1) & (REQUEST_CAP - 1));
  count <= u(count) + unsigned(u(decisionValid) != 0) + unsigned(p.prefetchEnqueue) - std::popcount(released);
  for (int w = 0; w < NUM_OF_DCACHE_WAYS; ++w) {
    for (auto &bank : datas[w]) bank.work();
    metadata[w].work();
  }
}
