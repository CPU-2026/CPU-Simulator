#pragma once
#include "SRAM.hpp"
#include "common.h"
#include "module.h"
#include "tools.h"
#include <array>
#include <cstdint>

static_assert(FETCH_REQUEST_CAP >= 2 && (FETCH_REQUEST_CAP & (FETCH_REQUEST_CAP - 1)) == 0);
constexpr int ICACHE_REQUEST_BITS = std::bit_width(FETCH_REQUEST_CAP - 1u);

struct CacheLine {
  Register<1> valid;
  Register<ICACHE_TAG_WIDTH> tag;
  Register<ICACHE_WORDS_PER_LINE> controlMask;
};

struct ICacheInput {
  Wire<1> needSquash;
  Wire<1> fetchValid;
  Wire<32> fetchPC;
  std::array<Wire<32>, FETCH_WIDTH> fetchPredictPC;
  Wire<CKPT_ID_WIDTH> fetchCkptId;
  Wire<3> fetchCount;
  Wire<1> popConsume;
  LineReturn lineReturn;
};

struct ICacheOutput {
  struct ICacheRequest {
    std::array<Register<32>, FETCH_WIDTH> raw_inst;
    std::array<Register<32>, FETCH_WIDTH> PC;
    std::array<Register<32>, FETCH_WIDTH> predictPC;
    Register<CKPT_ID_WIDTH> ckptId;
    Register<1> valid;
    Register<1> ready;
    Register<3> laneCount;
  };
  std::array<ICacheRequest, FETCH_REQUEST_CAP> requestBuffer;
  Wire<1> fillFire;
  Wire<1> readBlocked;
};

struct ICacheInner {
  SRAM<NUM_OF_ICACHE_SETS, ICACHE_LINE_BITS, RV32_WORD_BITS> ICacheData;
  std::array<CacheLine, NUM_OF_ICACHE_SETS> blocks;
  Register<ICACHE_REQUEST_BITS> head;
  Register<ICACHE_REQUEST_BITS> readIndex;
  Register<ICACHE_WORD_BITS> readLaneIndex;
  Register<1> readValid;
};

struct ICache : dark::Module<ICacheInput, ICacheOutput, ICacheInner> {
  ICache();
  // host-only cache profile counters (mirrors the main tree's ICache
  // hitCount/missCount). Incremented at the stage-1 push, once per accepted
  // fetch request. They are statistics, never read by the datapath, so they
  // cannot perturb the modelled behaviour.
  uint64_t statHits = 0;
  uint64_t statMisses = 0;

