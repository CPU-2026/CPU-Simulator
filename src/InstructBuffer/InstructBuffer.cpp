#include "../include/InstructBuffer.hpp"

void InstructBuffer::work() {
  // stage 3 flush first: on squash drop the whole queue (head==tail makes
  // stale entries unreachable, no need to clear them). lastValid must be
  // cleared too: main-tree semantics clear pushCache before the squash check,
  // so a squash cycle must not leak a stale scan event into the next cycle.
  if (needSquash) {
    head <= 0;
    tail <= 0;
    lastValid <= 0;
    lastValid1 <= 0;
  } else {
    const unsigned pop = static_cast<uint32_t>(decodePopCount);
    dark::debug::assert(pop <= DECODE_WIDTH && pop <= count(), "FQ decode pop exceeds old entries");
    const bool push1 = FETCH_WIDTH >= 2 &&
                       static_cast<bool>(icacheReturnValid1);
    // stage 2 push: exactly the ICache head-consumption handshake (one or two
    // instructions from the fetched group).
    if (static_cast<bool>(icacheReturnAccepted)) {
      const uint32_t t0 = static_cast<uint32_t>(tail) & (FQ_CAP - 1);
      const uint32_t t1 = (t0 + 1) & (FQ_CAP - 1);
      entries[t0].raw <= static_cast<uint32_t>(icacheReturnRaw);
      entries[t0].pc <= static_cast<uint32_t>(icacheReturnPC);
      entries[t0].predictedPC <=
          static_cast<uint32_t>(icacheReturnPredictedPC);
      entries[t0].ckptId <= static_cast<uint32_t>(icacheReturnCkptId);
      if (push1) {
        entries[t1].raw <= static_cast<uint32_t>(icacheReturnRaw1);
        entries[t1].pc <= static_cast<uint32_t>(icacheReturnPC1);
        entries[t1].predictedPC <=
            static_cast<uint32_t>(icacheReturnPredictedPC1);
        entries[t1].ckptId <= ((uint32_t(icacheReturnCkptId) + 1) & (CKPT_CAP - 1));
      }
      tail <= ((static_cast<uint32_t>(tail) + (push1 ? 2u : 1u)) & FQ_SEQ_MASK);
      lastValid <= 1;
      lastRaw <= static_cast<uint32_t>(icacheReturnRaw);
      lastPC <= static_cast<uint32_t>(icacheReturnPC);
      lastValid1 <= (push1 ? 1u : 0u);
      lastRaw1 <= static_cast<uint32_t>(icacheReturnRaw1);
      lastPC1 <= static_cast<uint32_t>(icacheReturnPC1);
    } else {
      lastValid <= 0;
      lastValid1 <= 0;
    }
    // stage 1 pop: hand the head entries to the decoder.
    if (pop > 0) {
      head <= ((static_cast<uint32_t>(head) + pop) & FQ_SEQ_MASK);
    }
  }
}
