#include "../include/FetchUnit.hpp"
#include <cstdint>

void FetchUnit::work() {
  // stage 1 flush first: on squash restore the PC and clear the halt flag
  if (needSquash) {
    programCounter <= static_cast<uint32_t>(SquashPC);
    haltFetched <= 0;
  } else {
    // stage 2 halt signal: latch when the ICache head holds the halt instruction
    if (haltSignal) {
      haltFetched <= 1;
    }
    // stage 3 normal advance: advance the PC when fetchDecision is valid
    if (FetchValid) {
      programCounter <= static_cast<uint32_t>(PredictPC);
    }
  }
}
