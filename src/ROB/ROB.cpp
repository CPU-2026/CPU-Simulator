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
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
    retirement[lane].valid = [this, lane]() { return lane < commitCount(); };
    retirement[lane].tag = [this, lane]() { return commitTag(lane); };
    retirement[lane].type = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].type); };
    retirement[lane].rd = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].dest); };
    retirement[lane].newPhy = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].newPhy); };
    retirement[lane].oldPhy = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].oldPhy); };
    retirement[lane].pc = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].pc); };
    retirement[lane].halt = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].halt); };
    retirement[lane].exception = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].exception); };
    retirement[lane].pop = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].isRet); };
    retirement[lane].push = [this, lane]() { return uint32_t(ROBqueue[robSlot(commitTag(lane))].isCall); };
    retirement[lane].exits = [this, lane]() { return uint32_t(commitExit[lane]); };
  }
  for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) {
    replayNarrow[lane].tag = [this, lane]() { return static_cast<uint32_t>(ROBqueue[replaySlot(lane)].tag); };
    replayNarrow[lane].rd = [this, lane]() { return static_cast<uint32_t>(ROBqueue[replaySlot(lane)].dest); };
    replayNarrow[lane].newPhy = [this, lane]() { return static_cast<uint32_t>(ROBqueue[replaySlot(lane)].newPhy); };
    replayRas[lane].tag = [this, lane]() { return static_cast<uint32_t>(replayNarrow[lane].tag); };
    replayRas[lane].pc = [this, lane]() { return static_cast<uint32_t>(ROBqueue[replaySlot(lane)].pc); };
    replayRas[lane].pop = [this, lane]() { return static_cast<uint32_t>(ROBqueue[replaySlot(lane)].isRet); };
    replayRas[lane].push = [this, lane]() { return static_cast<uint32_t>(ROBqueue[replaySlot(lane)].isCall); };
  }
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

unsigned ROB::replaySlot(unsigned lane) const {
  unsigned slot = robSlot(static_cast<uint32_t>(robHead)) + static_cast<uint32_t>(replayOffset) + lane;
  if (slot >= ROB_CAP) slot -= ROB_CAP;
  return slot;
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
unsigned ROB::count() const {
  return (static_cast<uint32_t>(next) - static_cast<uint32_t>(robHead)) &
         ROB_TAG_MASK;
}
bool ROB::canAllocate(unsigned n) const {
  return count() + n <= static_cast<unsigned>(ROB_CAP);
}
bool ROB::matchesTag(RobTag tag) const {
  const auto slot = robSlot(tag);
  const uint32_t headTag = static_cast<uint32_t>(robHead);
  const uint32_t span =
      (static_cast<uint32_t>(next) - headTag) & ROB_TAG_MASK;
  const uint32_t offset = (uint32_t(tag) - headTag) & ROB_TAG_MASK;
  return slot < ROB_CAP && offset < span &&
         static_cast<uint32_t>(ROBqueue[slot].tag) == tag;
}
bool ROB::willCommit() const {
  if (static_cast<bool>(retirementBlocked)) return false;
  if (isEmpty())
    return false;
  const RobTag headTag = static_cast<uint32_t>(robHead);
  if (!static_cast<bool>(ROBqueue[robSlot(headTag)].isCommitReady))
    return false;
  return !static_cast<bool>(squash.needSquash) ||
         isOlder(headTag, static_cast<uint32_t>(squash.SquashTag));
}
RobTag ROB::commitTag(unsigned lane) const {
  RobTag tag = uint32_t(robHead);
  for (unsigned i = 0; i < COMMIT_WIDTH; ++i)
    if (i < lane) tag = robNextTag(tag);
  return tag;
}

unsigned ROB::commitCount() const {
  bool open = !bool(retirementBlocked) && !bool(squash.needSquash) &&
      !bool(robHaltCommitted) && !bool(robTrapCommitted);
  unsigned count = 0;
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
    const RobTag tag = commitTag(lane);
    const auto &row = ROBqueue[robSlot(tag)];
    if (open && matchesTag(tag) && bool(row.isCommitReady) &&
        (uint32_t(row.type) != uint32_t(ROBType::STORE) || bool(commitStoreReady[lane]))) {
      ++count;
      open = !bool(row.halt) && !bool(row.exception) && !bool(commitExit[lane]);
    } else open = false;
  }
  return count;
}
bool ROB::storeWillCommit() const {
  return willCommit() &&
         static_cast<uint32_t>(ROBqueue[robSlot(
             static_cast<uint32_t>(robHead))].type) ==
             static_cast<uint32_t>(ROBType::STORE);
}

