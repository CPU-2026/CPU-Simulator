#pragma once
#include "Memory.hpp"
#include "common.h"
#include "module.h"
#include "tools.h"
#include <array>
#include <cstdint>
#include <vector>

// External memory model behind the AXI4-Lite bus. It replaces DMEM/IMEM: all
// data lives here (256 MiB little-endian RAM preloaded with the stdin image),
// every channel keeps a depth-16 FIFO, requests are served at most one read
// plus one write per cycle after a fixed latency, and the 0x80000000 word
// write terminates with the captured return value. Behaviour mirrors the
// course simulator slave (scripts/sim.cpp).
struct SlaveInput {
  Wire<1> arValid;
  Wire<32> arAddr;
  Wire<1> rReady;
  Wire<1> awValid;
  Wire<32> awAddr;
  Wire<1> wValid;
  Wire<32> wData;
  Wire<4> wStrb;
  Wire<1> bReady;
};
struct SlaveOutput {
  Wire<1> arReady;
  Wire<1> rValid;
  Wire<32> rData;
  Wire<2> rResp;
  Wire<1> awReady;
  Wire<1> wReady;
  Wire<1> bValid;
  Wire<2> bResp;
};
struct SlaveReadEntry {
  Register<32> data;
  Register<2> resp;
  Register<32> due;
};
struct SlaveWriteEntry {
  Register<2> resp;
  Register<32> result;
  Register<32> due;
  Register<1> exit;
};
struct SlaveWDataEntry {
  Register<32> data;
  Register<4> strb;
};
struct SlaveInner {
  std::array<Register<32>, AXI_DEPTH> arQueue;
  Register<4> arHead;
  Register<5> arCount;
  std::array<Register<32>, AXI_DEPTH> awQueue;
  Register<4> awHead;
  Register<5> awCount;
  std::array<SlaveWDataEntry, AXI_DEPTH> wQueue;
  Register<4> wHead;
  Register<5> wCount;
  std::array<SlaveReadEntry, AXI_DEPTH> rQueue;
  Register<4> rHead;
  Register<5> rCount;
  std::array<SlaveWriteEntry, AXI_DEPTH> bQueue;
  Register<4> bHead;
  Register<5> bCount;
  Register<32> cycle;
  Register<1> done;
  Register<32> result;
};
struct Slave : dark::Module<SlaveInput, SlaveOutput, SlaveInner> {
  explicit Slave(const Memory &mem);
  bool isDone() const { return static_cast<bool>(done); }
  uint32_t exitResult() const { return static_cast<uint32_t>(result); }
  void work() override;

 private:
  uint32_t response(uint32_t addr) const;
  uint32_t loadWord(uint32_t addr) const;
  void storeBytes(uint32_t addr, uint32_t data, uint32_t strb);
  // Plain (unsynced) payload array: only this module touches it and read data
  // is captured into R entries, so it needs no Register discipline.
  std::vector<uint8_t> ram;
};
