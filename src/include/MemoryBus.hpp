#pragma once
#include "common.h"
#include <array>

// Plain combinational payloads; persistent state is held only in Registers.
struct CacheRequest {
  Operation op = Operation::Load;
  int32_t value = 0;
  uint32_t address = 0;
  bool isSigned = false;
  int nBytes = 4;
  uint8_t robTag = 0, memIndex = 0;
};
struct CacheRead { uint32_t address = 0; uint8_t transaction = 0; };
struct CacheWrite : CacheRead { std::array<uint8_t, DCACHE_BLOCK_CAP> lineData{}; };
struct CacheMemoryRequest {
  bool readValid = false, writeValid = false;
  CacheRead read{};
  CacheWrite write{};
};
template<unsigned LANES> struct CacheSRAMPort {
  bool enable = false, writeEnable = false;
  uint32_t addr = 0;
  std::array<bool, LANES> writeMask{};
  std::array<uint32_t, LANES> writeData{};
};