void ROB::work() {
  bool issueValid[ISSUE_WIDTH];
  RobTag issueTag[ISSUE_WIDTH];
  uint32_t issueSlotIdx[ISSUE_WIDTH];
  {
    RobTag t = static_cast<uint32_t>(next);
    for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
      issueValid[L] = static_cast<bool>(issue[L].issueValid);
      issueTag[L] = t;
      issueSlotIdx[L] = robSlot(t);
      t = robNextTag(t);
    }
  }
  bool anyIssue = false;
  for (unsigned L = 0; L < ISSUE_WIDTH; ++L) anyIssue |= issueValid[L];
  const bool needSquash = static_cast<bool>(squash.needSquash);
  const RobTag squashTag = static_cast<uint32_t>(squash.SquashTag);
  const RobTag curHead = static_cast<uint32_t>(robHead);
  const RobTag curNext = static_cast<uint32_t>(next);
  const bool curEmpty = curHead == curNext;
  const unsigned retiring = commitCount();

  std::array<bool, ROB_CAP> readyWrite{};
  std::array<bool, ROB_CAP> readyData{};

  // The issue path must gate on fullness: a push into a full ROB silently
  // overwrites the head entry.
  dark::debug::assert(!(issueValid[0] && isFull()),
                      "ROB push into a full ROB overwrites the head entry");
  for (unsigned L = 0; L < ISSUE_WIDTH; ++L) {
    if (issueValid[L]) {
      dark::debug::assert(canAllocate(L + 1u), "ROB issue exceeds old free capacity");
      dark::debug::assert(L == 0 || issueValid[L - 1u], "ROB issue lanes must form a prefix");
      const uint32_t q = issueSlotIdx[L];
      ROBqueue[q].tag <= issueTag[L];
      ROBqueue[q].type <= static_cast<uint32_t>(issue[L].entry.type);
      readyWrite[q] = true;
      readyData[q] = static_cast<bool>(issue[L].entry.isCommitReady);
      ROBqueue[q].dest <= static_cast<uint32_t>(issue[L].entry.dest);
      ROBqueue[q].halt <= static_cast<uint32_t>(issue[L].entry.halt);
      ROBqueue[q].exception <= static_cast<uint32_t>(issue[L].entry.exception);
      ROBqueue[q].isCall <= static_cast<uint32_t>(issue[L].entry.isCall);
      ROBqueue[q].isRet <= static_cast<uint32_t>(issue[L].entry.isRet);
      ROBqueue[q].ckptId <= static_cast<uint32_t>(issue[L].entry.ckptId);
      ROBqueue[q].predictedPC <= static_cast<uint32_t>(issue[L].entry.predictedPC);
      ROBqueue[q].pc <= static_cast<uint32_t>(issue[L].entry.pc);
      ROBqueue[q].lqTailSnapshot <=
          static_cast<uint32_t>(issue[L].entry.lqTailSnapshot);
      ROBqueue[q].sqTailSnapshot <=
          static_cast<uint32_t>(issue[L].entry.sqTailSnapshot);
      ROBqueue[q].newPhy <= static_cast<uint32_t>(issue[L].entry.newPhy);
      ROBqueue[q].oldPhy <= static_cast<uint32_t>(issue[L].entry.oldPhy);
    }
  }

  // Main-tree push happens before ready writes. Authenticate against the row
  // tag after that possible push, then collapse all ready sources to one port.
  auto matchesEffectiveTag = [&](RobTag tag) {
    const uint32_t slot = robSlot(tag);
    if (slot >= ROB_CAP || (curEmpty && !anyIssue))
      return false;
    RobTag rowTag = static_cast<uint32_t>(ROBqueue[slot].tag);
    for (unsigned L = 0; L < ISSUE_WIDTH; ++L)
      if (issueValid[L] && slot == issueSlotIdx[L])
        rowTag = issueTag[L];
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
  // SQ owns the old-state slot/epoch decode. ROB validates its own row before
  // merging Store completion with the other ready sources.
  const uint32_t storeReadyMask = static_cast<uint32_t>(sq.readyMask);
  const uint32_t storeEpochMask = static_cast<uint32_t>(sq.epochMask);
  for (int slot = 0; slot < ROB_CAP; ++slot) {
    const uint32_t oneHot = uint32_t{1} << slot;
    const RobTag tag = static_cast<uint32_t>(ROBqueue[slot].tag);
    if ((storeReadyMask & oneHot) && matchesTag(tag) &&
        static_cast<uint32_t>(ROBqueue[slot].type) == static_cast<uint32_t>(ROBType::STORE) &&
        bool(storeEpochMask & oneHot) == bool(tag >> (ROB_TAG_WIDTH - 1)))
      markReady(tag);
  }
  if (static_cast<bool>(cdbOfALU.cdbValid)) {
    markReady(static_cast<uint32_t>(cdbOfALU.cdbRobTag));
  }
  if (static_cast<bool>(cdbOfShared.cdbValid)) {
    markReady(static_cast<uint32_t>(cdbOfShared.cdbRobTag));
  }
  for (int i = 0; i < ROB_CAP; ++i) {
    if (readyWrite[i])
      ROBqueue[i].isCommitReady <= readyData[i];
  }

  unsigned issueCount = 0;
  for (unsigned L = 0; L < ISSUE_WIDTH; ++L)
    if (issueValid[L]) ++issueCount;
  RobTag nextData = curNext;
  for (unsigned L = 0; L < issueCount; ++L) nextData = robNextTag(nextData);
  bool nextWrite = issueCount > 0;
  if (needSquash && matchesEffectiveTag(squashTag)) {
    nextWrite = true;
    nextData = robNextTag(squashTag);
  }
  if (nextWrite)
    next <= nextData;

  bool trap = false, halt = false;
  uint32_t haltRd = 0;
  RobTag retiredHead = curHead;
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
    if (lane < retiring) {
      const auto &row = ROBqueue[robSlot(retiredHead)];
      trap |= bool(row.exception);
      if (bool(row.halt)) { halt = true; haltRd = uint32_t(row.dest); }
      retiredHead = robNextTag(retiredHead);
    }
  }
  if (retiring != 0) robHead <= retiredHead;
  if (trap) robTrapCommitted <= 1;
  if (halt) { robHaltCommitted <= 1; robHaltRd <= haltRd; }
}
