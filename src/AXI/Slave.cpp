#include "../include/Slave.hpp"
#include <cstdint>

Slave::Slave(const Memory &mem) : ram(AXI_RAM_BYTES, 0) {
  for (uint32_t i = 0; i < MEM_SIZE; ++i)
    ram[i] = mem.read_data(i);

  arReady = [this]() -> uint32_t {
    return static_cast<uint32_t>(arCount) < AXI_DEPTH ? 1u : 0u;
  };
  rValid = [this]() -> uint32_t {
    const uint32_t h = static_cast<uint32_t>(rHead);
    return (static_cast<uint32_t>(rCount) != 0 &&
            static_cast<uint32_t>(rQueue[h].due) <= static_cast<uint32_t>(cycle))
               ? 1u
               : 0u;
  };
  rData = [this]() -> uint32_t {
    const uint32_t h = static_cast<uint32_t>(rHead);
    return static_cast<uint32_t>(rCount) != 0
               ? static_cast<uint32_t>(rQueue[h].data)
               : 0u;
  };
  rResp = [this]() -> uint32_t {
    const uint32_t h = static_cast<uint32_t>(rHead);
    return static_cast<uint32_t>(rCount) != 0
               ? static_cast<uint32_t>(rQueue[h].resp)
               : 0u;
  };
  awReady = [this]() -> uint32_t {
    return static_cast<uint32_t>(awCount) < AXI_DEPTH ? 1u : 0u;
  };
  wReady = [this]() -> uint32_t {
    return static_cast<uint32_t>(wCount) < AXI_DEPTH ? 1u : 0u;
  };
  bValid = [this]() -> uint32_t {
    const uint32_t h = static_cast<uint32_t>(bHead);
    return (static_cast<uint32_t>(bCount) != 0 &&
            static_cast<uint32_t>(bQueue[h].due) <= static_cast<uint32_t>(cycle))
               ? 1u
               : 0u;
  };
  bResp = [this]() -> uint32_t {
    const uint32_t h = static_cast<uint32_t>(bHead);
    return static_cast<uint32_t>(bCount) != 0
               ? static_cast<uint32_t>(bQueue[h].resp)
               : 0u;
  };
}

uint32_t Slave::response(uint32_t addr) const {
  if (addr & 3u)
    return AXI_SLVERR;
  return static_cast<uint64_t>(addr) + 4 > AXI_RAM_BYTES ? AXI_DECERR : AXI_OKAY;
}

uint32_t Slave::loadWord(uint32_t addr) const {
  uint32_t w = 0;
  for (int b = 0; b < RV32_WORD_BYTES; ++b)
    w |= static_cast<uint32_t>(ram[addr + static_cast<uint32_t>(b)]) << (b << 3);
  return w;
}

void Slave::storeBytes(uint32_t addr, uint32_t data, uint32_t strb) {
  for (int b = 0; b < RV32_WORD_BYTES; ++b)
    if (strb & (1u << b))
      ram[addr + static_cast<uint32_t>(b)] = static_cast<uint8_t>(data >> (b << 3));
}

