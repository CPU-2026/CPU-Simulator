#include "../include/DMEM.hpp"
#include "ROB.hpp"

int32_t DMEM::load_n_bytes(uint32_t address, int n, bool isSigned) const {
  int32_t result = 0;
  for (int i = 0; i < n; i++) {
    auto byte_data = read_data(address + i);
    result |= (byte_data << (i << 3));
    if (i == n - 1 && n < 4 && isSigned) {
      if (result & (1 << ((n << 3) - 1))) {
        auto mask = ~((1 << (n << 3)) - 1);
        result |= mask;
      }
    }
  }
  return result;
}

void DMEM::store_n_bytes(uint32_t address, int value, int n) {
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint8_t>(value >> (i << 3));
    write_data(address + i, byte_data);
  }
}

void DMEM::MemPull() { DMEMOutput::bufferValid <= false; }

bool DMEM::isBusy() const { return static_cast<bool>(DMEMOutput::busy); }

bool DMEM::isReady() const { return static_cast<bool>(DMEMOutput::bufferValid); }

void DMEM::work() {
  const bool busyOld = static_cast<bool>(DMEMOutput::busy);
  const bool bufValidOld = static_cast<bool>(DMEMOutput::bufferValid);

  // claim the pre-computed mem request (comb decided, wire latched)
  if (!busyOld && static_cast<bool>(DMEMInput::decisionValid)) {
    DMEMInner::execOp <= static_cast<uint32_t>(DMEMInput::op);
    DMEMInner::execValue <= static_cast<uint32_t>(DMEMInput::value);
    DMEMInner::execAddress <= static_cast<uint32_t>(DMEMInput::address);
    DMEMInner::execIsSigned <= static_cast<uint32_t>(DMEMInput::isSigned);
    DMEMInner::execNBytes <= static_cast<uint32_t>(DMEMInput::n_bytes);
    DMEMInner::execRobTag <= static_cast<uint32_t>(DMEMInput::robTag);
    DMEMInner::execMemIndex <= static_cast<uint32_t>(DMEMInput::memIndex);
    DMEMInner::execRemainCycle <= 3;
    DMEMOutput::busy <= true;
  }

  // reply pull: consume the previous cycle's reply (bufferValid_old)
  if (bufValidOld) {
    DMEMOutput::bufferValid <= false;
  }

  // execution stage: counts down remainCycle, produces reply
  if (busyOld) {
    const uint32_t rc = static_cast<uint32_t>(DMEMInner::execRemainCycle);
    if (rc == 1) {
      // last cycle: produce reply
      const uint32_t nEnc = static_cast<uint32_t>(DMEMInner::execNBytes);
      const int n = 1 << nEnc; // 0->1B, 1->2B, 2->4B
      const uint32_t addr = static_cast<uint32_t>(DMEMInner::execAddress);
      const bool isSignedV = static_cast<bool>(DMEMInner::execIsSigned);
      const uint32_t opVal = static_cast<uint32_t>(DMEMInner::execOp);
      const bool isLoad =
          opVal == static_cast<uint32_t>(Operation::Load);
      uint32_t outVal = static_cast<uint32_t>(DMEMInner::execValue);
      if (isLoad) {
        outVal = static_cast<uint32_t>(
            load_n_bytes(addr, n, isSignedV));
      } else {
        store_n_bytes(addr, static_cast<int32_t>(outVal), n);
      }
      DMEMOutput::respOp <= DMEMInner::execOp;
      DMEMOutput::respValue <= outVal;
      DMEMOutput::respAddress <= DMEMInner::execAddress;
      DMEMOutput::respIsSigned <= DMEMInner::execIsSigned;
      DMEMOutput::respNBytes <= DMEMInner::execNBytes;
      DMEMOutput::respRobTag <= DMEMInner::execRobTag;
      DMEMOutput::respMemIndex <= DMEMInner::execMemIndex;
      DMEMOutput::respRemainCycle <= 0;
      DMEMOutput::bufferValid <= true;
      DMEMOutput::busy <= false;
    } else {
      DMEMInner::execRemainCycle <= rc - 1;
    }
  }
}
