#include "../include/AXIBus.hpp"
#include <cstdint>

AXIBus::AXIBus() {
  arValid = [this]() -> uint32_t {
    return (static_cast<uint32_t>(ownerCount) < AXI_BUS_DEPTH &&
            (static_cast<bool>(fetchArValid) || static_cast<bool>(dataArValid)))
               ? 1u
               : 0u;
  };
  arAddr = [this]() -> uint32_t {
    return grantFetch() ? static_cast<uint32_t>(fetchArAddr)
                        : static_cast<uint32_t>(dataArAddr);
  };
  fetchArReady = [this]() -> uint32_t {
    return (static_cast<uint32_t>(ownerCount) < AXI_BUS_DEPTH && grantFetch() && static_cast<bool>(arReady)) ? 1u : 0u;
  };
  dataArReady = [this]() -> uint32_t {
    return (static_cast<uint32_t>(ownerCount) < AXI_BUS_DEPTH && grantData() && static_cast<bool>(arReady)) ? 1u : 0u;
  };
  rReady = [this]() -> uint32_t {
    if (ownerIsFetch())
      return static_cast<uint32_t>(fetchRReady);
    if (ownerIsData())
      return static_cast<uint32_t>(dataRReady);
    return 0u;
  };
  fetchRValid = [this]() -> uint32_t {
    return (static_cast<bool>(rValid) && ownerIsFetch()) ? 1u : 0u;
  };
  dataRValid = [this]() -> uint32_t {
    return (static_cast<bool>(rValid) && ownerIsData()) ? 1u : 0u;
  };
  fetchRData = [this]() -> uint32_t { return static_cast<uint32_t>(rData); };
  fetchRResp = [this]() -> uint32_t { return static_cast<uint32_t>(rResp); };
  dataRData = [this]() -> uint32_t { return static_cast<uint32_t>(rData); };
  dataRResp = [this]() -> uint32_t { return static_cast<uint32_t>(rResp); };
  awValid = [this]() -> uint32_t {
    return static_cast<uint32_t>(dataAwValid);
  };
  awAddr = [this]() -> uint32_t {
    return static_cast<uint32_t>(dataAwAddr);
  };
  dataAwReady = [this]() -> uint32_t {
    return static_cast<uint32_t>(awReady);
  };
  wValid = [this]() -> uint32_t { return static_cast<uint32_t>(dataWValid); };
  wData = [this]() -> uint32_t { return static_cast<uint32_t>(dataWData); };
  wStrb = [this]() -> uint32_t { return static_cast<uint32_t>(dataWStrb); };
  dataWReady = [this]() -> uint32_t {
    return static_cast<uint32_t>(wReady);
  };
  dataBValid = [this]() -> uint32_t { return static_cast<uint32_t>(bValid); };
  dataBResp = [this]() -> uint32_t { return static_cast<uint32_t>(bResp); };
  bReady = [this]() -> uint32_t { return static_cast<uint32_t>(dataBReady); };
}

bool AXIBus::grantFetch() const {
  if (static_cast<bool>(grantHeld)) return !static_cast<bool>(heldData);
  const bool fetch = static_cast<bool>(fetchArValid);
  const bool data = static_cast<bool>(dataArValid);
  return fetch && (!data || !static_cast<bool>(preferData));
}

bool AXIBus::grantData() const {
  if (static_cast<bool>(grantHeld)) return static_cast<bool>(heldData);
  const bool fetch = static_cast<bool>(fetchArValid);
  const bool data = static_cast<bool>(dataArValid);
  return data && (!fetch || static_cast<bool>(preferData));
}

bool AXIBus::ownerIsFetch() const {
  return static_cast<uint32_t>(ownerCount) != 0 &&
         !static_cast<bool>(owner[static_cast<uint32_t>(ownerHead)]);
}

bool AXIBus::ownerIsData() const {
  return static_cast<uint32_t>(ownerCount) != 0 &&
         static_cast<bool>(owner[static_cast<uint32_t>(ownerHead)]);
}

void AXIBus::work() {
  const uint32_t head = static_cast<uint32_t>(ownerHead);
  const uint32_t count = static_cast<uint32_t>(ownerCount);
  const bool bothValid = static_cast<bool>(fetchArValid) &&
                         static_cast<bool>(dataArValid);
  const bool arFire = count < AXI_BUS_DEPTH && (static_cast<bool>(fetchArValid) ||
                       static_cast<bool>(dataArValid)) &&
                      static_cast<bool>(arReady);
  const bool rFire = static_cast<bool>(rValid) && count != 0 &&
                     (static_cast<bool>(owner[head])
                          ? static_cast<bool>(dataRReady)
                          : static_cast<bool>(fetchRReady));

  // The granted AR order is the R return order; one owner tag per beat.
  const uint32_t tail = (head + count) & (AXI_BUS_DEPTH - 1);
  if (static_cast<bool>(arValid) && !static_cast<bool>(arReady) && !static_cast<bool>(grantHeld)) {
    grantHeld <= 1;
    heldData <= (grantData() ? 1u : 0u);
  } else if (arFire) grantHeld <= 0;
  if (arFire)
    owner[tail] <= (grantFetch() ? 0u : 1u);
  if (rFire)
    ownerHead <= ((head + 1u) & (AXI_BUS_DEPTH - 1));
  ownerCount <= count + (arFire ? 1u : 0u) - (rFire ? 1u : 0u);
  if (arFire && bothValid)
    preferData <= (static_cast<bool>(preferData) ? 0u : 1u);
}
