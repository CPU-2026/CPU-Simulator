#pragma once
#include "../include/Memory.hpp"
#include "../include/common.h"
#include "module.h"
#include "tools.h"

struct DMEMInput {
  Wire<1> decisionValid;
  Wire<5> op;
  Wire<32> value;
  Wire<32> address;
  Wire<1> isSigned;
  Wire<2> n_bytes;
  Wire<7> robTag;
  Wire<7> memIndex;
};
struct DMEMInner{
  Register<5> execOp;
  Register<3> execRemainCycle;
  Register<32> execValue;
  Register<32> execAddress;
  Register<1> execIsSigned;
  Register<2> execNBytes;
  Register<7> execRobTag;
  Register<7> execMemIndex;
}; // MemExecution
struct DMEMOutput{
  Register<5> respOp;
  Register<3> respRemainCycle;
  Register<32> respValue;
  Register<32> respAddress;
  Register<1> respIsSigned;
  Register<2> respNBytes;
  Register<7> respRobTag;
  Register<7> respMemIndex;
  Register<1> busy;
  Register<1> bufferValid;
}; // MemOutputBuffer

// Data memory.
struct DMEM : public Memory, dark::Module<DMEMInput, DMEMOutput, DMEMInner> {
  void store_n_bytes(uint32_t address, int value, int n);
  void MemPull();
  DMEM(const Memory &mem) : Memory(mem) {}
  int32_t load_n_bytes(uint32_t address, int n, bool isSigned) const;
  bool isBusy() const;
  bool isReady() const;
  void work() override;
};
