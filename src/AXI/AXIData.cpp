#include "AXIData.hpp"
#include <cassert>

namespace { template<class T> uint32_t u(const T &v) { return static_cast<uint32_t>(v); } }

AXIData::AXIData() {
  arValid = [this]() { return readIssue() >= 0; };
  arAddr = [this]() { const int s = readIssue(); return s >= 0 ? u(reads[s].address) + (u(reads[s].issued) << 2) : 0; };
  rReady = []() { return 1u; };
  awValid = [this]() { return writeIssue(true) >= 0; };
  awAddr = [this]() { const int s = writeIssue(true); return s >= 0 ? u(writes[s].address) + (u(writes[s].aw) << 2) : 0; };
  wValid = [this]() { return writeIssue(false) >= 0; };
  wData = [this]() {
    const int s = writeIssue(false);
    uint32_t data = 0;
    if (s >= 0) for (int b = 0; b < 4; ++b) data |= u(writes[s].bytes[(u(writes[s].w) << 2) + b]) << (b << 3);
    return data;
  };
  wStrb = [this]() { return writeIssue(false) >= 0 ? 15u : 0u; };
  bReady = []() { return 1u; };
}
uint32_t AXIData::freeCredits() const { return AXI_DEPTH - u(rn) - u(wn); }
bool AXIData::exitReady() const { return u(rn) == 0 && u(wn) == 0; }
bool AXIData::isReplyReady() const { return u(rn) != 0 && u(reads[u(rh)].collected) == DCACHE_WORDS_PER_LINE; }
bool AXIData::isWriteReplyReady() const { return u(wn) != 0 && u(writes[u(wh)].collected) == (u(writes[u(wh)].mmio) ? 1u : DCACHE_WORDS_PER_LINE); }
uint8_t AXIData::lineByte(int b) const { return u(reads[u(rh)].bytes[b]); }
uint32_t AXIData::replyAddress() const { return u(reads[u(rh)].address); }
uint32_t AXIData::replyTransaction() const { return u(reads[u(rh)].transaction); }
uint32_t AXIData::writeReplyAddress() const { return u(writes[u(wh)].address); }
uint32_t AXIData::writeReplyTransaction() const { return u(writes[u(wh)].transaction); }
int AXIData::readIssue() const {
  int found = -1;
  for (int age = 0; age < AXI_DEPTH; ++age) {
    const int s = (u(rh) + age) & 15;
    if (uint32_t(age) < u(rn) && found < 0 && u(reads[s].issued) < DCACHE_WORDS_PER_LINE) found = s;
  }
  return found;
}
int AXIData::writeIssue(bool address) const {
  int found = -1;
  for (int age = 0; age < AXI_DEPTH; ++age) {
    const int s = (u(wh) + age) & 15;
    if (uint32_t(age) < u(wn) && found < 0 && (address ? u(writes[s].aw) : u(writes[s].w)) <
        (u(writes[s].mmio) ? 1u : DCACHE_WORDS_PER_LINE)) found = s;
  }
  return found;
}
int AXIData::readCollect() const {
  int found = -1;
  for (int age = 0; age < AXI_DEPTH; ++age) {
    const int s = (u(rh) + age) & 15;
    if (uint32_t(age) < u(rn) && found < 0 && u(reads[s].collected) < u(reads[s].issued)) found = s;
  }
  return found;
}
int AXIData::writeCollect() const {
  int found = -1;
  for (int age = 0; age < AXI_DEPTH; ++age) {
    const int s = (u(wh) + age) & 15;
    if (uint32_t(age) < u(wn) && found < 0 && u(writes[s].collected) < u(writes[s].aw) && u(writes[s].collected) < u(writes[s].w)) found = s;
  }
  return found;
}
void AXIData::work() {
  const bool pushW = u(writeValid) || u(exitValid), pushR = u(readValid);
  assert(!pushW || freeCredits() > 0);
  assert(!pushR || freeCredits() > unsigned(pushW));
  assert(!u(exitValid) || (!u(writeValid) && exitReady()));
  const int rd = readIssue(), aw = writeIssue(true), wd = writeIssue(false);
  if (rd >= 0 && u(arReady)) reads[rd].issued <= u(reads[rd].issued) + 1;
  if (aw >= 0 && u(awReady)) writes[aw].aw <= u(writes[aw].aw) + 1;
  if (wd >= 0 && u(wReady)) writes[wd].w <= u(writes[wd].w) + 1;
  if (u(rValid)) {
    const int s = readCollect();
    assert(s >= 0 && u(rResp) == AXI_OKAY);
    for (int b = 0; b < 4; ++b) reads[s].bytes[(u(reads[s].collected) << 2) + b] <= u(rData) >> (b << 3);
    reads[s].collected <= u(reads[s].collected) + 1;
  }
  if (u(bValid)) {
    const int s = writeCollect();
    assert(s >= 0 && u(bResp) == AXI_OKAY);
    writes[s].collected <= u(writes[s].collected) + 1;
  }
  if (pushR) {
    auto &r = reads[(u(rh) + u(rn)) & 15];
    r.address <= u(readAddress); r.transaction <= u(readTransaction);
    r.issued <= 0; r.collected <= 0;
  }
  if (pushW) {
    auto &w = writes[(u(wh) + u(wn)) & 15];
    w.address <= (u(exitValid) ? AXI_EXIT_ADDR : u(writeAddress));
    w.transaction <= u(writeTransaction); w.aw <= 0; w.w <= 0; w.collected <= 0; w.mmio <= u(exitValid);
    for (int b = 0; b < DCACHE_BLOCK_CAP; ++b) w.bytes[b] <= (u(exitValid)
        ? (b < 4 ? u(exitValue) >> ((b & 3) << 3) : 0) : u(writeLineData[b]));
  }
  if (u(replyConsumed)) { assert(isReplyReady()); rh <= ((u(rh) + 1) & 15); }
  if (u(writeConsumed)) { assert(isWriteReplyReady()); wh <= ((u(wh) + 1) & 15); }
  rn <= u(rn) + unsigned(pushR) - unsigned(u(replyConsumed) != 0);
  wn <= u(wn) + unsigned(pushW) - unsigned(u(writeConsumed) != 0);
}
