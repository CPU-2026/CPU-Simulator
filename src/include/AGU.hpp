#pragma once
#include "common.h"
#include "tools.h"
#include <array>
#include <cstdint>

static_assert(AGU_CAP == 4); // slot scans are fixed-length over AGU_CAP
// memIndex = MEM_STORE_BIT | slot(6b) must fit Wire<7>
static_assert(MEM_STORE_BIT == 0x40 && LQ_CAP <= 64 && SQ_CAP <= 64);

struct AGUInput {
  Wire<1> needSquash;
  Wire<ROB_TAG_WIDTH> SquashTag;
  // dispatch payload from the DispatchArbiter's agu grant (wired with lambdas
  // that guard on dispatch.valid; the load/store RS array choice is made
  // inside the wiring lambdas)
  Wire<1> dispatchValid;
  Wire<32> src1Value;
  Wire<32> src2Value;
  Wire<7> memIndex;
  Wire<ROB_TAG_WIDTH> dispatchRobTag;
};

struct AGUEntry {
  Register<32> value;
  Register<ROB_TAG_WIDTH> robTag;
  Register<7> memIndex;
};

struct AGUOutput {
  std::array<AGUEntry, AGU_CAP> slots;
  std::array<Register<1>, AGU_CAP> slotValid;
};

class AGU : public dark::Module<AGUInput, AGUOutput> {
public:
  // bridge accessors over the committed (_M_old) state; signatures unchanged
  // so unconverted consumers (LQ/SQ/MemRequestArbiter/FlushArbiter) keep working
  bool isFull() const;
  bool isEmpty() const;
  uint32_t headValue() const;
  RobTag headRobTag() const;
  uint8_t headMemIndex() const;
  void work() override;
};
