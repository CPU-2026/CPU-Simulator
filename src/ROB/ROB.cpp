#include "../include/ROB.hpp"
#include "common.h"
#include <array>
#include <cstdint>

bool ROB::isOlder(RobTag tag_a, RobTag tag_b) {
  if (tag_a >> (ROB_TAG_WIDTH - 1) != tag_b >> (ROB_TAG_WIDTH - 1)) {
    return (tag_a & ROB_INDEX_MASK) > (tag_b & ROB_INDEX_MASK);
  }
  return (tag_a & ROB_INDEX_MASK) < (tag_b & ROB_INDEX_MASK);
}
bool ROB::isYounger(RobTag tag_a, RobTag tag_b) {
  return isOlder(tag_b, tag_a);
}

ROB::ROB() { wire_output(); }

void ROB::wire_output() {
  headView.head = [this]() -> uint32_t {
    return static_cast<uint32_t>(robHead);
  };
  headView.isEmpty = [this]() -> uint32_t {
    return (static_cast<uint32_t>(robHead) == static_cast<uint32_t>(next)) ? 1u
                                                                           : 0u;
  };
  headView.isHeadCommitReady = [this]() -> uint32_t {
    if (static_cast<uint32_t>(robHead) == static_cast<uint32_t>(next))
      return 0u;
    return static_cast<bool>(
               ROBqueue[robSlot(static_cast<uint32_t>(robHead))].isCommitReady)
               ? 1u
               : 0u;
  };
  headView.isHeadHalt = [this]() -> uint32_t {
    if (static_cast<uint32_t>(robHead) == static_cast<uint32_t>(next))
      return 0u;
    return static_cast<bool>(
               ROBqueue[robSlot(static_cast<uint32_t>(robHead))].halt)
               ? 1u
               : 0u;
  };
  headView.headType = [this]() -> uint32_t {
    if (static_cast<uint32_t>(robHead) == static_cast<uint32_t>(next))
      return 0u;
    return static_cast<uint32_t>(
        ROBqueue[robSlot(static_cast<uint32_t>(robHead))].type);
  };
  for (int i = 0; i < ROB_CAP; ++i) {
    entry.tag[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].tag);
    };
    entry.isCommitReady[i] = [this, i]() -> uint32_t {
      return static_cast<bool>(ROBqueue[i].isCommitReady) ? 1u : 0u;
    };
    entry.isCall[i] = [this, i]() -> uint32_t {
      return static_cast<bool>(ROBqueue[i].isCall) ? 1u : 0u;
    };
    entry.isRet[i] = [this, i]() -> uint32_t {
      return static_cast<bool>(ROBqueue[i].isRet) ? 1u : 0u;
    };
    entry.dest[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].dest);
    };
    entry.ckptId[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].ckptId);
    };
    entry.predictedPC[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].predictedPC);
    };
    entry.pc[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].pc);
    };
    entry.lqTailSnapshot[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].lqTailSnapshot);
    };
    entry.sqTailSnapshot[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].sqTailSnapshot);
    };
    entry.newPhy[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].newPhy);
    };
    entry.oldPhy[i] = [this, i]() -> uint32_t {
      return static_cast<uint32_t>(ROBqueue[i].oldPhy);
    };
  }
}

bool ROB::isHaltCommitted() const {
  return static_cast<bool>(robHaltCommitted);
}
uint32_t ROB::getHaltRd() const { return static_cast<uint32_t>(robHaltRd); }
RobTag ROB::getNextTag() const { return static_cast<uint32_t>(next); }
bool ROB::isFull() const {
  const RobTag nextTag = static_cast<uint32_t>(next);
  const RobTag headTag = static_cast<uint32_t>(robHead);
  return robSlot(nextTag) == robSlot(headTag) && nextTag != headTag;
}
bool ROB::isEmpty() const {
  return static_cast<uint32_t>(robHead) == static_cast<uint32_t>(next);
}
bool ROB::matchesTag(RobTag tag) const {
  const auto slot = robSlot(tag);
  return !isEmpty() && slot < ROB_CAP &&
         static_cast<uint32_t>(ROBqueue[slot].tag) == tag;
}
bool ROB::willCommit() const {
  if (isEmpty())
    return false;
  const RobTag headTag = static_cast<uint32_t>(robHead);
  if (!static_cast<bool>(ROBqueue[robSlot(headTag)].isCommitReady))
    return false;
  return !static_cast<bool>(squash.needSquash) ||
         isOlder(headTag, static_cast<uint32_t>(squash.SquashTag));
}
bool ROB::storeWillCommit() const {
  return willCommit() &&
         static_cast<uint32_t>(ROBqueue[robSlot(
             static_cast<uint32_t>(robHead))].type) ==
             static_cast<uint32_t>(ROBType::STORE);
}

