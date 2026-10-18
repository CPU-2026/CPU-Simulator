#pragma once
#include "common.h"
#include "module.h"
#include "tools.h"
#include <array>
#include <cstdint>

// Instruction-side AXI4-Lite adapter. Keeps IMEM's upstream contract -- a
// queue of IMEM_CAP fetch lines, line return valid/addr/word taps for the
// ICache -- but each line is fetched as ICACHE_WORDS_PER_LINE single-beat
// reads on the shared AXI bus. Beats are issued oldest line first, so the
// returned words stay in order and the head line becomes the return source.
struct AXIFetchInput {
  Wire<1> needSquash;
  Wire<1> fetchValid;
  Wire<32> fetchPC;
  Wire<1> lineConsumed;
  Wire<1> arReady; // this master won the AR beat
  Wire<1> rValid;
  Wire<32> rData;
};
struct AXIFetchOutput {
  Wire<1> arValid;
  Wire<32> arAddr;
  Wire<1> rReady;
};
struct AXIFetchLine {
  Register<1> valid;
  Register<32> lineAddr;
  Register<5> issued;    // read beats handed to the bus
  Register<5> collected; // read beats received
};
struct AXIFetchInner {
  std::array<AXIFetchLine, IMEM_CAP> lines;
  std::array<std::array<Register<RV32_WORD_BITS>, ICACHE_WORDS_PER_LINE>, IMEM_CAP> payload;
  Register<std::bit_width(static_cast<uint32_t>(IMEM_CAP - 1))> head;
  Register<std::bit_width(static_cast<uint32_t>(IMEM_CAP))> count, tail, issue, collect;
  Register<9> inflight; // issued beats not yet received
  Register<9> discard;  // squashed beats still to drop
  // An AR beat asserted but not yet accepted when squash hits: it must stay
  // on the bus until handshake (VALID cannot be withdrawn) and its data is
  // then dropped like any other squashed beat.
  Register<1> pending;
  Register<32> pendingAddr;
};
struct AXIFetch : dark::Module<AXIFetchInput, AXIFetchOutput, AXIFetchInner> {
  AXIFetch();
  uint32_t occupancy() const;
  bool isRequestFull() const { return occupancy() == IMEM_CAP; }
  bool retValid() const;
  uint32_t retLineAddr() const;
  uint32_t retWord(int w) const;
  void work() override;

 private:
  bool oldestWithDeficit(uint32_t &slot) const;
  bool oldestUnscheduled(uint32_t &slot) const;
};
