#include "AXIData.hpp"
#include <cassert>

namespace {
template<class T> uint32_t u(const T &v) { return static_cast<uint32_t>(v); }
uint32_t freeDataSlot(uint32_t busy) {
  uint32_t slot = 0;
  bool found = false;
  for (uint32_t i = 0; i < AXI_DATA_CAP; ++i) {
    if (!found && !(busy & (1u << i))) { slot = i; found = true; }
  }
  assert(found);
  return slot;
}
}

AXIData::AXIData() {
  arValid = [this]() { return readIssue() >= 0; };
  arAddr = [this]() { const int s = readIssue(); return s >= 0 ? u(readEntry(s).address) + (u(readEntry(s).issued) << 2) : 0; };
  rReady = []() { return 1u; };
  awValid = [this]() { return writeIssue(true) >= 0; };
  awAddr = [this]() { const int s = writeIssue(true); return s >= 0 ? u(writeEntry(s).address) + (u(writeEntry(s).issued) << 2) : 0; };
  wValid = [this]() { return writeIssue(false) >= 0; };
  wData = [this]() {
    const int s = writeIssue(false);
    uint32_t data = 0;
    if (s >= 0) data = u(payload[u(writeOrder[s])][u(writeEntry(s).w)]);
    return data;
  };
  wStrb = [this]() { return writeIssue(false) >= 0 ? 15u : 0u; };
  bReady = []() { return 1u; };
}
uint32_t AXIData::freeCredits() const { return AXI_DATA_CAP - u(rn) - u(wn); }
bool AXIData::exitReady() const { return u(rn) == 0 && u(wn) == 0; }
bool AXIData::isReplyReady() const { return u(rn) != 0 && u(readEntry(u(rh)).collected) == DCACHE_WORDS_PER_LINE; }
bool AXIData::isWriteReplyReady() const { return u(wn) != 0 && u(writeEntry(u(wh)).collected) == (u(writeEntry(u(wh)).mmio) ? 1u : DCACHE_WORDS_PER_LINE); }
uint8_t AXIData::lineByte(int b) const { return isReplyReady() ? u(payload[u(readOrder[u(rh)])][b >> 2]) >> ((b & 3) << 3) : 0; }
uint32_t AXIData::replyAddress() const { return u(readEntry(u(rh)).address); }
uint32_t AXIData::replyTransaction() const { return u(readEntry(u(rh)).transaction); }
uint32_t AXIData::writeReplyAddress() const { return u(writeEntry(u(wh)).address); }
uint32_t AXIData::writeReplyTransaction() const { return u(writeEntry(u(wh)).transaction); }
int AXIData::readIssue() const {
  return u(ra) != u(rt) ? int(u(ra) & 15) : -1;
}
int AXIData::writeIssue(bool address) const {
  const uint32_t cursor = address ? u(wa) : u(wd);
  return cursor != u(wt) ? int(cursor & 15) : -1;
}
int AXIData::readCollect() const {
  const int s = u(rr) & 15;
  return u(rr) != u(rt) && u(readEntry(s).collected) < u(readEntry(s).issued) ? s : -1;
}
int AXIData::writeCollect() const {
  const int s = u(wb) & 15;
  return u(wb) != u(wt) && u(writeEntry(s).collected) < u(writeEntry(s).issued) &&
      u(writeEntry(s).collected) < u(writeEntry(s).w) ? s : -1;
}
void AXIData::work() {
  const bool pushW = u(writeValid) || u(exitValid), pushR = u(readValid);
  assert(!pushW || freeCredits() > 0);
  assert(!pushR || freeCredits() > unsigned(pushW));
  assert(!u(exitValid) || (!u(writeValid) && exitReady()));
  uint32_t reserved = u(dataBusy), allocated = 0;
  uint32_t writeSlot = 0, readSlot = 0;
  if (pushW) {
    writeSlot = freeDataSlot(reserved);
    allocated |= 1u << writeSlot;
    reserved |= 1u << writeSlot;
  }
  if (pushR) { readSlot = freeDataSlot(reserved); allocated |= 1u << readSlot; }
  const int rd = readIssue(), aw = writeIssue(true), wd = writeIssue(false);
  if (rd >= 0 && u(arReady)) {
    entries[u(readOrder[rd])].issued <= u(readEntry(rd).issued) + 1;
    if (u(readEntry(rd).issued) + 1 == DCACHE_WORDS_PER_LINE) ra <= ((u(ra) + 1) & 31);
  }
  if (aw >= 0 && u(awReady)) {
    entries[u(writeOrder[aw])].issued <= u(writeEntry(aw).issued) + 1;
    if (u(writeEntry(aw).issued) + 1 == (u(writeEntry(aw).mmio) ? 1u : DCACHE_WORDS_PER_LINE)) wa <= ((u(wa) + 1) & 31);
  }
  if (wd >= 0 && u(wReady)) {
    entries[u(writeOrder[wd])].w <= u(writeEntry(wd).w) + 1;
    if (u(writeEntry(wd).w) + 1 == (u(writeEntry(wd).mmio) ? 1u : DCACHE_WORDS_PER_LINE)) this->wd <= ((u(this->wd) + 1) & 31);
  }
  if (u(rValid)) {
    const int s = readCollect();
    assert(s >= 0 && u(rResp) == AXI_OKAY);
    payload[u(readOrder[s])][u(readEntry(s).collected)] <= u(rData);
    entries[u(readOrder[s])].collected <= u(readEntry(s).collected) + 1;
    if (u(readEntry(s).collected) + 1 == DCACHE_WORDS_PER_LINE) rr <= ((u(rr) + 1) & 31);
  }
  if (u(bValid)) {
    const int s = writeCollect();
    assert(s >= 0 && u(bResp) == AXI_OKAY);
    entries[u(writeOrder[s])].collected <= u(writeEntry(s).collected) + 1;
    if (u(writeEntry(s).collected) + 1 == (u(writeEntry(s).mmio) ? 1u : DCACHE_WORDS_PER_LINE)) wb <= ((u(wb) + 1) & 31);
  }
  if (pushR) {
    readOrder[(u(rh) + u(rn)) & 15] <= readSlot;
    auto &r = entries[readSlot];
    r.address <= u(readAddress); r.transaction <= u(readTransaction);
    r.issued <= 0; r.collected <= 0;
    rt <= ((u(rt) + 1) & 31);
  }
  if (pushW) {
    writeOrder[(u(wh) + u(wn)) & 15] <= writeSlot;
    auto &w = entries[writeSlot];
    w.address <= (u(exitValid) ? AXI_EXIT_ADDR : u(writeAddress));
    w.transaction <= u(writeTransaction); w.issued <= 0; w.w <= 0; w.collected <= 0; w.mmio <= u(exitValid);
    for (int word = 0; word < DCACHE_WORDS_PER_LINE; ++word) {
      uint32_t value = 0;
      for (int byte = 0; byte < 4; ++byte)
        value |= u(writeLineData[(word << 2) + byte]) << (byte << 3);
      if (!u(exitValid) || word == 0) payload[writeSlot][word] <= (u(exitValid) ? u(exitValue) : value);
    }
    wt <= ((u(wt) + 1) & 31);
  }
  if (u(replyConsumed)) { assert(isReplyReady()); rh <= ((u(rh) + 1) & 15); }
  if (u(writeConsumed)) { assert(isWriteReplyReady()); wh <= ((u(wh) + 1) & 15); }
  rn <= u(rn) + unsigned(pushR) - unsigned(u(replyConsumed) != 0);
  wn <= u(wn) + unsigned(pushW) - unsigned(u(writeConsumed) != 0);
  uint32_t released = 0;
  if (u(replyConsumed)) released |= 1u << u(readOrder[u(rh)]);
  if (u(writeConsumed)) released |= 1u << u(writeOrder[u(wh)]);
  dataBusy <= ((u(dataBusy) & ~released) | allocated);
}