  // Window invariant over requestBuffer.valid[]; occupancy is derived, not
  // stored (same scheme as IMEM).
  uint32_t occupancy() const {
    uint32_t n = 0;
    for (int i = 0; i < FETCH_REQUEST_CAP; ++i)
      n += static_cast<bool>(requestBuffer[i].valid) ? 1u : 0u;
    return n;
  }
  bool isRequestFull() const { return occupancy() == FETCH_REQUEST_CAP; }
  // Only the occupied head may bypass the request-slot register. Read data
  // and the saved slot/lane identity all come from the committed old state.
  bool headReadReady() const {
    const auto h = static_cast<uint32_t>(head);
    return static_cast<bool>(requestBuffer[h].valid) &&
           !static_cast<bool>(requestBuffer[h].ready) &&
           static_cast<bool>(readValid) &&
           static_cast<uint32_t>(readIndex) == h;
  }
  // A saved instruction or a matching completed SRAM read is deliverable.
  bool isReturnReady() const {
    auto h = static_cast<uint32_t>(head);
    return static_cast<bool>(requestBuffer[h].valid) &&
           (static_cast<bool>(requestBuffer[h].ready) || headReadReady());
  }
  bool hit(uint32_t addr) const {
    auto index = (addr >> ICACHE_OFFSET_BITS) & ICACHE_INDEX_MASK;
    return static_cast<bool>(blocks[index].valid) &&
           static_cast<uint32_t>(blocks[index].tag) ==
                (addr >> ICACHE_TAG_SHIFT);
  }
  static bool isControl(uint32_t raw) {
    return (raw & 127u) == 0x63u || (raw & 127u) == 0x6fu ||
           ((raw & 127u) == 0x67u && ((raw >> 12) & 7u) == 0);
  }
  bool firstControl(uint32_t pc) const {
    return hit(pc) && ((uint32_t(blocks[(pc >> ICACHE_OFFSET_BITS) & ICACHE_INDEX_MASK].controlMask) >>
                       ((pc >> 2) & ICACHE_WORD_INDEX_MASK)) & 1u);
  }
  bool refillHit(uint32_t pc) const {
    return static_cast<bool>(fillFire) &&
           (pc & ~ICACHE_OFFSET_MASK) == static_cast<uint32_t>(lineReturn.lineAddr);
  }
  uint32_t returnCount() const {
    return static_cast<uint32_t>(
        requestBuffer[static_cast<uint32_t>(head)].laneCount);
  }
  uint32_t returnRaw(unsigned L) const {
    return headReadReady()
               ? ICacheData.readLane(static_cast<uint32_t>(readLaneIndex) + L)
               : static_cast<uint32_t>(
                     requestBuffer[static_cast<uint32_t>(head)].raw_inst[L]);
  }
  uint32_t returnPC(unsigned L) const {
    return static_cast<uint32_t>(
        requestBuffer[static_cast<uint32_t>(head)].PC[L]);
  }
  uint32_t returnPredictPC(unsigned L) const {
    return static_cast<uint32_t>(
        requestBuffer[static_cast<uint32_t>(head)].predictPC[L]);
  }
  uint32_t returnCkptId() const {
    return static_cast<uint32_t>(
        requestBuffer[static_cast<uint32_t>(head)].ckptId);
  }
  bool refillValid() const {
    bool found = false;
    const uint32_t occ = occupancy();
    const bool offered = static_cast<bool>(lineReturn.valid);
    const uint32_t lineAddr = static_cast<uint32_t>(lineReturn.lineAddr);
    for (int age = 0; age < FETCH_REQUEST_CAP; ++age) {
      const auto slot = (static_cast<uint32_t>(head) + age) & (FETCH_REQUEST_CAP - 1);
      const uint32_t requestLine =
          static_cast<uint32_t>(requestBuffer[slot].PC[0]) &
          ~ICACHE_OFFSET_MASK;
      if (offered && age < occ && !found &&
          static_cast<bool>(requestBuffer[slot].valid) &&
          !static_cast<bool>(requestBuffer[slot].ready) &&
          !(static_cast<bool>(readValid) &&
            static_cast<uint32_t>(readIndex) == slot) && requestLine == lineAddr) {
        found = true;
      }
    }
    return found;
  }
  uint8_t refillSlot() const {
    bool found = false;
    uint8_t refillTarget = 0;
    const uint32_t occ = occupancy();
    const bool offered = static_cast<bool>(lineReturn.valid);
    const uint32_t lineAddr = static_cast<uint32_t>(lineReturn.lineAddr);
    for (int age = 0; age < FETCH_REQUEST_CAP; ++age) {
      const auto slot = (static_cast<uint32_t>(head) + age) & (FETCH_REQUEST_CAP - 1);
      const uint32_t requestLine =
          static_cast<uint32_t>(requestBuffer[slot].PC[0]) &
          ~ICACHE_OFFSET_MASK;
      if (offered && age < occ && !found &&
          static_cast<bool>(requestBuffer[slot].valid) &&
          !static_cast<bool>(requestBuffer[slot].ready) &&
          !(static_cast<bool>(readValid) &&
            static_cast<uint32_t>(readIndex) == slot) && requestLine == lineAddr) {
        found = true;
        refillTarget = static_cast<uint8_t>(slot);
      }
    }
    return refillTarget;
  }
  // combinational predicate: the ICache head holds the halt instruction.
  // Member function (not a stored Register) so it is visible the same cycle.
  bool isHaltSignal() const {
    if (!legacyHaltEnabled || !isReturnReady())
      return false;
    const unsigned cnt = returnCount();
    for (unsigned L = 0; L < cnt; ++L)
      if (returnRaw(L) == 0x0ff00513)
        return true;
    return false;
  }
  void work() override;
};
