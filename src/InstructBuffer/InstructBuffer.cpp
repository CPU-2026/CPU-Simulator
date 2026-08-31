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
  } else {
    // stage 2 push: latch an ICache line return unless halted or full
    if (static_cast<bool>(icacheReturnReady) &&
        !static_cast<bool>(haltFetched) && !isFull()) {
      auto t = static_cast<uint32_t>(tail);
      entries[t].raw <= static_cast<uint32_t>(icacheReturnRaw);
      entries[t].pc <= static_cast<uint32_t>(icacheReturnPC);
      entries[t].predictedPC <=
          static_cast<uint32_t>(icacheReturnPredictedPC);
      entries[t].ckptId <= static_cast<uint32_t>(icacheReturnCkptId);
      tail <= ((t + 1) & (FQ_CAP - 1));
      lastValid <= 1;
      lastRaw <= static_cast<uint32_t>(icacheReturnRaw);
      lastPC <= static_cast<uint32_t>(icacheReturnPC);
    } else {
      lastValid <= 0;
    }
    // stage 1 pop: hand the head entry to the decoder when it can accept it
    if (!isEmpty() && !static_cast<bool>(decodeFull)) {
      head <= ((static_cast<uint32_t>(head) + 1) & (FQ_CAP - 1));
    }
  }
}
