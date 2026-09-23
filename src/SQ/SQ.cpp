#include "../include/SQ.hpp"
#include "../include/ROB.hpp"
#include <cstdint>
#include <stdexcept>
#include <sys/types.h>

bool SQ::isEmpty() const {
  return static_cast<uint32_t>(tail) == static_cast<uint32_t>(head);
}

bool SQ::isFull() const {
  return ((static_cast<uint32_t>(tail) + 1) & SQ_MASK) ==
         static_cast<uint32_t>(head);
}

bool SQ::isActive(uint8_t index) const {
  if (static_cast<uint32_t>(head) == static_cast<uint32_t>(tail))
    return false;
  return ((static_cast<uint32_t>(index) - static_cast<uint32_t>(head) +
           SQ_CAP) &
           SQ_MASK) <
         ((static_cast<uint32_t>(tail) - static_cast<uint32_t>(head) +
           SQ_CAP) &
           SQ_MASK);
}

uint8_t SQ::getHead() const { return static_cast<uint32_t>(head); }
uint8_t SQ::getTail() const { return static_cast<uint32_t>(tail); }

auto SQ::getAddress(int index) const -> uint32_t {
  if (SQqueue[index].isAddressReady)
    return static_cast<uint32_t>(SQqueue[index].address);
  throw std::runtime_error("Address is not ready!");
}

auto SQ::getValue(int index) const -> int32_t {
  if (SQqueue[index].isValueReady)
    return static_cast<int32_t>(static_cast<uint32_t>(SQqueue[index].value));
  throw std::runtime_error("Value is not ready!");
}

auto SQ::headRobTag() const -> RobTag {
  return static_cast<uint32_t>(SQqueue[static_cast<uint32_t>(head)].robTag);
}

auto SQ::getRobTag(int index) const -> RobTag {
  return static_cast<uint32_t>(SQqueue[index].robTag);
}

auto SQ::getNBytes(int index) const -> int {
  uint32_t enc = static_cast<uint32_t>(SQqueue[index].n_bytes);
  return 1 << enc; // 0->1B, 1->2B, 2->4B
}

auto SQ::planDataForward(int index, int32_t value) const -> StoreNotify {
  StoreNotify notify{};
  if (SQqueue[index].isAddressReady == 0)
    return notify;
  notify.storeTag = static_cast<uint32_t>(SQqueue[index].robTag);
  notify.addr = static_cast<uint32_t>(SQqueue[index].address);
  notify.value = value;
  RobTag knownSameAddressOldestTag =
      static_cast<uint32_t>(SQqueue[index].robTag);
  RobTag unknownOldestTag = static_cast<uint32_t>(SQqueue[index].robTag);
  bool FoundKnownSameAddressOldest = false;
  bool FoundUnknownOldest = false;
  for (int k = 1; k <= SQ_CAP; ++k) {
    uint8_t i = (index + k) & SQ_MASK;
    if (i == index || !isActive(i))
      continue;
    if (((i - index) & SQ_MASK) >= ((tail - index) & SQ_MASK))
      continue;
    if (SQqueue[i].address == SQqueue[index].address &&
        SQqueue[i].isAddressReady && !FoundKnownSameAddressOldest) {
      knownSameAddressOldestTag = static_cast<uint32_t>(SQqueue[i].robTag);
      FoundKnownSameAddressOldest = true;
    } else if (SQqueue[i].isAddressReady == 0 && !FoundUnknownOldest) {
      unknownOldestTag = static_cast<uint32_t>(SQqueue[i].robTag);
      FoundUnknownOldest = true;
    }
  }
  notify.foundKnownSame = FoundKnownSameAddressOldest;
  notify.knownSameAddressOldestTag = knownSameAddressOldestTag;
  notify.foundUnknown = FoundUnknownOldest;
  notify.unknownOldestTag = unknownOldestTag;
  notify.valid = true;
  return notify;
}

