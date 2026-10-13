#include "../include/DMEM.hpp"
#include "common.h"
#include <cstdint>

void DMEM::writeLine(uint32_t addr, const uint8_t *lineData) {
  for (int i = 0; i < DCACHE_BLOCK_CAP; i++) {
    write_data(addr + i, lineData[i]);
  }
}

void DMEM::work() {
  // Every read of the registered state is the committed (_M_old) view, so the
  // two ports are order-independent by construction.
  const bool readBusyOld = static_cast<bool>(readBusy);
  const bool writeBusyOld = static_cast<bool>(writeBusy);
  const bool replyValidOld = static_cast<bool>(replyValid);

  // ---- claim: one pulse per port, only when that port is drained ----
  // A held reply blocks a new read so the fill identity cannot be lost.
  if (static_cast<bool>(readValid) && !readBusyOld && !replyValidOld) {
    execReadAddress <= static_cast<uint32_t>(readAddress);
    execReadRemainCycle <= MEM_LATENCY;
    readBusy <= true;
  }
  if (static_cast<bool>(writeValid) && !writeBusyOld) {
    execWriteAddress <= static_cast<uint32_t>(writeAddress);
    execWriteRemainCycle <= MEM_LATENCY;
    for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
      execWriteLineData[i] <= static_cast<uint32_t>(writeLineData[i]);
    writeBusy <= true;
  }

  // ---- completion: a read produces a reply, a write lands its line ----
  const bool readDone = readBusyOld &&
                        static_cast<uint32_t>(execReadRemainCycle) == 1;
  const bool writeDone = writeBusyOld &&
                         static_cast<uint32_t>(execWriteRemainCycle) == 1;

  if (readDone) {
    const uint32_t blockBase =
        static_cast<uint32_t>(execReadAddress) & ~DCACHE_OFFSET_MASK;
    for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
      replyLineData[i] <= read_data(blockBase + i);
    readBusy <= false;
  } else if (readBusyOld) {
    // countdown: hold the port busy (Register keeps its value unassigned)
    execReadRemainCycle <= static_cast<uint32_t>(execReadRemainCycle) - 1;
  }

  if (writeDone) {
    uint8_t line[DCACHE_BLOCK_CAP];
    for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
      line[i] = static_cast<uint8_t>(static_cast<uint32_t>(execWriteLineData[i]));
    writeLine(static_cast<uint32_t>(execWriteAddress), line);
    writeBusy <= false;
  } else if (writeBusyOld) {
    execWriteRemainCycle <= static_cast<uint32_t>(execWriteRemainCycle) - 1;
  }

  // ---- reply hold: keep until the DCache fill consumes it ----
  // (readDone wins over the pull: a freshly produced reply is never pulled in
  // the same cycle it appears -- that is the main tree's write order too.)
  if (readDone) {
    replyValid <= true;
  } else if (replyValidOld && static_cast<bool>(replyConsumed)) {
    replyValid <= false;
  }
}
