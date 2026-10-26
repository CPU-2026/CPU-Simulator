#pragma once
#include "Commit.hpp"
#include "common.h"
#include "module.h"
#include <array>
#include <cstdint>

struct SQEntry {
  Register<ROB_TAG_WIDTH> robTag;
  Register<32> address;
  Register<32> value;
  Register<2> nBytes; // 0->1B, 1->2B, 2->4B (DMEM/LQ aligned)
  Register<1> isAddressReady;
  Register<1> isValueReady;
  Register<1> isCommitted;
};

struct SQInputSquash {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
};
struct SQInputMemDispatch {
  Wire<1> memDispatchValid;
  Wire<1> memDispatchIsStore;
};
struct SQInputROB {
  Wire<SQ_SEQ_WIDTH> squashSQTailSnapshot;
  Wire<1> squashTagMatch;
  Wire<ROB_TAG_WIDTH> robHeadTag;
  Wire<1> storeWillCommit;
};
struct SQInputIssue {
  Wire<1> issueValid;
  Wire<1> issueStore;
  Wire<ROB_TAG_WIDTH> issueTag;
  Wire<2> issueBytes;
};
struct SQInputAGU {
  Wire<1> isAGUEmpty;
  Wire<7> aguHeadMemIndex;
  Wire<ROB_TAG_WIDTH> aguHeadRobTag;
  Wire<32> aguHeadValue;
};
struct SQInputPRF {
  std::array<Wire<1>, STORERS_CAP> svWriteValid; // value-ready + squash guard (wire-side)
  std::array<Wire<32>, STORERS_CAP> svValue;
  std::array<Wire<7>, STORERS_CAP> svMemIndex;
};
struct SQInput {
  std::array<CommitPort, COMMIT_WIDTH> commits;
  SQInputSquash squash;
  std::array<SQInputIssue, ISSUE_WIDTH> issue;
  SQInputMemDispatch memDispatch;
  SQInputROB rob;
  SQInputAGU agu;
  SQInputPRF prf;
};

// --- Output: combinational public views (assigns over committed state) ---
// StoreNotify packet: data-forward per storeValueRS slot and the single
// AGU-head-driven address-forward.
struct SQOutputNotifyData {
  std::array<Wire<1>, STORERS_CAP> valid;
  std::array<Wire<ROB_TAG_WIDTH>, STORERS_CAP> storeTag;
  std::array<Wire<32>, STORERS_CAP> addr;
  std::array<Wire<32>, STORERS_CAP> value;
  std::array<Wire<3>, STORERS_CAP> nBytes; // Actual byte count, not size encoding.
  std::array<Wire<1>, STORERS_CAP> foundKnownSame;
  std::array<Wire<ROB_TAG_WIDTH>, STORERS_CAP> knownTag;
  std::array<Wire<1>, STORERS_CAP> foundUnknown;
  std::array<Wire<ROB_TAG_WIDTH>, STORERS_CAP> unknownTag;
};
struct SQOutputNotifyAddr {
  Wire<1> valid;
  Wire<ROB_TAG_WIDTH> storeTag;
  Wire<32> addr;
  Wire<32> value;
  Wire<3> nBytes;
  Wire<1> foundKnownSame;
  Wire<ROB_TAG_WIDTH> knownTag;
  Wire<1> foundUnknown;
  Wire<ROB_TAG_WIDTH> unknownTag;
};
// StoreResponse: the store-forward reply to the AGU-head load query.
struct SQOutputReply {
  Wire<1> valid;
  Wire<32> value;
};
static_assert(ROB_CAP <= 32, "Store completion bitmap fits its carrier");
struct StoreCompletion {
  uint32_t readyMask = 0;
  uint32_t epochMask = 0;
};
struct SQOutputCompletion {
  Wire<ROB_CAP> readyMask;
  Wire<ROB_CAP> epochMask;
};
struct SQOutput {
  SQOutputNotifyData data;
  SQOutputNotifyAddr addr;
  SQOutputReply reply;
  SQOutputCompletion completion;
};

struct SQInner {
  std::array<SQEntry, SQ_CAP> SQqueue;
  Register<SQ_SEQ_WIDTH> head;
  Register<SQ_SEQ_WIDTH> tail;
};

struct StoreNotify {
  bool valid = false;
  RobTag storeTag = 0; // robTag of the broadcasting source store
  uint32_t addr = 0;    // store address (ready)
  int value = 0;        // store data
  uint8_t nBytes = 0;
  bool foundKnownSame =
      false; // a younger store with the same known address exists
  RobTag knownSameAddressOldestTag = 0; // robTag of the oldest of those
  bool foundUnknown = false;    // a younger store with unknown address exists
  RobTag unknownOldestTag = 0; // robTag of the oldest of those
};

struct StoreResponse {
  bool valid = false;
  int value = 0;
  uint8_t startByte = 0;
};

struct SQ : public dark::Module<SQInput, SQOutput, SQInner> {
private:
  void wire_output();
public:
  SQ();
  StoreCompletion completionBitmap() const;
  bool isEmpty() const;
  bool isFull() const;
  bool canAllocate(unsigned n) const {
    const unsigned used = (uint32_t(tail) - uint32_t(head)) & SQ_SEQ_MASK;
    return used + n <= unsigned(SQ_CAP);
  }
  bool isActive(uint8_t index) const;
  bool matchesAddress(uint8_t index, RobTag tag) const {
    return isActive(index) && getRobTag(index) == tag;
  }
  uint8_t getHead() const;
  uint8_t getTail() const; // Packed sequence, not a physical array index.
  // Occupancy boundary AFTER this cycle's own enqueue (see
  // LQ::getTailSnapshot).
  uint8_t getTailSnapshot() const {
    return static_cast<uint32_t>((tail + 1) & SQ_SEQ_MASK);
  }
  bool isReadyToCommit(int index) const;
  bool isCommitted(int index) const {
    return isActive(index) && static_cast<bool>(SQqueue[index].isCommitted);
  }
  auto getAddress(int index) const -> uint32_t;
  auto getValue(int index) const -> int32_t;
  auto headRobTag() const -> RobTag;
  auto getRobTag(int index) const -> RobTag;
  auto getNBytes(int index) const -> int;
  bool isAddressReady(int index) const {
    return static_cast<bool>(SQqueue[index].isAddressReady);
  }
  bool isValueReady(int index) const {
    return static_cast<bool>(SQqueue[index].isValueReady);
  }
  auto planDataForward(int index, int32_t value) const -> StoreNotify;
  auto planAddressForward(int index, uint32_t address, RobTag producerTag) const -> StoreNotify;
  auto replyToLoadRequest(uint32_t addr, uint8_t nBytes, RobTag loadTag) const
      -> StoreResponse;
  // Called after forwarding fails; nBytes is the actual Load byte count.
  bool canSendLoadToCache(uint32_t addr, uint8_t nBytes, RobTag loadTag) const;
  void work() override;
};
