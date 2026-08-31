#include "../include/LQ.hpp"
#include "../include/ROB.hpp"
#include <cstdint>
#include <stdexcept>

bool LQ::isEmpty() const { return static_cast<uint32_t>(tail) == static_cast<uint32_t>(head); }

bool LQ::isFull() const { return ((static_cast<uint32_t>(tail) + 1) & 0x0F) == static_cast<uint32_t>(head); }

bool LQ::isActive(uint8_t index) const {
  if (static_cast<uint32_t>(head) == static_cast<uint32_t>(tail))
    return false;
  return ((index - static_cast<uint32_t>(head) + LQ_CAP) & 0x0F) < ((static_cast<uint32_t>(tail) - static_cast<uint32_t>(head) + LQ_CAP) & 0x0F);
}

void LQ::pop() { head <= ((static_cast<uint32_t>(head) + 1) & 0xF); }

void LQ::pushLoad(RobTag robTag, int n_bytes, bool isUnsigned) {
  auto t = static_cast<uint32_t>(tail);
  // LQqueue[t] = {}; is ill-formed for Register members (deleted copy); the
  // subsequent per-field <= already sets all live fields. Only isCDBBroadcast
  // is not otherwise written on push and must be cleared explicitly; address/
  // value are written later via writeAddress/writeValue and are not read while
  // !isAddressReady / NOTREADY, so no need to clear them.
  LQqueue[t].isCDBBroadcast <= false;
  LQqueue[t].robTag <= robTag;
  // n_bytes already 2b encoded (0->1B, 1->2B, 2->4B) from IssuePacket via wire
  LQqueue[t].n_bytes <= static_cast<uint32_t>(n_bytes);
  LQqueue[t].isUnsigned <= isUnsigned;
  LQqueue[t].isAddressReady <= false;
  LQqueue[t].valueState <= static_cast<uint32_t>(ValueState::NOTREADY);
  tail <= ((t + 1) & 0xF);
}

uint8_t LQ::getHead() const { return static_cast<uint32_t>(head); }
uint8_t LQ::getTail() const { return static_cast<uint32_t>(tail); }

void LQ::flush(uint8_t tailSnapshot) { tail <= tailSnapshot; }

void LQ::writeAddress(uint32_t address, int index) {
  LQqueue[index].address <= address;
  LQqueue[index].isAddressReady <= true;
}

void LQ::writeValue(int32_t value, int index) {
  LQqueue[index].value <= value;
  LQqueue[index].valueState <= static_cast<uint32_t>(ValueState::READY);
  LQqueue[index].isCDBBroadcast <= false;
}

void LQ::writeValueIfFetching(uint8_t robTag, int index, int32_t value) {
  if (LQqueue[index].robTag != robTag)
    return;
  if (static_cast<uint32_t>(LQqueue[index].valueState) !=
      static_cast<uint32_t>(ValueState::FETCHING))
    return;
  writeValue(value, index);
}

void LQ::setValueState(int index, ValueState state) {
  LQqueue[index].valueState <= static_cast<uint32_t>(state);
}

auto LQ::getIsCDBBroadcast(int index) const -> bool {
  return static_cast<bool>(LQqueue[index].isCDBBroadcast);
}

auto LQ::getAddress(int index) const -> uint32_t {
  if (LQqueue[index].isAddressReady)
    return static_cast<uint32_t>(LQqueue[index].address);
  throw std::runtime_error("Address is not ready!");
}

auto LQ::getValue(int index) const -> int32_t {
  if (static_cast<uint32_t>(LQqueue[index].valueState) ==
      static_cast<uint32_t>(ValueState::READY))
    return static_cast<uint32_t>(LQqueue[index].value);
  throw std::runtime_error("Value is not ready!");
}

auto LQ::headRobTag() const -> uint8_t {
  return static_cast<uint32_t>(LQqueue[static_cast<uint32_t>(head)].robTag);
}

auto LQ::getRobTag(int index) const -> uint8_t {
  return static_cast<uint32_t>(LQqueue[index].robTag);
}

auto LQ::getIsUnsigned(int index) const -> bool {
  return static_cast<bool>(LQqueue[index].isUnsigned);
}

auto LQ::getNBytes(int index) const -> int {
  uint32_t enc = static_cast<uint32_t>(LQqueue[index].n_bytes);
  return 1 << enc; // 0->1, 1->2, 2->4
}

int LQ::LoadDetect() const {
  if (static_cast<uint32_t>(head) == static_cast<uint32_t>(tail))
    return 0xFFFFFFFF;
  int UnloadIndex = 0;
  bool foundUnload = false;
  for (int k = 0; k < LQ_CAP; k++) {
    uint8_t cur = static_cast<uint32_t>((static_cast<uint32_t>(head) + k) & 0x0F);
    if (!isActive(cur) || foundUnload)
      continue;
    if (LQqueue[cur].isAddressReady &&
        static_cast<uint32_t>(LQqueue[cur].valueState) ==
            static_cast<uint32_t>(ValueState::NOTREADY)) {
      foundUnload = true;
      UnloadIndex = cur;
    }
  }
  return foundUnload ? UnloadIndex : 0xFFFFFFFF;
}

int LQ::CDBDetect() const {
  if (static_cast<uint32_t>(head) == static_cast<uint32_t>(tail))
    return 0xFFFFFFFF;
  bool found = false;
  int detectedIndex = 0xFFFFFFFF;
  for (int k = 0; k < LQ_CAP; ++k) {
    uint8_t cur = static_cast<uint32_t>((static_cast<uint32_t>(head) + k) & 0x0F);
    if (!isActive(cur) || found)
      continue;
    if (LQqueue[cur].isAddressReady &&
        static_cast<uint32_t>(LQqueue[cur].valueState) ==
            static_cast<uint32_t>(ValueState::READY) &&
        LQqueue[cur].isCDBBroadcast == 0) {
      detectedIndex = cur;
      found = true;
    }
  }
  return detectedIndex;
}