auto SQ::planAddressForward(int index, uint32_t address) const -> StoreNotify {
  StoreNotify notify{};
  if (SQqueue[index].isValueReady == 0)
    return notify;
  notify.storeTag = static_cast<uint32_t>(SQqueue[index].robTag);
  notify.addr = address;
  notify.value = static_cast<uint32_t>(SQqueue[index].value);
  RobTag knownSameAddressOldestTag =
      static_cast<uint32_t>(SQqueue[index].robTag);
  RobTag unknownOldestTag = static_cast<uint32_t>(SQqueue[index].robTag);
  bool FoundKnownSameAddressOldest = false;
  bool FoundUnknownOldest = false;
  for (int k = 1; k <= SQ_CAP; ++k) {
    uint8_t i = (index + k) & SQ_MASK;
    if (i == index || !isActive(i))
      continue;
    if (((i - index) & SQ_MASK) >= ((tail - index) & SQ_MASK))
      continue;
    if (SQqueue[i].address == address && SQqueue[i].isAddressReady &&
        !FoundKnownSameAddressOldest) {
      knownSameAddressOldestTag = static_cast<uint32_t>(SQqueue[i].robTag);
      FoundKnownSameAddressOldest = true;
    } else if (SQqueue[i].isAddressReady == 0 && !FoundUnknownOldest) {
      unknownOldestTag = static_cast<uint32_t>(SQqueue[i].robTag);
      FoundUnknownOldest = true;
    }
  }
  notify.foundKnownSame = FoundKnownSameAddressOldest;
  notify.knownSameAddressOldestTag = knownSameAddressOldestTag;
  notify.foundUnknown = FoundUnknownOldest;
  notify.unknownOldestTag = unknownOldestTag;
  notify.valid = true;
  return notify;
}

auto SQ::replyToLoadRequest(uint32_t addr,
                            RobTag loadTag) const -> StoreResponse {
  int youngestSameAddrOrder = -1;
  int youngestUnknownOrder = -1;
  int forwardValue = 0;
  bool FoundSameAddr = false;
  bool SameAddrValueReady = false;
  bool reachedYoungerStore = false;
  for (int k = 0; k < SQ_CAP; k++) {
    const auto index = static_cast<uint32_t>((head + k) & SQ_MASK);
    if (!isActive(index))
      continue;
    if (!static_cast<bool>(SQqueue[index].isCommitted) &&
        ROB::isYounger(static_cast<uint32_t>(SQqueue[index].robTag), loadTag)) {
      reachedYoungerStore = true;
    }
    if (reachedYoungerStore)
      continue;
    if (!static_cast<bool>(SQqueue[index].isAddressReady)) {
      youngestUnknownOrder = k;
    } else if (SQqueue[index].address == addr) {
      youngestSameAddrOrder = k;
      FoundSameAddr = true;
      SameAddrValueReady = static_cast<bool>(SQqueue[index].isValueReady);
      if (SQqueue[index].isValueReady)
        forwardValue =
            static_cast<int32_t>(static_cast<uint32_t>(SQqueue[index].value));
    }
  }
  StoreResponse reply{};
  reply.valid = SameAddrValueReady && FoundSameAddr &&
                youngestSameAddrOrder > youngestUnknownOrder;
  reply.value = forwardValue;
  return reply;
}

bool SQ::canDispatchLoad(uint32_t addr, RobTag loadTag) const {
  bool hasSameAddressStore = false;
  bool hasUnknownAddressStore = false;
  for (int k = 0; k < SQ_CAP; k++) {
    if (hasSameAddressStore)
      continue;
    uint8_t cur = static_cast<uint32_t>((head + k) & SQ_MASK);
    if (!isActive(cur))
      continue;
    if (static_cast<bool>(SQqueue[cur].isCommitted))
      continue;
    if (!ROB::isOlder(static_cast<uint32_t>(SQqueue[cur].robTag), loadTag))
      continue;
    if (SQqueue[cur].isAddressReady && SQqueue[cur].address == addr)
      hasSameAddressStore = true;
    if (!static_cast<bool>(SQqueue[cur].isAddressReady))
      hasUnknownAddressStore = true;
  }
  return !hasSameAddressStore && !hasUnknownAddressStore;
}

bool SQ::isReadyToCommit(int index) const {
  return SQqueue[index].isAddressReady && SQqueue[index].isValueReady;
}

