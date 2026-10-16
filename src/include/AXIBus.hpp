#pragma once
#include "module.h"
#include "tools.h"
#include <array>
#include <cstdint>

// CPU-side AXI4-Lite interconnect between the two cache adapters and the
// single external slave port. The fetch and data masters share AR in a
// round-robin (only contention toggles the pointer); the R beats are routed
// back by an owner FIFO that follows the granted AR order. AW/W/B belong to
// the data master alone and pass straight through.
constexpr int AXI_BUS_DEPTH = 64;
static_assert(AXI_BUS_DEPTH == 1 || (AXI_BUS_DEPTH & (AXI_BUS_DEPTH - 1)) == 0);

struct AXIBusInput {
  Wire<1> fetchArValid;
  Wire<32> fetchArAddr;
  Wire<1> fetchRReady;
  Wire<1> dataArValid;
  Wire<32> dataArAddr;
  Wire<1> dataRReady;
  Wire<1> dataAwValid;
  Wire<32> dataAwAddr;
  Wire<1> dataWValid;
  Wire<32> dataWData;
  Wire<4> dataWStrb;
  Wire<1> dataBReady;
  Wire<1> arReady;
  Wire<1> rValid;
  Wire<32> rData;
  Wire<2> rResp;
  Wire<1> awReady;
  Wire<1> wReady;
  Wire<1> bValid;
  Wire<2> bResp;
};
struct AXIBusOutput {
  Wire<1> fetchArReady;
  Wire<1> fetchRValid;
  Wire<32> fetchRData;
  Wire<2> fetchRResp;
  Wire<1> dataArReady;
  Wire<1> dataRValid;
  Wire<32> dataRData;
  Wire<2> dataRResp;
  Wire<1> dataAwReady;
  Wire<1> dataWReady;
  Wire<1> dataBValid;
  Wire<2> dataBResp;
  Wire<1> arValid;
  Wire<32> arAddr;
  Wire<1> rReady;
  Wire<1> awValid;
  Wire<32> awAddr;
  Wire<1> wValid;
  Wire<32> wData;
  Wire<4> wStrb;
  Wire<1> bReady;
};
struct AXIBusInner {
  std::array<Register<1>, AXI_BUS_DEPTH> owner; // 0 = fetch, 1 = data
  Register<6> ownerHead;
  Register<7> ownerCount;
  Register<1> preferData;
  Register<1> grantHeld, heldData;
};
struct AXIBus : dark::Module<AXIBusInput, AXIBusOutput, AXIBusInner> {
  AXIBus();
  void work() override;

 private:
  bool grantFetch() const;
  bool grantData() const;
  bool ownerIsFetch() const;
  bool ownerIsData() const;
};
