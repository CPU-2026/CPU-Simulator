#pragma once
#include "common.h"
#include "tools.h"
#include <array>
#include <cstdint>

static_assert(FQ_CAP == (1 << FQ_PTR_WIDTH));   // head/tail carrier is exact
static_assert(CKPT_CAP == (1 << CKPT_ID_WIDTH)); // ckptId carrier is exact

struct InstructBufferEntry {
  Register<32> raw;
  Register<32> pc;
  Register<32> predictedPC;
  Register<CKPT_ID_WIDTH> ckptId;
};

struct FQInput {
  Wire<1> needSquash;
  Wire<1> icacheReturnAccepted;
  Wire<3> icacheReturnCount;
  std::array<Wire<32>, FETCH_WIDTH> icacheReturnRaw;
  std::array<Wire<32>, FETCH_WIDTH> icacheReturnPC;
  std::array<Wire<32>, FETCH_WIDTH> icacheReturnPredictedPC;
  Wire<CKPT_ID_WIDTH> icacheReturnCkptId;
  Wire<DECODE_COUNT_WIDTH> decodePopCount;
};

struct FQOutput {
  Register<FQ_SEQ_WIDTH> head;
  Register<FQ_SEQ_WIDTH> tail;
  Register<3> lastCount;
  std::array<Register<32>, FETCH_WIDTH> lastRaw;
  std::array<Register<32>, FETCH_WIDTH> lastPC;
};

struct FQInner {
  std::array<InstructBufferEntry, FQ_CAP> entries;
};

struct InstructBuffer : dark::Module<FQInput, FQOutput, FQInner> {
  void work() override;

  unsigned count() const {
    return (static_cast<uint32_t>(tail) - static_cast<uint32_t>(head)) &
           FQ_SEQ_MASK;
  }
  // bridge accessors: combinational reads of the committed (_M_old) state
  // (renamed from FetchQueue; member aliases FQModule/FQ* kept for callers)
  bool isFull() const { return count() == static_cast<unsigned>(FQ_CAP); }
  bool isEmpty() const { return count() == 0u; }
  const InstructBufferEntry &entryAt(unsigned offset) const {
    return entries[(static_cast<uint32_t>(head) + offset) & (FQ_CAP - 1)];
  }
  // Capacity to absorb `push` new entries after `pop` this-cycle removals.
  bool canAccept(unsigned push, unsigned pop) const {
    return count() + push <= static_cast<unsigned>(FQ_CAP) + pop;
  }
  uint32_t getLastCount() const {
    return static_cast<uint32_t>(lastCount);
  }
  uint32_t getLastRaw(unsigned L) const {
    return static_cast<uint32_t>(lastRaw[L]);
  }
  uint32_t getLastPC(unsigned L) const {
    return static_cast<uint32_t>(lastPC[L]);
  }
  uint32_t headRaw() const {
    return static_cast<uint32_t>(entries[static_cast<uint32_t>(head) & (FQ_CAP - 1)].raw);
  }
  uint32_t headRaw1() const {
    return static_cast<uint32_t>(
        entries[(static_cast<uint32_t>(head) + 1) & (FQ_CAP - 1)].raw);
  }
  int headpc() const {
    return static_cast<int>(
        static_cast<uint32_t>(entries[static_cast<uint32_t>(head) & (FQ_CAP - 1)].pc));
  }
  int headpc1() const {
    return static_cast<int>(static_cast<uint32_t>(
        entries[(static_cast<uint32_t>(head) + 1) & (FQ_CAP - 1)].pc));
  }
  int32_t headPredictedPC() const {
    return static_cast<int32_t>(static_cast<uint32_t>(
        entries[static_cast<uint32_t>(head) & (FQ_CAP - 1)].predictedPC));
  }
  int32_t headPredictedPC1() const {
    return static_cast<int32_t>(static_cast<uint32_t>(
        entries[(static_cast<uint32_t>(head) + 1) & (FQ_CAP - 1)].predictedPC));
  }
  uint8_t headCkptId() const {
    return static_cast<uint8_t>(static_cast<uint32_t>(
        entries[static_cast<uint32_t>(head) & (FQ_CAP - 1)].ckptId));
  }
  uint8_t headCkptId1() const {
    return static_cast<uint8_t>(static_cast<uint32_t>(
        entries[(static_cast<uint32_t>(head) + 1) & (FQ_CAP - 1)].ckptId));
  }
};