void ROB::work() {
  const bool issueValid = static_cast<bool>(issue.issueValid);
  const bool needSquash = static_cast<bool>(squash.needSquash);
  const RobTag squashTag = static_cast<uint32_t>(squash.SquashTag);
  const RobTag curHead = static_cast<uint32_t>(robHead);
  const RobTag curNext = static_cast<uint32_t>(next);
  const bool curEmpty = curHead == curNext;
  const bool willCommitNow = willCommit();
  const uint32_t issueSlot = robSlot(curNext);

  std::array<bool, ROB_CAP> readyWrite{};
  std::array<bool, ROB_CAP> readyData{};

  // The issue path must gate on fullness: a push into a full ROB silently
  // overwrites the head entry.
  dark::debug::assert(!(issueValid && isFull()),
                      "ROB push into a full ROB overwrites the head entry");
  if (issueValid) {
    const uint32_t q = issueSlot;
    ROBqueue[q].tag <= curNext;
    ROBqueue[q].type <= static_cast<uint32_t>(issue.entry.type);
    readyWrite[q] = true;
    readyData[q] = static_cast<bool>(issue.entry.isCommitReady);
    ROBqueue[q].dest <= static_cast<uint32_t>(issue.entry.dest);
    ROBqueue[q].halt <= static_cast<uint32_t>(issue.entry.halt);
    ROBqueue[q].isCall <= static_cast<uint32_t>(issue.entry.isCall);
    ROBqueue[q].isRet <= static_cast<uint32_t>(issue.entry.isRet);
    ROBqueue[q].ckptId <= static_cast<uint32_t>(issue.entry.ckptId);
    ROBqueue[q].predictedPC <= static_cast<uint32_t>(issue.entry.predictedPC);
    ROBqueue[q].pc <= static_cast<uint32_t>(issue.entry.pc);
    ROBqueue[q].lqTailSnapshot <=
        static_cast<uint32_t>(issue.entry.lqTailSnapshot);
    ROBqueue[q].sqTailSnapshot <=
        static_cast<uint32_t>(issue.entry.sqTailSnapshot);
    ROBqueue[q].newPhy <= static_cast<uint32_t>(issue.entry.newPhy);
    ROBqueue[q].oldPhy <= static_cast<uint32_t>(issue.entry.oldPhy);
  }

  // Main-tree push happens before ready writes. Authenticate against the row
  // tag after that possible push, then collapse all ready sources to one port.
  auto matchesEffectiveTag = [&](RobTag tag) {
    const uint32_t slot = robSlot(tag);
    if (slot >= ROB_CAP || (curEmpty && !issueValid))
      return false;
    const RobTag rowTag =
        issueValid && slot == issueSlot
            ? curNext
            : static_cast<uint32_t>(ROBqueue[slot].tag);
    return rowTag == tag;
  };
  auto markReady = [&](RobTag tag) {
    if ((!needSquash || ROB::isOlder(tag, squashTag)) &&
        matchesEffectiveTag(tag)) {
      const uint32_t slot = robSlot(tag);
      readyWrite[slot] = true;
      readyData[slot] = true;
    }
  };

  if (!static_cast<bool>(bru.isBRUEmpty)) {
    markReady(static_cast<uint32_t>(bru.bruHeadRobTag));
  }
  {
    uint32_t sqHead = static_cast<uint32_t>(sq.sqHead);
    for (int k = 0; k < MEMQ_SCAN_WINDOW; ++k) {
      uint32_t i = (sqHead + k) & SQ_MASK;
      if (!static_cast<bool>(sq.sqValid[i]))
        continue;
      if (!static_cast<bool>(sq.sqReadyToCommit[i]))
        continue;
      if (static_cast<bool>(sq.sqCommitted[i]))
        continue;
      RobTag sqTag = static_cast<uint32_t>(sq.sqRobTag[i]);
      markReady(sqTag);
    }
  }
  if (static_cast<bool>(cdbOfALU.cdbValid)) {
    markReady(static_cast<uint32_t>(cdbOfALU.cdbRobTag));
  }
  if (static_cast<bool>(cdbOfLQ.cdbValid)) {
    markReady(static_cast<uint32_t>(cdbOfLQ.cdbRobTag));
  }
  if (static_cast<bool>(cdbOfMUL.cdbValid)) {
    markReady(static_cast<uint32_t>(cdbOfMUL.cdbRobTag));
  }
  if (static_cast<bool>(cdbOfDIV.cdbValid)) {
    markReady(static_cast<uint32_t>(cdbOfDIV.cdbRobTag));
  }
  for (int i = 0; i < ROB_CAP; ++i) {
    if (readyWrite[i])
      ROBqueue[i].isCommitReady <= readyData[i];
  }

  bool nextWrite = issueValid;
  RobTag nextData = robNextTag(curNext);
  if (needSquash && matchesEffectiveTag(squashTag)) {
    nextWrite = true;
    nextData = robNextTag(squashTag);
  }
  if (nextWrite)
    next <= nextData;

  // A strictly older head commit coexists with a younger squash.
  if (willCommitNow) {
    const uint32_t oldIdx = robSlot(curHead);
    const bool oldHalt = static_cast<bool>(ROBqueue[oldIdx].halt);
    const uint32_t oldDest = static_cast<uint32_t>(ROBqueue[oldIdx].dest);
    robHead <= robNextTag(curHead);
    if (oldHalt) {
      robHaltCommitted <= true;
      robHaltRd <= oldDest;
    }
  }
}
