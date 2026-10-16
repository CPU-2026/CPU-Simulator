#include "../include/AXIFetch.hpp"
#include <cstdint>

AXIFetch::AXIFetch() {
  // An asserted AR beat is never withdrawn: squash keeps the oldest
  // unscheduled beat valid (and captures it as the pending one) until the
  // handshake completes.
  arValid = [this]() -> uint32_t {
    uint32_t slot = 0;
    return (static_cast<bool>(pending) || oldestUnscheduled(slot)) ? 1u : 0u;
  };
  arAddr = [this]() -> uint32_t {
    if (static_cast<bool>(pending))
      return static_cast<uint32_t>(pendingAddr);
    uint32_t slot = static_cast<uint32_t>(head);
    oldestUnscheduled(slot);
    return static_cast<uint32_t>(lines[slot].lineAddr) +
           (static_cast<uint32_t>(lines[slot].issued) << RV32_WORD_BYTE_BITS);
  };
  // Every fired read beat returns exactly one R beat; the adapter can always
  // take it (collect or drop), so the read data channel never stalls.
  rReady = [this]() -> uint32_t { return 1u; };
}

uint32_t AXIFetch::occupancy() const {
  uint32_t n = 0;
  for (int i = 0; i < IMEM_CAP; ++i)
    n += static_cast<bool>(lines[i].valid) ? 1u : 0u;
  return n;
}

bool AXIFetch::oldestUnscheduled(uint32_t &slot) const {
  const uint32_t occ = occupancy();
  const uint32_t h = static_cast<uint32_t>(head);
  for (uint32_t age = 0; age < occ; ++age) {
    const uint32_t s = (h + age) & (IMEM_CAP - 1);
    if (static_cast<bool>(lines[s].valid) &&
        static_cast<uint32_t>(lines[s].issued) < ICACHE_WORDS_PER_LINE) {
      slot = s;
      return true;
    }
  }
  return false;
}

bool AXIFetch::oldestWithDeficit(uint32_t &slot) const {
  const uint32_t occ = occupancy();
  const uint32_t h = static_cast<uint32_t>(head);
  for (uint32_t age = 0; age < occ; ++age) {
    const uint32_t s = (h + age) & (IMEM_CAP - 1);
    if (static_cast<bool>(lines[s].valid) &&
        static_cast<uint32_t>(lines[s].collected) <
            static_cast<uint32_t>(lines[s].issued)) {
      slot = s;
      return true;
    }
  }
  return false;
}

bool AXIFetch::retValid() const {
  const uint32_t h = static_cast<uint32_t>(head);
  return static_cast<bool>(lines[h].valid) && static_cast<bool>(lines[h].ready);
}

uint32_t AXIFetch::retLineAddr() const {
  return static_cast<uint32_t>(lines[static_cast<uint32_t>(head)].lineAddr);
}

uint32_t AXIFetch::retWord(int w) const {
  return static_cast<uint32_t>(lines[static_cast<uint32_t>(head)].words[w]);
}

void AXIFetch::work() {
  const uint32_t occ = occupancy();
  const uint32_t h = static_cast<uint32_t>(head);
  const uint32_t infl = static_cast<uint32_t>(inflight);
  const uint32_t disc = static_cast<uint32_t>(discard);

  const bool rBeat = static_cast<bool>(rValid) && static_cast<bool>(rReady);
  const bool arFire = static_cast<bool>(arValid) && static_cast<bool>(arReady);
  const bool doPop = static_cast<bool>(lineConsumed);
  const uint32_t nextInflight = infl + (arFire ? 1u : 0u) - (rBeat ? 1u : 0u);
  uint32_t nextDiscard = disc;

  if (needSquash) {
    // Speculative lines are dropped; beats already on the bus are counted as
    // orphans and drained through the discard counter. An asserted AR beat
    // that has not been accepted yet stays on the bus until its handshake.
    for (int i = 0; i < IMEM_CAP; ++i)
      lines[i].valid <= 0;
    head <= 0;
    if (static_cast<bool>(pending)) {
      if (arFire)
        pending <= 0;
    } else if (!arFire) {
      uint32_t slot = 0;
      if (oldestUnscheduled(slot)) {
        pending <= 1;
        pendingAddr <= static_cast<uint32_t>(lines[slot].lineAddr) +
                       (static_cast<uint32_t>(lines[slot].issued)
                        << RV32_WORD_BYTE_BITS);
      }
    }
    nextDiscard = nextInflight;
  } else {
    // Collect one read beat into the oldest line still missing words; beats
    // owed to squashed lines are dropped first (they return before new ones).
    if (rBeat) {
      uint32_t slot = 0;
      if (disc == 0 && oldestWithDeficit(slot)) {
        const uint32_t c = static_cast<uint32_t>(lines[slot].collected);
        lines[slot].words[c] <= static_cast<uint32_t>(rData);
        lines[slot].collected <= c + 1u;
        if (c + 1u == ICACHE_WORDS_PER_LINE)
          lines[slot].ready <= 1;
      } else if (disc != 0) {
        nextDiscard = disc - 1u;
      }
    }
    // Issue one read beat for the oldest line whose burst is not complete;
    // a latched squashed beat is carried out first and joins the orphans.
    if (arFire) {
      if (static_cast<bool>(pending)) {
        pending <= 0;
        nextDiscard = nextDiscard + 1u;
      } else {
        uint32_t slot = 0;
        if (oldestUnscheduled(slot))
          lines[slot].issued <= static_cast<uint32_t>(lines[slot].issued) + 1u;
      }
    }
    // Pop the consumed head line and admit a new fetch line in its wake.
    const bool doPush = static_cast<bool>(fetchValid) && !static_cast<bool>(needSquash) &&
                        (occ < static_cast<uint32_t>(IMEM_CAP) || doPop);
    const uint32_t pushSlot = (h + occ) & (IMEM_CAP - 1);
    const bool reuseHead = doPop && doPush && pushSlot == h;
    if (doPop && !reuseHead)
      lines[h].valid <= 0;
    if (doPush) {
      lines[pushSlot].valid <= 1;
      lines[pushSlot].lineAddr <= static_cast<uint32_t>(fetchPC);
      lines[pushSlot].issued <= 0;
      lines[pushSlot].collected <= 0;
      lines[pushSlot].ready <= 0;
    }
    if (doPop)
      head <= ((h + 1u) & (IMEM_CAP - 1));
  }
  discard <= nextDiscard;
  inflight <= nextInflight;
}
