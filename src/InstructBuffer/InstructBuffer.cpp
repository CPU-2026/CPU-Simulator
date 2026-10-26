#include "../include/InstructBuffer.hpp"

void InstructBuffer::work() {
  if (needSquash) {
    head <= 0;
    tail <= 0;
    lastCount <= 0;
  } else {
    const unsigned pop = static_cast<uint32_t>(decodePopCount);
    dark::debug::assert(pop <= DECODE_WIDTH && pop <= count(), "FQ decode pop exceeds old entries");
    if (static_cast<bool>(icacheReturnAccepted)) {
      const unsigned cnt = static_cast<uint32_t>(icacheReturnCount);
      dark::debug::assert(cnt >= 1 && cnt <= FETCH_WIDTH, "FQ push count out of range");
      const uint32_t t0 = static_cast<uint32_t>(tail) & (FQ_CAP - 1);
      for (unsigned L = 0; L < cnt; ++L) {
        const uint32_t t = (t0 + L) & (FQ_CAP - 1);
        entries[t].raw <= static_cast<uint32_t>(icacheReturnRaw[L]);
        entries[t].pc <= static_cast<uint32_t>(icacheReturnPC[L]);
        entries[t].predictedPC <= static_cast<uint32_t>(icacheReturnPredictedPC[L]);
        entries[t].ckptId <= ((static_cast<uint32_t>(icacheReturnCkptId) + L) & (CKPT_CAP - 1));
      }
      tail <= ((static_cast<uint32_t>(tail) + cnt) & FQ_SEQ_MASK);
      lastCount <= cnt;
      for (unsigned L = 0; L < FETCH_WIDTH; ++L) {
        lastRaw[L] <= ((L < cnt) ? static_cast<uint32_t>(icacheReturnRaw[L]) : 0u);
        lastPC[L] <= ((L < cnt) ? static_cast<uint32_t>(icacheReturnPC[L]) : 0u);
      }
    } else {
      lastCount <= 0;
    }
    if (pop > 0) {
      head <= ((static_cast<uint32_t>(head) + pop) & FQ_SEQ_MASK);
    }
  }
}
