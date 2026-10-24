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
};

struct ICacheInput {
  Wire<1> needSquash;
  Wire<1> fetchValid;
  Wire<32> fetchPC; // Ungated PC: read-port backpressure must not depend on fetchValid.
  Wire<32> fetchPredictPC;
  Wire<CKPT_ID_WIDTH> fetchCkptId;
  Wire<1> fetchValid1;
  Wire<32> fetchPredictPC1;
  Wire<1> popConsume;
  LineReturn lineReturn; // Offered IMEM return, before the fillFire handshake.
};

struct ICacheOutput {
  struct ICacheRequest {
    // valid marks the slot as occupied (window bitmap -> occupancy source);
    // ready marks the instruction as deliverable (hit or backfilled).
    // This split keeps placeholders inside the window so the derived-count
    // scheme stays exact. A slot holds one fetch group: lane0 always valid
    // when the slot is valid, lane1 valid only when FETCH_WIDTH == 2 and the
    // group did not degrade.
    Register<32> raw_inst;
    Register<32> PC;
    Register<32> predictPC;
    Register<CKPT_ID_WIDTH> ckptId;
    Register<1> valid;
    Register<1> ready;
    Register<32> raw_inst1;
    Register<32> PC1;
    Register<32> predictPC1;
    Register<1> valid1;
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
  bool refillHit(uint32_t pc) const {
    return static_cast<bool>(fillFire) &&
           (pc & ~ICACHE_OFFSET_MASK) == static_cast<uint32_t>(lineReturn.lineAddr);
  }
  auto returnRaw() const {
    return headReadReady()
               ? ICacheData.readLane(static_cast<uint32_t>(readLaneIndex))
               : static_cast<uint32_t>(
                     requestBuffer[static_cast<uint32_t>(head)].raw_inst);
  }
  auto returnRaw1() const {
    if (!static_cast<bool>(requestBuffer[static_cast<uint32_t>(head)].valid1))
      return 0u;
    return headReadReady()
               ? ICacheData.readLane(static_cast<uint32_t>(readLaneIndex) + 1)
               : static_cast<uint32_t>(
                     requestBuffer[static_cast<uint32_t>(head)].raw_inst1);
  }
  auto returnPC() const {
    return static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].PC);
  }
  auto returnPC1() const {
    return static_cast<uint32_t>(requestBuffer[static_cast<uint32_t>(head)].PC1);
  }
  auto returnPredictPC() const {
    return static_cast<uint32_t>(
        requestBuffer[static_cast<uint32_t>(head)].predictPC);
  }
  auto returnPredictPC1() const {
    return static_cast<uint32_t>(
        requestBuffer[static_cast<uint32_t>(head)].predictPC1);
  }
  auto returnValid1() const {
    return static_cast<bool>(requestBuffer[static_cast<uint32_t>(head)].valid1)
               ? 1u
               : 0u;
  }
  auto returnCkptId() const {
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
          static_cast<uint32_t>(requestBuffer[slot].PC) &
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
          static_cast<uint32_t>(requestBuffer[slot].PC) &
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
    return legacyHaltEnabled && isReturnReady() &&
           (returnRaw() == 0x0ff00513 ||
            (static_cast<bool>(
                 requestBuffer[static_cast<uint32_t>(head)].valid1) &&
             returnRaw1() == 0x0ff00513));
  }
  void work() override;
};
