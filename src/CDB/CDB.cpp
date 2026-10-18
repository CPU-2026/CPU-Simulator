#include "../include/CDB.hpp"
#include "../include/ROB.hpp"
#include <cstdint>

bool AluCDB::aluLive() const {
  return !static_cast<bool>(aluEmpty) &&
         (!static_cast<bool>(squashNeed) ||
          ROB::isOlder(static_cast<uint32_t>(aluRobTag),
                       static_cast<uint32_t>(squashTag)));
}

void AluCDB::wire_output() {
  valid = [this]() -> uint32_t { return aluLive() ? 1u : 0u; };
  value = [this]() -> uint32_t {
    return aluLive() ? static_cast<uint32_t>(aluValue) : 0u;
  };
  robTag = [this]() -> uint32_t {
    return aluLive() ? static_cast<uint32_t>(aluRobTag) : 0u;
  };
  isControl = [this]() -> uint32_t {
    return aluLive() ? static_cast<uint32_t>(aluIsControl) : 0u;
  };
}

bool MulCDB::mulLive() const {
  return !static_cast<bool>(mulEmpty) &&
         (!static_cast<bool>(squashNeed) ||
          ROB::isOlder(static_cast<uint32_t>(mulRobTag),
                       static_cast<uint32_t>(squashTag)));
}

void MulCDB::wire_output() {
  valid = [this]() -> uint32_t { return mulLive(); };
  value = [this]() -> uint32_t {
    return mulLive() ? static_cast<uint32_t>(mulValue) : 0u;
  };
  robTag = [this]() -> uint32_t {
    return mulLive() ? static_cast<uint32_t>(mulRobTag) : 0u;
  };
}


bool DivCDB::divLive() const {
  return !static_cast<bool>(divEmpty) &&
         (!static_cast<bool>(squashNeed) ||
          ROB::isOlder(static_cast<uint32_t>(divRobTag),
                       static_cast<uint32_t>(squashTag)));
}

void DivCDB::wire_output() {
  valid = [this]() -> uint32_t { return divLive() ? 1u : 0u; };
  value = [this]() -> uint32_t {
    return divLive() ? static_cast<uint32_t>(divValue) : 0u;
  };
  robTag = [this]() -> uint32_t {
    return divLive() ? static_cast<uint32_t>(divRobTag) : 0u;
  };
}

bool LqCDB::lsqLive() const {
  return static_cast<bool>(lsqValid) &&
         (!static_cast<bool>(squashNeed) ||
          ROB::isOlder(static_cast<uint32_t>(lsqRobTag),
                       static_cast<uint32_t>(squashTag)));
}

void LqCDB::wire_output() {
  valid = [this]() -> uint32_t { return lsqLive() ? 1u : 0u; };
  value = [this]() -> uint32_t {
    return lsqLive() ? static_cast<uint32_t>(lsqValue) : 0u;
  };
  robTag = [this]() -> uint32_t {
    return lsqLive() ? static_cast<uint32_t>(lsqRobTag) : 0u;
  };
  memIndex = [this]() -> uint32_t {
    return lsqLive() ? static_cast<uint32_t>(lsqMemIndex) : 0u;
  };
}

uint32_t SharedCDB::selectOldest() const {
  uint32_t winner = 0;
  RobTag tag = 0;
  for (uint32_t i = 0; i < 3; ++i) {
    const RobTag candidateTag = static_cast<uint32_t>(candidates[i].robTag);
    if (static_cast<bool>(candidates[i].valid) &&
        (winner == 0 || ROB::isOlder(candidateTag, tag))) {
      winner = i + 1;
      tag = candidateTag;
    }
  }
  return winner;
}

SharedCDB::SharedCDB() {
  selected = [this]() { return selectOldest(); };
  valid = [this]() { return static_cast<uint32_t>(selected) != 0; };
  value = [this]() -> uint32_t {
    const uint32_t source = static_cast<uint32_t>(selected);
    return source ? static_cast<uint32_t>(candidates[source - 1].value) : 0u;
  };
  robTag = [this]() -> uint32_t {
    const uint32_t source = static_cast<uint32_t>(selected);
    return source ? static_cast<uint32_t>(candidates[source - 1].robTag) : 0u;
  };
  mulGrant = [this]() { return static_cast<uint32_t>(selected) == 1; };
  divGrant = [this]() { return static_cast<uint32_t>(selected) == 2; };
  lqGrant = [this]() { return static_cast<uint32_t>(selected) == 3; };
}