void SQ::work() {
  struct EntryIntent {
    bool tagWrite = false;
    RobTag tag = 0;
    bool addressWrite = false;
    uint32_t address = 0;
    bool valueWrite = false;
    uint32_t value = 0;
    bool bytesWrite = false;
    uint32_t bytes = 0;
    bool addressReadyWrite = false;
    bool addressReady = false;
    bool valueReadyWrite = false;
    bool valueReady = false;
    bool committedWrite = false;
    bool committed = false;
  };
  std::array<EntryIntent, SQ_CAP> intent{};
  bool headWrite = false;
  uint32_t headData = static_cast<uint32_t>(head);
  bool tailWrite = false;
  uint32_t tailData = static_cast<uint32_t>(tail);

  // Push initializes every reused row field. Later same-cycle sources update
  // the local intent, preserving main-tree last-writer-wins ordering.
  if (static_cast<bool>(issue.issueValid) &&
      static_cast<bool>(issue.issueStore)) {
    const uint32_t t = static_cast<uint32_t>(tail);
    intent[t].tagWrite = true;
    intent[t].tag = static_cast<uint32_t>(issue.issueTag);
    intent[t].addressWrite = true;
    intent[t].address = 0;
    intent[t].valueWrite = true;
    intent[t].value = 0;
    intent[t].bytesWrite = true;
    intent[t].bytes = static_cast<uint32_t>(issue.issueBytes);
    intent[t].addressReadyWrite = true;
    intent[t].addressReady = false;
    intent[t].valueReadyWrite = true;
    intent[t].valueReady = false;
    intent[t].committedWrite = true;
    intent[t].committed = false;
    tailWrite = true;
    tailData = (t + 1) & SQ_MASK;
  }

  // store value ready: write the value from the PRF (RS owns the slot, it
  // frees it in its own tick). svWriteValid already includes the squash guard.
  for (int i = 0; i < STORERS_CAP; ++i) {
    if (static_cast<bool>(prf.svWriteValid[i])) {
      const uint32_t q = memSlot(static_cast<uint32_t>(prf.svMemIndex[i]));
      intent[q].valueWrite = true;
      intent[q].value = static_cast<uint32_t>(prf.svValue[i]);
      intent[q].valueReadyWrite = true;
      intent[q].valueReady = true;
    }
  }
  // store address ready: write the address from the AGU result
  if (!static_cast<bool>(agu.isAGUEmpty) &&
      isStoreMem(static_cast<uint32_t>(agu.aguHeadMemIndex))) {
    RobTag aguRobTag = static_cast<uint32_t>(agu.aguHeadRobTag);
    if (!static_cast<bool>(squash.needSquash) ||
        ROB::isOlder(aguRobTag, static_cast<uint32_t>(squash.SquashTag))) {
      const uint32_t q =
          memSlot(static_cast<uint32_t>(agu.aguHeadMemIndex));
      intent[q].addressWrite = true;
      intent[q].address = static_cast<uint32_t>(agu.aguHeadValue);
      intent[q].addressReadyWrite = true;
      intent[q].addressReady = true;
    }
  }
  // dispatch decision apply: store sent to DMEM
  if (static_cast<bool>(memDispatch.memDispatchValid) &&
      static_cast<bool>(memDispatch.memDispatchIsStore)) {
    headWrite = true;
    headData = (static_cast<uint32_t>(head) + 1) & SQ_MASK;
  }

  if (static_cast<bool>(rob.storeWillCommit)) {
    const RobTag storeTag = static_cast<uint32_t>(rob.robHeadTag);
    for (int i = 0; i < SQ_CAP; ++i) {
      if (isActive(i) &&
          static_cast<uint32_t>(SQqueue[i].robTag) == storeTag &&
          !static_cast<bool>(SQqueue[i].isCommitted)) {
        intent[i].committedWrite = true;
        intent[i].committed = true;
      }
    }
  }

  // flush on squash
  if (static_cast<bool>(squash.needSquash) &&
      static_cast<bool>(rob.squashTagMatch)) {
    tailWrite = true;
    tailData = static_cast<uint32_t>(rob.squashSQTailSnapshot);
  }

  for (int i = 0; i < SQ_CAP; ++i) {
    if (intent[i].tagWrite)
      SQqueue[i].robTag <= intent[i].tag;
    if (intent[i].addressWrite)
      SQqueue[i].address <= intent[i].address;
    if (intent[i].valueWrite)
      SQqueue[i].value <= intent[i].value;
    if (intent[i].bytesWrite)
      SQqueue[i].n_bytes <= intent[i].bytes;
    if (intent[i].addressReadyWrite)
      SQqueue[i].isAddressReady <= intent[i].addressReady;
    if (intent[i].valueReadyWrite)
      SQqueue[i].isValueReady <= intent[i].valueReady;
    if (intent[i].committedWrite)
      SQqueue[i].isCommitted <= intent[i].committed;
  }
  if (headWrite)
    head <= headData;
  if (tailWrite)
    tail <= tailData;
}