void Slave::work() {
  const uint32_t cyc = static_cast<uint32_t>(cycle);
  const uint32_t arH = static_cast<uint32_t>(arHead);
  const uint32_t awH = static_cast<uint32_t>(awHead);
  const uint32_t wH = static_cast<uint32_t>(wHead);
  const uint32_t rH = static_cast<uint32_t>(rHead);
  const uint32_t bH = static_cast<uint32_t>(bHead);
  const uint32_t arN = static_cast<uint32_t>(arCount);
  const uint32_t awN = static_cast<uint32_t>(awCount);
  const uint32_t wN = static_cast<uint32_t>(wCount);
  const uint32_t rN = static_cast<uint32_t>(rCount);
  const uint32_t bN = static_cast<uint32_t>(bCount);

  // Handshakes on the five channels (fire = valid && ready at the same edge).
  const bool rFire = rN != 0 && static_cast<uint32_t>(rQueue[rH].due) <= cyc &&
                     static_cast<bool>(rReady);
  const bool bFire = bN != 0 && static_cast<uint32_t>(bQueue[bH].due) <= cyc &&
                     static_cast<bool>(bReady);
  const bool arFire = static_cast<bool>(arValid) && static_cast<bool>(arReady);
  const bool awFire = static_cast<bool>(awValid) && static_cast<bool>(awReady);
  const bool wFire = static_cast<bool>(wValid) && static_cast<bool>(wReady);

  // Responses are consumed before new services start, so a full output FIFO
  // drains in the same cycle its entry is handed out.
  const uint32_t rFree = rN - (rFire ? 1u : 0u);
  const uint32_t bFree = bN - (bFire ? 1u : 0u);
  const bool svcRead = arN != 0 && rFree < AXI_DEPTH;
  const bool svcWrite = awN != 0 && wN != 0 && bFree < AXI_DEPTH;
  uint32_t rng = cyc == 0 ? 1u : static_cast<uint32_t>(random);
  if (svcRead && svcWrite) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; }
  const bool writeFirst = svcRead && svcWrite && (rng & 1u);
  if (cyc == 0 || (svcRead && svcWrite)) random <= rng;

  // Capture the old read before applying the write; the course model chooses
  // the read/write service order with its deterministic xorshift sequence.
  uint32_t rdData = 0;
  uint32_t rdResp = AXI_OKAY;
  if (svcRead) {
    const uint32_t addr = static_cast<uint32_t>(arQueue[arH]);
    rdResp = response(addr);
    if (rdResp == AXI_OKAY) rdData = loadWord(addr);
  }

  // The write is applied before the read service (deterministic order).
  uint32_t wrResp = AXI_OKAY;
  uint32_t wrResult = 0;
  bool wrExit = false;
  if (svcWrite) {
    const uint32_t addr = static_cast<uint32_t>(awQueue[awH]);
    const uint32_t data = static_cast<uint32_t>(wQueue[wH].data);
    const uint32_t strb = static_cast<uint32_t>(wQueue[wH].strb);
    wrResult = data;
    wrExit = (addr == AXI_EXIT_ADDR) && (strb == 0xfu);
    wrResp = (addr == AXI_EXIT_ADDR) ? (wrExit ? AXI_OKAY : AXI_SLVERR)
                                     : response(addr);
    if (wrResp == AXI_OKAY && !wrExit)
      storeBytes(addr, data, strb);
  }
  if (svcRead && writeFirst) {
    const uint32_t addr = static_cast<uint32_t>(arQueue[arH]);
    rdResp = response(addr);
    if (rdResp == AXI_OKAY)
      rdData = loadWord(addr);
  }

  // Accepted requests enter the tail slot; a serviced entry leaves its slot
  // to the new service in the same cycle (head advances past it).
  const uint32_t arTail = (arH + arN) & (AXI_DEPTH - 1);
  const uint32_t awTail = (awH + awN) & (AXI_DEPTH - 1);
  const uint32_t wTail = (wH + wN) & (AXI_DEPTH - 1);
  const uint32_t rTail = (rH + rN) & (AXI_DEPTH - 1);
  const uint32_t bTail = (bH + bN) & (AXI_DEPTH - 1);
  if (arFire)
    arQueue[arTail] <= static_cast<uint32_t>(arAddr);
  if (awFire)
    awQueue[awTail] <= static_cast<uint32_t>(awAddr);
  if (wFire) {
    wQueue[wTail].data <= static_cast<uint32_t>(wData);
    wQueue[wTail].strb <= static_cast<uint32_t>(wStrb);
  }
  if (svcRead) {
    rQueue[rTail].data <= rdData;
    rQueue[rTail].resp <= rdResp;
    rQueue[rTail].due <= cyc + 1 + AXI_LATENCY;
  }
  if (svcWrite) {
    bQueue[bTail].resp <= wrResp;
    bQueue[bTail].result <= wrResult;
    bQueue[bTail].due <= cyc + 1 + AXI_LATENCY;
    bQueue[bTail].exit <= wrExit;
  }

  arHead <= ((arH + (svcRead ? 1u : 0u)) & (AXI_DEPTH - 1));
  arCount <= arN - (svcRead ? 1u : 0u) + (arFire ? 1u : 0u);
  awHead <= ((awH + (svcWrite ? 1u : 0u)) & (AXI_DEPTH - 1));
  awCount <= awN - (svcWrite ? 1u : 0u) + (awFire ? 1u : 0u);
  wHead <= ((wH + (svcWrite ? 1u : 0u)) & (AXI_DEPTH - 1));
  wCount <= wN - (svcWrite ? 1u : 0u) + (wFire ? 1u : 0u);
  rHead <= ((rH + (rFire ? 1u : 0u)) & (AXI_DEPTH - 1));
  rCount <= rFree + (svcRead ? 1u : 0u);
  bHead <= ((bH + (bFire ? 1u : 0u)) & (AXI_DEPTH - 1));
  bCount <= bFree + (svcWrite ? 1u : 0u);

  if (bFire && static_cast<bool>(bQueue[bH].exit)) {
    done <= 1;
    result <= static_cast<uint32_t>(bQueue[bH].result);
  }
  cycle <= cyc + 1;
}