bool LQ::isReadyToCommit(int index) const {
  if (LQqueue[index].isAddressReady &&
      static_cast<uint32_t>(LQqueue[index].valueState) == static_cast<uint32_t>(ValueState::READY)) {
    return true;
  }
  return false;
}

void LQ::applyStoreForward(const StoreNotify &notify) {
  for (int k = 0; k < LQ_CAP; ++k) {
    uint8_t i = static_cast<uint32_t>((static_cast<uint32_t>(head) + k) & 0xF);
    if (!isActive(i))
      break;
    if (LQqueue[i].isAddressReady == 0)
      continue;
    if (LQqueue[i].address != notify.addr)
      continue;
    if (!ROB::isOlder(notify.storeTag, static_cast<uint32_t>(LQqueue[i].robTag)))
      continue;
    bool blocked =
        (notify.foundKnownSame &&
         ROB::isOlder(notify.knownSameAddressOldestTag, static_cast<uint32_t>(LQqueue[i].robTag))) ||
        (notify.foundUnknown &&
         ROB::isOlder(notify.unknownOldestTag, static_cast<uint32_t>(LQqueue[i].robTag)));
    if (!blocked) {
      writeValue(notify.value, i);
    }
  }
}

void LQ::work() {
  if (issue.issueValid && issue.issueLoad)
    pushLoad(static_cast<uint32_t>(issue.issueTag), static_cast<uint32_t>(issue.issueBytes), static_cast<uint32_t>(issue.issueIsUnsigned));
  // store-forward broadcasts from SQ (data-ready events pre-computed in comb)
  for (int i = 0; i < STORERS_CAP; ++i) {
    if (!static_cast<bool>(storeNotifies.snValid[i]))
      continue;
    StoreNotify n{};
    n.valid = true;
    n.storeTag = static_cast<uint32_t>(storeNotifies.snStoreTag[i]);
    n.addr = static_cast<uint32_t>(storeNotifies.snAddr[i]);
    n.value = static_cast<uint32_t>(storeNotifies.snValue[i]);
    n.foundKnownSame = static_cast<bool>(storeNotifies.snFoundKnownSame[i]);
    n.knownSameAddressOldestTag = static_cast<uint32_t>(storeNotifies.snKnownTag[i]);
    n.foundUnknown = static_cast<bool>(storeNotifies.snFoundUnknown[i]);
    n.unknownOldestTag = static_cast<uint32_t>(storeNotifies.snUnknownTag[i]);
    applyStoreForward(n);
  }
  if (static_cast<bool>(storeNotifies.sanValid)) {
    StoreNotify n{};
    n.valid = true;
    n.storeTag = static_cast<uint32_t>(storeNotifies.sanStoreTag);
    n.addr = static_cast<uint32_t>(storeNotifies.sanAddr);
    n.value = static_cast<uint32_t>(storeNotifies.sanValue);
    n.foundKnownSame = static_cast<bool>(storeNotifies.sanFoundKnownSame);
    n.knownSameAddressOldestTag = static_cast<uint32_t>(storeNotifies.sanKnownTag);
    n.foundUnknown = static_cast<bool>(storeNotifies.sanFoundUnknown);
    n.unknownOldestTag = static_cast<uint32_t>(storeNotifies.sanUnknownTag);
    applyStoreForward(n);
  }
  // AGU: load address ready -> write address + query SQ for forwarding
  if (!static_cast<bool>(agu.isAGUEmpty) &&
      !isStoreMem(static_cast<uint32_t>(agu.aguHeadMemIndex))) {
    auto aguRobTag = static_cast<uint32_t>(agu.aguHeadRobTag);
    if (!static_cast<bool>(squash.needSquash) ||
        ROB::isOlder(aguRobTag, static_cast<uint32_t>(squash.SquashTag))) {
      auto aguMemIndex = static_cast<uint32_t>(agu.aguHeadMemIndex);
      auto value = static_cast<uint32_t>(agu.aguHeadValue);
      auto index = memSlot(aguMemIndex);
      writeAddress(value, index);
      if (static_cast<bool>(agu.sqReplyValid)) {
        writeValue(static_cast<uint32_t>(agu.sqReplyValue), index);
      }
    }
  }
  // dispatch decision apply
  if (memDispatch.memDispatchValid && memDispatch.memDispatchIsLoad) {
    setValueState(memSlot(static_cast<uint32_t>(memDispatch.memDispatchMemIndex)), ValueState::FETCHING);
  }
  // retire pop
  uint8_t cur = getHead();
  bool retireLoad = !static_cast<bool>(squash.needSquash) && cur != getTail() &&
                    (rob.isROBEmpty ||
                     ROB::isOlder(headRobTag(), static_cast<uint32_t>(rob.robHeadTag)));
  if (retireLoad) {
    pop();
  }
  // load response from DMEM
  if (loadResp.loadRespValid) {
    writeValueIfFetching(static_cast<uint32_t>(loadResp.loadRespRobTag),
                         memSlot(static_cast<uint32_t>(loadResp.loadRespMemIndex)),
                         static_cast<uint32_t>(loadResp.loadRespValue));
  }
  // CDB consume
  if (cdb.lsqSetCDB)
    LQqueue[memSlot(static_cast<uint32_t>(cdb.cdbMemIndex))].isCDBBroadcast <= true;
  // flush on squash
  if (static_cast<bool>(squash.needSquash))
    flush(static_cast<uint32_t>(rob.squashLQTailSnapshot));
}
