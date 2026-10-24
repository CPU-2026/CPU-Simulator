#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include "common.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
// Pre-decode scan of the last fetched word (see main tree CPU.cpp): classify
// the unconditional-jump family (jal / jalr) so RAS maintenance and BTB type
// training no longer depend on prediction-table hits. Conditional branches
// are deliberately excluded. Mirrors main-tree logic exactly.
FetchTypeInfo scanJump(bool valid, uint32_t raw, uint32_t pc) {
  FetchTypeInfo fi{};
  if (!valid)
    return fi;
  const uint32_t opcode = raw & 0x7F;
  const uint32_t rd = (raw >> 7) & 0x1F;
  const uint32_t rs1 = (raw >> 15) & 0x1F;
  const uint32_t funct3 = (raw >> 12) & 0x7;
  const bool rdLink = (rd == 1 || rd == 5);
  const bool rs1Link = (rs1 == 1 || rs1 == 5);
  fi.pc = pc;
  if (opcode == 0x6F) { // jal: direct call iff rd is a link register
    fi.isCall = rdLink;
    fi.jalTargetValid = true;
    uint32_t uoff = ((raw >> 31) & 1U) << 20;
    uoff |= ((raw >> 20) & 1U) << 11;
    for (int i = 12; i <= 19; ++i)
      uoff |= ((raw >> i) & 1U) << i;
    for (int i = 21; i <= 30; ++i)
      uoff |= ((raw >> i) & 1U) << (i - 20);
    const auto off = static_cast<int32_t>((uoff ^ 0x100000U) - 0x100000U);
    // uint32 bit-vector add: signed int32_t add past the range is host UB.
    fi.jalTarget = pc + static_cast<uint32_t>(off);
    fi.valid = true;
  } else if (opcode == 0x67 && funct3 == 0) { // jalr
    fi.isCall = rdLink;                       // indirect call (push)
    fi.isRet = rs1Link;                       // independent pop; both => pop then push
    fi.valid = true;
  }
  return fi;
}

} // namespace

CPU::CPU(const Memory &mem) : SlaveModule(mem) {
  wire();
  // Non-owning registration in the exact work() order of the former
  // hand-written run loop (module members outlive dark::CPU's pointers).
  dcpu.add_module(&FetchUnitModule);
  dcpu.add_module(&AXIFetchModule);
  dcpu.add_module(&ICacheModule);
  dcpu.add_module(&FQModule);
  dcpu.add_module(&LQModule);
  dcpu.add_module(&SQModule);
  dcpu.add_module(&ROBModule);
  dcpu.add_module(&PRFModule);
  dcpu.add_module(&ALUModule);
  dcpu.add_module(&MULModule);
  dcpu.add_module(&DIVModule);
  dcpu.add_module(&AGUModule);
  dcpu.add_module(&BRUModule);
  dcpu.add_module(&BPUModule);
  dcpu.add_module(&DCacheModule);
  dcpu.add_module(&AXIDataModule);
  dcpu.add_module(&AXIBusModule);
  dcpu.add_module(&SlaveModule);
  dcpu.add_module(&RSModule);
  dcpu.add_module(&RATModule);
  dcpu.add_module(&AluCDBArbiterModule);
  dcpu.add_module(&LqCDBArbiterModule);
  dcpu.add_module(&MulCDBModule);
  dcpu.add_module(&DivCDBModule);
  dcpu.add_module(&SharedCDBModule);
  dcpu.add_module(&MemArbiterModule);
  dcpu.add_module(&DispatchArbiterModule);
  dcpu.add_module(&IssueArbiterModule);
  dcpu.add_module(&flushArbiter);
  dcpu.add_module(&DecodeUnitModule);
}

void CPU::wire() {
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
    connectCommit(RATModule.commits[lane], ROBModule.retirement[lane]);
    connectCommit(PRFModule.commits[lane], ROBModule.retirement[lane]);
    connectCommit(LQModule.commits[lane], ROBModule.retirement[lane]);
    connectCommit(SQModule.commits[lane], ROBModule.retirement[lane]);
    connectCommit(BPUModule.commits[lane], ROBModule.retirement[lane]);
    ROBModule.commitExit[lane] = [this, lane]() {
      bool exits = false;
      const auto tag = ROBModule.commitTag(lane);
      for (int row = 0; row < SQ_CAP; ++row)
        if (SQModule.isActive(row) && !SQModule.isCommitted(row) &&
            SQModule.getRobTag(row) == tag && SQModule.isAddressReady(row))
          exits |= SQModule.getAddress(row) == AXI_EXIT_ADDR;
      return exits;
    };
    ROBModule.commitStoreReady[lane] = [this, lane]() {
      bool ready = false;
      const auto tag = ROBModule.commitTag(lane);
      for (int row = 0; row < SQ_CAP; ++row)
        ready |= SQModule.isActive(row) && !SQModule.isCommitted(row) &&
            SQModule.getRobTag(row) == tag && SQModule.isReadyToCommit(row);
      return ready;
    };
  }
  auto recoveryPort = [this](RecoveryInput &port) {
    port.active = [this]() { return static_cast<uint32_t>(recoveryActive); };
    port.beat = [this]() { return static_cast<uint32_t>(recoveryBeat); };
    port.offset = [this]() { return static_cast<uint32_t>(recoveryOffset); };
    port.liveCount = [this]() { return static_cast<uint32_t>(recoveryCount); };
    port.nextTag = [this]() { return static_cast<uint32_t>(recoveryNextTag); };
    port.squashTag = [this]() { return static_cast<uint32_t>(recoveryBoundary); };
  };
  recoveryPort(RATModule.recovery);
  recoveryPort(PRFModule.recovery);
  recoveryPort(BPUModule.recovery);
  ROBModule.replayOffset = [this]() { return static_cast<bool>(recoveryActive) ? static_cast<uint32_t>(recoveryOffset) : 0u; };
  for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) {
    RATModule.replay[lane].tag = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayNarrow[lane].tag); };
    RATModule.replay[lane].rd = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayNarrow[lane].rd); };
    RATModule.replay[lane].newPhy = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayNarrow[lane].newPhy); };
    PRFModule.replay[lane].tag = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayNarrow[lane].tag); };
    PRFModule.replay[lane].rd = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayNarrow[lane].rd); };
    PRFModule.replay[lane].newPhy = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayNarrow[lane].newPhy); };
    BPUModule.replay[lane].tag = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayRas[lane].tag); };
    BPUModule.replay[lane].pc = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayRas[lane].pc); };
    BPUModule.replay[lane].pop = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayRas[lane].pop); };
    BPUModule.replay[lane].push = [this, lane]() { return static_cast<uint32_t>(ROBModule.replayRas[lane].push); };
  }
  flushArbiter.broadcastBlocked = [this]() { return static_cast<bool>(recoveryActive) || static_cast<bool>(exitCommitted); };
  ROBModule.retirementBlocked = [this]() { return !bool(initialized) || bool(exitCommitted) ||
      bool(recoveryActive) || bool(flushArbiter.needSquash); };
  // Wire FetchUnit's input wires once. needSquash/SquashPC come straight from
  // FlushArbiter (single producer, via its combinational member accessor);
  // haltSignal from ICache's combinational predicate; FetchValid/PredictPC
  // sample the BPU-owned prediction bundle (BPUOutput flat out* wires). Note:
  // the module inherits its Input, so wire the module instance
  // (FetchUnitModule.xxx), not a separate Input member.
  FetchUnitModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  FetchUnitModule.SquashPC = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashPC);
  };
  FetchUnitModule.FetchValid = [this]() {
    return static_cast<uint32_t>(BPUModule.outValid);
  };
  FetchUnitModule.PredictPC = [this]() {
    return static_cast<uint32_t>(BPUModule.outPredPC);
  };
  FetchUnitModule.haltSignal = [this]() {
    return static_cast<bool>(ICacheModule.popConsume) && ICacheModule.isHaltSignal();
  };

  // Wire InstructBuffer's (FQ) input wires once. icacheReturn* sample the
  // committed ICache/DecodeUnit views; haltFetched is a FetchUnit Register.
  FQModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  FQModule.icacheReturnAccepted = [this]() {
    return static_cast<bool>(ICacheModule.popConsume);
  };
  FQModule.icacheReturnValid1 = [this]() {
    return static_cast<uint32_t>(ICacheModule.returnValid1());
  };
  FQModule.icacheReturnRaw = [this]() { return ICacheModule.returnRaw(); };
  FQModule.icacheReturnPC = [this]() { return ICacheModule.returnPC(); };
  FQModule.icacheReturnPredictedPC = [this]() {
    return ICacheModule.returnPredictPC();
  };
  FQModule.icacheReturnCkptId = [this]() {
    return ICacheModule.returnCkptId();
  };
  FQModule.icacheReturnRaw1 = [this]() { return ICacheModule.returnRaw1(); };
  FQModule.icacheReturnPC1 = [this]() { return ICacheModule.returnPC1(); };
  FQModule.icacheReturnPredictedPC1 = [this]() {
    return ICacheModule.returnPredictPC1();
  };
  FQModule.decodePopCount = [this]() { return decodePopCount(); };

  // Wire the DecodeUnit's input wires once. fq* sample the converted
  // InstructBuffer's bridge accessors (_M_old); issueValid samples the
  // IssueArbiter's combinational Output.
  DecodeUnitModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  DecodeUnitModule.issueValid = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.valid);
  };
  DecodeUnitModule.decodePushCount = [this]() { return decodePopCount(); };
  DecodeUnitModule.fqHeadRaw = [this]() { return FQModule.headRaw(); };
  DecodeUnitModule.fqHeadPc = [this]() { return FQModule.headpc(); };
  DecodeUnitModule.fqHeadPredictedPC = [this]() {
    return FQModule.headPredictedPC();
  };
  DecodeUnitModule.fqHeadCkptId = [this]() { return FQModule.headCkptId(); };
  DecodeUnitModule.fqHeadRaw1 = [this]() { return FQModule.headRaw1(); };
  DecodeUnitModule.fqHeadPc1 = [this]() { return FQModule.headpc1(); };
  DecodeUnitModule.fqHeadPredictedPC1 = [this]() {
    return FQModule.headPredictedPC1();
  };
  DecodeUnitModule.fqHeadCkptId1 = [this]() { return FQModule.headCkptId1(); };
  // Wire the instruction-side AXI adapter's input wires once.
  AXIFetchModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  // original comb() gated imemFetch: a fetch that hits in the ICache must
  // NOT claim a memory line; accepted requests carry instruction-line bases.
  AXIFetchModule.fetchValid = [this]() {
    return static_cast<bool>(BPUModule.outValid) &&
           !ICacheModule.hit(static_cast<uint32_t>(BPUModule.outPC)) &&
           !ICacheModule.refillHit(static_cast<uint32_t>(FetchUnitModule.programCounter));
  };
  AXIFetchModule.fetchPC = [this]() {
    return static_cast<uint32_t>(BPUModule.outPC) & ~ICACHE_OFFSET_MASK;
  };
  AXIFetchModule.lineConsumed = [this]() {
    return !static_cast<bool>(exitCommitted) && static_cast<bool>(ICacheModule.fillFire);
  };

  // Wire the ICache's input wires once. fetch* sample the BPU-owned
  // prediction bundle (ungated — the IMEM side carries the icacheHit gate);
  // popConsume is a combinational predicate over committed state; lineReturn
  // references IMEM's combinational return view (Wire-over-_M_old, no extra
  // pipeline stage).
  ICacheModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  ICacheModule.fetchValid = [this]() {
    return static_cast<uint32_t>(BPUModule.outValid);
  };
  ICacheModule.fetchPC = [this]() {
    // outPC itself is fetchAllowed-gated. Use the raw PC here so the path
    // fillFire -> readBlocked -> fetchAllowed cannot form a combinational loop.
    return static_cast<uint32_t>(FetchUnitModule.programCounter);
  };
  ICacheModule.fetchPredictPC = [this]() {
    return static_cast<uint32_t>(BPUModule.outPredictedPC);
  };
  ICacheModule.fetchCkptId = [this]() {
    return static_cast<uint32_t>(BPUModule.outCkptId);
  };
  ICacheModule.fetchValid1 = [this]() {
    return static_cast<uint32_t>(BPUModule.outValid1);
  };
  ICacheModule.fetchPredictPC1 = [this]() {
    return static_cast<uint32_t>(BPUModule.outPredictedPC1);
  };
  ICacheModule.popConsume = [this]() {
    return !static_cast<bool>(recoveryActive) && !static_cast<bool>(flushArbiter.needSquash) && ICacheModule.isReturnReady() &&
           !static_cast<bool>(FetchUnitModule.haltFetched) &&
           FQModule.canAccept(ICacheModule.returnValid1() ? 2u : 1u,
                              decodePopCount());
  };
  ICacheModule.lineReturn.valid = [this]() { return AXIFetchModule.retValid(); };
  ICacheModule.lineReturn.lineAddr = [this]() {
    return AXIFetchModule.retLineAddr();
  };
  for (int w = 0; w < ICACHE_WORDS_PER_LINE; ++w) {
    ICacheModule.lineReturn.data[w] = [this, w]() {
      return AXIFetchModule.retWord(w);
    };
  }

  // ---- Wire the ALU result and three shared-CDB candidates (_M_old only;
  // lsq* are gated here so an invalid CDBDetect index never reaches
  // getValue's throw; each source carries its own squash guard) ----
  AluCDBArbiterModule.aluEmpty = [this]() {
    return static_cast<bool>(recoveryActive) || ALUModule.isEmpty() ? 1u : 0u;
  };
  AluCDBArbiterModule.aluValue = [this]() { return ALUModule.headValue(); };
  AluCDBArbiterModule.aluRobTag = [this]() {
    return static_cast<uint32_t>(ALUModule.headRobTag());
  };
  AluCDBArbiterModule.aluIsControl = [this]() {
    return ALUModule.headIsControl() ? 1u : 0u;
  };
  AluCDBArbiterModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  AluCDBArbiterModule.squashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };
  LqCDBArbiterModule.lsqValid = [this]() {
    return !bool(recoveryActive) && !bool(exitCommitted) && LQModule.resultCandidate().valid ? 1u : 0u;
  };
  LqCDBArbiterModule.lsqMemIndex = [this]() {
    return LQModule.resultCandidate().index;
  };
  LqCDBArbiterModule.lsqRobTag = [this]() {
    return LQModule.resultCandidate().robTag;
  };
  LqCDBArbiterModule.lsqValue = [this]() {
    return LQModule.resultCandidate().value;
  };
  LqCDBArbiterModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  LqCDBArbiterModule.squashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };
  // MulCDB: the MUL candidate before shared arbitration. Empty/value/tag sample the
  // MUL head bridges (_M_old); the squash guard lives in MulCDB::mulLive.
  MulCDBModule.mulEmpty = [this]() { return static_cast<bool>(recoveryActive) || MULModule.isEmpty() ? 1u : 0u; };
  MulCDBModule.mulValue = [this]() { return MULModule.headValue(); };
  MulCDBModule.mulRobTag = [this]() {
    return static_cast<uint32_t>(MULModule.headRobTag());
  };
  MulCDBModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  MulCDBModule.squashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };
  // DivCDB: the DIV candidate before shared arbitration. The divider has no output
  // buffer, so the bus is a straight tap on the unit's result registers --
  // divEmpty = !isReady, and divValue is gated by isReady because
  // DIV::getValue() throws on a stale operationType (the same guard the
  // reference's divCDB::build applies before reading it). The squash guard
  // lives in DivCDB::divLive.
  DivCDBModule.divEmpty = [this]() { return !static_cast<bool>(recoveryActive) && DIVModule.isReady() ? 0u : 1u; };
  DivCDBModule.divValue = [this]() {
    return DIVModule.isReady() ? DIVModule.getValue() : 0u;
  };
  DivCDBModule.divRobTag = [this]() {
    return static_cast<uint32_t>(DIVModule.getResultRobtag());
  };
  DivCDBModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  DivCDBModule.squashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };

  SharedCDBModule.candidates[0].valid = [this]() { return static_cast<bool>(MulCDBModule.valid); };
  SharedCDBModule.candidates[0].value = [this]() { return static_cast<uint32_t>(MulCDBModule.value); };
  SharedCDBModule.candidates[0].robTag = [this]() { return static_cast<uint32_t>(MulCDBModule.robTag); };
  SharedCDBModule.candidates[1].valid = [this]() { return static_cast<bool>(DivCDBModule.valid); };
  SharedCDBModule.candidates[1].value = [this]() { return static_cast<uint32_t>(DivCDBModule.value); };
  SharedCDBModule.candidates[1].robTag = [this]() { return static_cast<uint32_t>(DivCDBModule.robTag); };
  SharedCDBModule.candidates[2].valid = [this]() { return static_cast<bool>(LqCDBArbiterModule.valid); };
  SharedCDBModule.candidates[2].value = [this]() { return static_cast<uint32_t>(LqCDBArbiterModule.value); };
  SharedCDBModule.candidates[2].robTag = [this]() { return static_cast<uint32_t>(LqCDBArbiterModule.robTag); };

  // ---- Wire MemArbiter's Input Wires (producers stay bridges reading
  // _M_old; SQ head addr/value gated by ready flags and LQ load payload gated
  // by loadValid so the un-ready throw paths are never reached -- the ready-
  // flag gate is a superset-safe call of the original storeSelected-only
  // call sites, pure reads with no side effects) ----
  MemArbiterModule.dmemBusy = [this]() {
    return static_cast<uint32_t>(DCacheModule.isRequestFull());
  };
  MemArbiterModule.sqEmpty = [this]() { return SQModule.isEmpty() ? 1u : 0u; };
  MemArbiterModule.sqHeadRobTag = [this]() {
    return static_cast<uint32_t>(SQModule.headRobTag());
  };
  MemArbiterModule.sqHead = [this]() {
    return static_cast<uint32_t>(SQModule.getHead());
  };
  MemArbiterModule.sqHeadNEnc = [this]() {
    int n = SQModule.getNBytes(SQModule.getHead());
    return static_cast<uint32_t>(n == 1 ? 0 : (n == 2 ? 1 : 2));
  };
  MemArbiterModule.sqHeadAddr = [this]() {
    return (!SQModule.isEmpty() && SQModule.isAddressReady(SQModule.getHead()))
               ? static_cast<uint32_t>(SQModule.getAddress(SQModule.getHead()))
               : 0u;
  };
  MemArbiterModule.sqHeadValue = [this]() {
    return (!SQModule.isEmpty() && SQModule.isValueReady(SQModule.getHead()))
               ? static_cast<uint32_t>(SQModule.getValue(SQModule.getHead()))
               : 0u;
  };
  MemArbiterModule.sqHeadCommitted = [this]() {
    return !SQModule.isEmpty() && SQModule.isCommitted(SQModule.getHead()) ? 1u
                                                                           : 0u;
  };
  MemArbiterModule.robHeadTag = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.head);
  };
  MemArbiterModule.robStoreWillCommit = [this]() {
    bool retiring = false;
    for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
      retiring |= bool(ROBModule.retirement[lane].valid) && !SQModule.isEmpty() &&
          uint32_t(ROBModule.retirement[lane].tag) == SQModule.getRobTag(SQModule.getHead());
    return retiring;
  };
  MemArbiterModule.loadValid = [this]() {
    return LQModule.LoadDetect(LQModule.addressBypass()) != 0xFFFFFFFF ? 1u : 0u;
  };
  MemArbiterModule.loadIndex = [this]() {
    auto d = LQModule.LoadDetect(LQModule.addressBypass());
    return d != 0xFFFFFFFF ? static_cast<uint32_t>(d) : 0u;
  };
  MemArbiterModule.loadAddr = [this]() {
    const auto event = LQModule.addressBypass();
    auto d = LQModule.LoadDetect(event);
    return d != 0xFFFFFFFF ? LQModule.requestAddress(d, event) : 0u;
  };
  MemArbiterModule.loadRobTag = [this]() {
    auto d = LQModule.LoadDetect(LQModule.addressBypass());
    return d != 0xFFFFFFFF ? static_cast<uint32_t>(LQModule.getRobTag(d)) : 0u;
  };
  MemArbiterModule.loadIsSigned = [this]() {
    auto d = LQModule.LoadDetect(LQModule.addressBypass());
    return d != 0xFFFFFFFF ? (LQModule.getIsUnsigned(d) ? 0u : 1u) : 0u;
  };
  MemArbiterModule.loadNEnc = [this]() {
    auto d = LQModule.LoadDetect(LQModule.addressBypass());
    if (d == 0xFFFFFFFF)
      return 0u;
    int n = LQModule.getNBytes(d);
    return static_cast<uint32_t>(n == 1 ? 0 : (n == 2 ? 1 : 2));
  };
  MemArbiterModule.loadCanSendToCache = [this]() {
    const auto event = LQModule.addressBypass();
    auto d = LQModule.LoadDetect(event);
    return d != 0xFFFFFFFF && SQModule.canSendLoadToCache(
        LQModule.requestAddress(d, event), LQModule.getNBytes(d), LQModule.getRobTag(d))
               ? 1u
               : 0u;
  };
  MemArbiterModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  MemArbiterModule.squashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };

  // ---- Wire DispatchArbiter's Input Wires (ports are buses: RS slot fields
  // + PRF ready bitmap; physical source tags remain Wire<7> for PRF indexing,
  // while the separate ROB identity fields use ROB_TAG_WIDTH) ----
  // The ALU's single result slot is released in the same cycle it is presented
  // (or cancelled by flush if it cannot broadcast), so the buffer can
  // take a new op every cycle and the dispatch port is never capacity-blocked.
  // aluCanAccept includes same-cycle result drain or cancellation.
  DispatchArbiterModule.aluCanAccept = [this]() {
    const bool drained =
        !ALUModule.isEmpty() &&
        (!static_cast<bool>(flushArbiter.needSquash) ||
         ROB::isOlder(static_cast<uint32_t>(ALUModule.headRobTag()),
                      static_cast<uint32_t>(flushArbiter.SquashTag)));
    const bool cancelled = !ALUModule.isEmpty() &&
        static_cast<bool>(flushArbiter.needSquash) &&
        !ROB::isOlder(static_cast<uint32_t>(ALUModule.headRobTag()),
                     static_cast<uint32_t>(flushArbiter.SquashTag));
    return (!ALUModule.isFull() || drained || cancelled) ? 1u : 0u;
  };
  // LSQ entries are reserved at issue; full LQ/SQ cannot backpressure an
  // address update. The old result is delivered or cancelled this cycle.
  DispatchArbiterModule.aguCanAccept = [this]() {
    const bool drained = !AGUModule.isEmpty() &&
        (!static_cast<bool>(flushArbiter.needSquash) ||
         ROB::isOlder(AGUModule.headRobTag(),
                      static_cast<uint32_t>(flushArbiter.SquashTag)));
    const bool cancelled = !AGUModule.isEmpty() &&
        static_cast<bool>(flushArbiter.needSquash) &&
        !ROB::isOlder(AGUModule.headRobTag(),
                      static_cast<uint32_t>(flushArbiter.SquashTag));
    return (!AGUModule.isFull() || drained || cancelled) ? 1u : 0u;
  };
  DispatchArbiterModule.bruFull = [this]() {
    return BRUModule.isFull() ? 1u : 0u;
  };
  for (int i = 0; i < INTEGERRS_CAP; ++i) {
    DispatchArbiterModule.intBusy[i] = [this, i]() {
      return RSModule.isIntFree(i) ? 0u : 1u;
    };
    DispatchArbiterModule.intSrc1Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getIntSrc1(i).tag);
    };
    DispatchArbiterModule.intSrc2Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getIntSrc2(i).tag);
    };
    DispatchArbiterModule.intRobTag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getIntRobTag(i));
    };
  }
  for (int i = 0; i < LOADRS_CAP; ++i) {
    DispatchArbiterModule.loadBusy[i] = [this, i]() {
      return RSModule.isLoadFree(i) ? 0u : 1u;
    };
    DispatchArbiterModule.loadSrc1Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getLoadSrc1(i).tag);
    };
    DispatchArbiterModule.loadSrc2Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getLoadSrc2(i).tag);
    };
    DispatchArbiterModule.loadRobTag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getLoadRobTag(i));
    };
  }
  for (int i = 0; i < STORERS_CAP; ++i) {
    DispatchArbiterModule.saBusy[i] = [this, i]() {
      return RSModule.isSaFree(i) ? 0u : 1u;
    };
    DispatchArbiterModule.saSrc1Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getSaSrc1(i).tag);
    };
    DispatchArbiterModule.saRobTag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getSaRobTag(i));
    };
  }
  for (int i = 0; i < BRANCHRS_CAP; ++i) {
    DispatchArbiterModule.brBusy[i] = [this, i]() {
      return RSModule.isBrFree(i) ? 0u : 1u;
    };
    DispatchArbiterModule.brSrc1Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getBrSrc1(i).tag);
    };
    DispatchArbiterModule.brSrc2Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getBrSrc2(i).tag);
    };
    DispatchArbiterModule.brRobTag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getBrRobTag(i));
    };
  }
  // Admission propagates from the actual result grant/cancellation through
  // CSA to the partial-product stage, rather than inspecting only occupancy.
  DispatchArbiterModule.mulCanAccept = [this]() {
    return MULModule.partialCanAccept() ? 1u : 0u;
  };
  for (int i = 0; i < MULTIPLYRS_CAP; ++i) {
    DispatchArbiterModule.mulBusy[i] = [this, i]() {
      return RSModule.isMulFree(i) ? 0u : 1u;
    };
    DispatchArbiterModule.mulSrc1Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getMulSrc1(i).tag);
    };
    DispatchArbiterModule.mulSrc2Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getMulSrc2(i).tag);
    };
    DispatchArbiterModule.mulRobTag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getMulRobTag(i));
    };
  }
  // DIV channel buses: divideRS slot fields + the unit's own canAccept()
  // gate (the divider has no output buffer, so it is not an isFull gate --
  // verbatim from the main tree's div channel).
  DispatchArbiterModule.divAccept = [this]() {
    return DIVModule.canAccept() ? 1u : 0u;
  };
  for (int i = 0; i < DIVIDERS_CAP; ++i) {
    DispatchArbiterModule.divBusy[i] = [this, i]() {
      return RSModule.isDivFree(i) ? 0u : 1u;
    };
    DispatchArbiterModule.divSrc1Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getDivSrc1(i).tag);
    };
    DispatchArbiterModule.divSrc2Tag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getDivSrc2(i).tag);
    };
    DispatchArbiterModule.divRobTag[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getDivRobTag(i));
    };
  }
  for (int i = 0; i < PRF_CAP; ++i) {
    DispatchArbiterModule.prdReady[i] = [this, i]() {
      return PRFModule.isReady(i) ? 1u : 0u;
    };
    DispatchArbiterModule.bypassReady[i] = [this, i]() {
      return PRFModule.isOperandReady(Operand{.tag = i, .imm = 0}, grantedWritebacks()) ? 1u : 0u;
    };
  }
  DispatchArbiterModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  DispatchArbiterModule.squashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };

  // Wire the ALU's input wires once: dispatch payload samples the
  // DispatchArbiter's alu grant; RS/PRF reads guard on dispatch.valid.
  ALUModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  ALUModule.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  ALUModule.dispatchValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.alu.valid);
  };
  ALUModule.src1Value = [this]() {
    const auto &d = DispatchArbiterModule.alu;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                      RSModule.getIntSrc1(static_cast<uint32_t>(d.rsIndex)), grantedWritebacks()))
               : 0u;
  };
  ALUModule.src2Value = [this]() {
    const auto &d = DispatchArbiterModule.alu;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                      RSModule.getIntSrc2(static_cast<uint32_t>(d.rsIndex)), grantedWritebacks()))
               : 0u;
  };
  ALUModule.op = [this]() {
    const auto &d = DispatchArbiterModule.alu;
    return static_cast<bool>(d.valid) ? static_cast<uint32_t>(RSModule.getIntOp(
                                            static_cast<uint32_t>(d.rsIndex)))
                                      : 0u;
  };
  ALUModule.dispatchRobTag = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.alu.robTag);
  };
  ALUModule.cdbValid = [this]() { return AluCDBArbiterModule.valid ? 1u : 0u; };
  ALUModule.cdbRobTag = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.robTag);
  };

  // Wire the MUL's input wires once: dispatch payload samples the
  // DispatchArbiter's mul grant; RS/PRF reads guard on dispatch.valid; the
  // CDB drain samples the dedicated MulCDB bus.
  MULModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  MULModule.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  MULModule.dispatchValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.mul.valid);
  };
  MULModule.src1Value = [this]() {
    const auto &d = DispatchArbiterModule.mul;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                     RSModule.getMulSrc1(static_cast<uint32_t>(d.rsIndex))))
               : 0u;
  };
  MULModule.src2Value = [this]() {
    const auto &d = DispatchArbiterModule.mul;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                     RSModule.getMulSrc2(static_cast<uint32_t>(d.rsIndex))))
               : 0u;
  };
  MULModule.op = [this]() {
    const auto &d = DispatchArbiterModule.mul;
    return static_cast<bool>(d.valid) ? static_cast<uint32_t>(RSModule.getMulOp(
                                            static_cast<uint32_t>(d.rsIndex)))
                                      : 0u;
  };
  MULModule.dispatchRobTag = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.mul.robTag);
  };
  MULModule.cdbValid = [this]() { return SharedCDBModule.mulGrant ? 1u : 0u; };
  MULModule.cdbRobTag = [this]() {
    return static_cast<uint32_t>(MulCDBModule.robTag);
  };

  // Wire the DIV's input wires once: mirror of the MUL block over the
  // divideRS grant / the DivCDB drain.
  DIVModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  DIVModule.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  DIVModule.dispatchValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.div.valid);
  };
  DIVModule.src1Value = [this]() {
    const auto &d = DispatchArbiterModule.div;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                     RSModule.getDivSrc1(static_cast<uint32_t>(d.rsIndex))))
               : 0u;
  };
  DIVModule.src2Value = [this]() {
    const auto &d = DispatchArbiterModule.div;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                     RSModule.getDivSrc2(static_cast<uint32_t>(d.rsIndex))))
               : 0u;
  };
  DIVModule.op = [this]() {
    const auto &d = DispatchArbiterModule.div;
    return static_cast<bool>(d.valid) ? static_cast<uint32_t>(RSModule.getDivOp(
                                            static_cast<uint32_t>(d.rsIndex)))
                                      : 0u;
  };
  DIVModule.dispatchRobTag = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.div.robTag);
  };
  DIVModule.cdbValid = [this]() { return SharedCDBModule.divGrant ? 1u : 0u; };
  DIVModule.outReady = [this]() { return SharedCDBModule.divGrant ? 1u : 0u; };
  DIVModule.cdbRobTag = [this]() {
    return static_cast<uint32_t>(DivCDBModule.robTag);
  };

  // Wire the AGU's input wires once: the load/store RS array choice is made
  // here (rsType on the DispatchArbiter's agu grant), guarded by
  // dispatch.valid.
  AGUModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  AGUModule.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  AGUModule.dispatchValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.agu.valid);
  };
  AGUModule.src1Value = [this]() {
    const auto &d = DispatchArbiterModule.agu;
    if (!static_cast<bool>(d.valid))
      return 0u;
    if (static_cast<uint32_t>(d.rsType) == static_cast<uint32_t>(RSType::Load))
      return static_cast<uint32_t>(PRFModule.getOperandValue(
          RSModule.getLoadSrc1(static_cast<uint32_t>(d.rsIndex))));
    return static_cast<uint32_t>(PRFModule.getOperandValue(
        RSModule.getSaSrc1(static_cast<uint32_t>(d.rsIndex))));
  };
  AGUModule.src2Value = [this]() {
    const auto &d = DispatchArbiterModule.agu;
    if (!static_cast<bool>(d.valid))
      return 0u;
    if (static_cast<uint32_t>(d.rsType) == static_cast<uint32_t>(RSType::Load))
      return static_cast<uint32_t>(PRFModule.getOperandValue(
          RSModule.getLoadSrc2(static_cast<uint32_t>(d.rsIndex))));
    return static_cast<uint32_t>(PRFModule.getOperandValue(
        RSModule.getSaSrc2(static_cast<uint32_t>(d.rsIndex))));
  };
  AGUModule.memIndex = [this]() {
    const auto &d = DispatchArbiterModule.agu;
    if (!static_cast<bool>(d.valid))
      return 0u;
    if (static_cast<uint32_t>(d.rsType) == static_cast<uint32_t>(RSType::Load))
      return static_cast<uint32_t>(
          RSModule.getLoadMemIndex(static_cast<uint32_t>(d.rsIndex)));
    return static_cast<uint32_t>(
        RSModule.getSaMemIndex(static_cast<uint32_t>(d.rsIndex)));
  };
  AGUModule.dispatchRobTag = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.agu.robTag);
  };

  // Wire the BRU's input wires once: payload comes from the branchRS slot
  // selected by the DispatchArbiter's bru grant, guarded by dispatch.valid.
  BRUModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  BRUModule.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  BRUModule.dispatchValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.bru.valid);
  };
  BRUModule.src1Value = [this]() {
    const auto &d = DispatchArbiterModule.bru;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                      RSModule.getBrSrc1(static_cast<uint32_t>(d.rsIndex)), grantedWritebacks()))
               : 0u;
  };
  BRUModule.src2Value = [this]() {
    const auto &d = DispatchArbiterModule.bru;
    return static_cast<bool>(d.valid)
               ? static_cast<uint32_t>(PRFModule.getOperandValue(
                      RSModule.getBrSrc2(static_cast<uint32_t>(d.rsIndex)), grantedWritebacks()))
               : 0u;
  };
  BRUModule.pc = [this]() {
    const auto &d = DispatchArbiterModule.bru;
    return static_cast<bool>(d.valid) ? static_cast<uint32_t>(RSModule.getBrPc(
                                            static_cast<uint32_t>(d.rsIndex)))
                                      : 0u;
  };
  BRUModule.imm = [this]() {
    const auto &d = DispatchArbiterModule.bru;
    return static_cast<bool>(d.valid) ? static_cast<uint32_t>(RSModule.getBrImm(
                                            static_cast<uint32_t>(d.rsIndex)))
                                      : 0u;
  };
  BRUModule.op = [this]() {
    const auto &d = DispatchArbiterModule.bru;
    return static_cast<bool>(d.valid) ? static_cast<uint32_t>(RSModule.getBrOp(
                                            static_cast<uint32_t>(d.rsIndex)))
                                      : 0u;
  };
  BRUModule.dispatchRobTag = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.bru.robTag);
  };

  // Wire PRF's Input Wires (Verilog-style explicit ports)
  PRFModule.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  PRFModule.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  // Dual-CDB write ports: ALU keeps isControl; shared MUL/DIV/LQ omits it.
  // newPhy is
  // looked up from the ROB entry by tag, valid-gated (a broadcast only ever
  // targets an in-flight entry's rename).
  PRFModule.cdbOfALU.cdbValid = [this]() {
    const RobTag tag = static_cast<uint32_t>(AluCDBArbiterModule.robTag);
    return AluCDBArbiterModule.valid && ROBModule.matchesTag(tag) ? 1u : 0u;
  };
  PRFModule.cdbOfALU.cdbValue = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.value);
  };
  PRFModule.cdbOfALU.cdbRobTag = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.robTag);
  };
  PRFModule.cdbOfALU.cdbIsControl = [this]() {
    return AluCDBArbiterModule.isControl ? 1u : 0u;
  };
  PRFModule.cdbOfALU.cdbNewPhy = [this]() {
    const RobTag tag = static_cast<uint32_t>(AluCDBArbiterModule.robTag);
    if (!static_cast<bool>(AluCDBArbiterModule.valid) ||
        !ROBModule.matchesTag(tag))
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(ROBModule.entry.newPhy[robSlot(tag)]);
  };
  PRFModule.cdbOfShared.cdbValid = [this]() {
    const RobTag tag = static_cast<uint32_t>(SharedCDBModule.robTag);
    return SharedCDBModule.valid && ROBModule.matchesTag(tag) ? 1u : 0u;
  };
  PRFModule.cdbOfShared.cdbValue = [this]() {
    return static_cast<uint32_t>(SharedCDBModule.value);
  };
  PRFModule.cdbOfShared.cdbRobTag = [this]() {
    return static_cast<uint32_t>(SharedCDBModule.robTag);
  };
  PRFModule.cdbOfShared.cdbNewPhy = [this]() {
    const RobTag tag = static_cast<uint32_t>(SharedCDBModule.robTag);
    if (!static_cast<bool>(SharedCDBModule.valid) || !ROBModule.matchesTag(tag))
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(ROBModule.entry.newPhy[robSlot(tag)]);
  };
  PRFModule.issue.issueValid = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.valid);
  };
  PRFModule.issue.issuePhy = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.phy);
  };
  PRFModule.issue.issueAllocDest = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.allocDest);
  };
  PRFModule.issue.issuePC = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.pc);
  };
  PRFModule.issue.issueIsControl = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.isControl);
  };
  PRFModule.issue.issueCkptId = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.ckptId);
  };
  PRFModule.rob.robWillCommit = [this]() {
    return ROBModule.willCommit() ? 1u : 0u;
  };
  PRFModule.rob.robHeadIsHalt = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.isHeadHalt);
  };
  PRFModule.rob.robHeadType = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.headType);
  };
  PRFModule.rob.robHeadOldPhy = [this]() {
    if (static_cast<bool>(ROBModule.headView.isEmpty))
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(
        ROBModule.entry
            .oldPhy[robSlot(static_cast<uint32_t>(ROBModule.headView.head))]);
  };
  // Recovery payloads use the shared narrow window wired above.
  PRFModule.rob.robNextTag = [this]() { return ROBModule.getNextTag(); };

  // Wire RAT's Input Wires
  RATModule.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  RATModule.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  RATModule.issueValid = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.valid);
  };
  RATModule.issuePhy = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.phy);
  };
  RATModule.issueDest = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.dest);
  };
  RATModule.issueAllocDest = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.allocDest);
  };
  RATModule.rob.isEmpty = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.isEmpty);
  };
  RATModule.rob.willCommit = [this]() {
    return ROBModule.willCommit() ? 1u : 0u;
  };
  RATModule.rob.head = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.head);
  };
  RATModule.rob.next = [this]() { return ROBModule.getNextTag(); };
  RATModule.rob.headDest = [this]() {
    if (static_cast<bool>(ROBModule.headView.isEmpty))
      return 0u;
    return static_cast<uint32_t>(
        ROBModule.entry
            .dest[robSlot(static_cast<uint32_t>(ROBModule.headView.head))]);
  };
  RATModule.rob.headNewPhy = [this]() {
    if (static_cast<bool>(ROBModule.headView.isEmpty))
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(
        ROBModule.entry
            .newPhy[robSlot(static_cast<uint32_t>(ROBModule.headView.head))]);
  };
  for (int i = 0; i < ROB_CAP; ++i) {
    RATModule.rob.tag[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.tag[i]);
    };
  }

  // ---- Wire the IssueArbiter's Input Wires (stateless issue arbiter; the
  // former build() read these same producer bridges from comb, so every wire
  // is same-source same-cycle by construction). The RS busy bitmaps feed the
  // module's verbatim first-fit scans; the RAT operand views feed the
  // module's verbatim resolveSrc (assert-gated, NOT superset-safe). ----
  IssueArbiterModule.dec.isEmpty = [this]() {
    return DecodeUnitModule.isEmpty() ? 1u : 0u;
  };
  IssueArbiterModule.dec.type = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headType());
  };
  IssueArbiterModule.dec.opcode = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headOpcode());
  };
  IssueArbiterModule.dec.funct3 = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headFunct3());
  };
  IssueArbiterModule.dec.funct7 = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headFunct7());
  };
  IssueArbiterModule.dec.rd = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headRd());
  };
  IssueArbiterModule.dec.rs1 = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headRs1());
  };
  IssueArbiterModule.dec.rs2 = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headRs2());
  };
  IssueArbiterModule.dec.imm = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headImm());
  };
  IssueArbiterModule.dec.pc = [this]() { return DecodeUnitModule.headPc(); };
  IssueArbiterModule.dec.isHalt = [this]() {
    return DecodeUnitModule.headIsHalt() ? 1u : 0u;
  };
  IssueArbiterModule.dec.allocDest = [this]() {
    return DecodeUnitModule.headAllocDest() ? 1u : 0u;
  };
  IssueArbiterModule.dec.predictedPC = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headPredictedPC());
  };
  IssueArbiterModule.dec.ckptId = [this]() {
    return static_cast<uint32_t>(DecodeUnitModule.headCkptId());
  };
  IssueArbiterModule.rob.isFull = [this]() {
    return ROBModule.isFull() ? 1u : 0u;
  };
  IssueArbiterModule.rob.nextTag = [this]() { return ROBModule.getNextTag(); };
  for (int i = 0; i < INTEGERRS_CAP; ++i) {
    IssueArbiterModule.rs.intBusy[i] = [this, i]() {
      return RSModule.isIntFree(i) ? 0u : 1u;
    };
  }
  IssueArbiterModule.rs.intReuseValid = [this]() {
    return !bool(recoveryActive) && !bool(exitCommitted) && bool(DispatchArbiterModule.alu.valid) ? 1u : 0u;
  };
  IssueArbiterModule.rs.intReuseSlot = [this]() { return uint32_t(DispatchArbiterModule.alu.rsIndex); };
  IssueArbiterModule.rs.brReuseValid = [this]() {
    return !bool(recoveryActive) && !bool(exitCommitted) && bool(DispatchArbiterModule.bru.valid) ? 1u : 0u;
  };
  IssueArbiterModule.rs.brReuseSlot = [this]() { return uint32_t(DispatchArbiterModule.bru.rsIndex); };
  for (int i = 0; i < LOADRS_CAP; ++i) {
    IssueArbiterModule.rs.loadBusy[i] = [this, i]() {
      return RSModule.isLoadFree(i) ? 0u : 1u;
    };
  }
  for (int i = 0; i < STORERS_CAP; ++i) {
    IssueArbiterModule.rs.saBusy[i] = [this, i]() {
      return RSModule.isSaFree(i) ? 0u : 1u;
    };
    IssueArbiterModule.rs.svBusy[i] = [this, i]() {
      return RSModule.isSvFree(i) ? 0u : 1u;
    };
  }
  for (int i = 0; i < BRANCHRS_CAP; ++i) {
    IssueArbiterModule.rs.brBusy[i] = [this, i]() {
      return RSModule.isBrFree(i) ? 0u : 1u;
    };
  }
  for (int i = 0; i < MULTIPLYRS_CAP; ++i) {
    IssueArbiterModule.rs.mulBusy[i] = [this, i]() {
      return RSModule.isMulFree(i) ? 0u : 1u;
    };
  }
  for (int i = 0; i < DIVIDERS_CAP; ++i) {
    IssueArbiterModule.rs.divBusy[i] = [this, i]() {
      return RSModule.isDivFree(i) ? 0u : 1u;
    };
  }
  IssueArbiterModule.prf.freeListEmpty = [this]() {
    return PRFModule.isFreeListEmpty() ? 1u : 0u;
  };
  IssueArbiterModule.prf.freePhy = [this]() {
    return static_cast<uint32_t>(
        PRFModule.getFreeListSlot(PRFModule.getHeadSeq()));
  };
  IssueArbiterModule.lsq.lqFull = [this]() {
    return LQModule.isFull() ? 1u : 0u;
  };
  IssueArbiterModule.lsq.sqFull = [this]() {
    return SQModule.isFull() ? 1u : 0u;
  };
  IssueArbiterModule.lsq.lqTail = [this]() {
    return static_cast<uint32_t>(LQModule.getTail());
  };
  IssueArbiterModule.lsq.sqTail = [this]() {
    return static_cast<uint32_t>(SQModule.getTail());
  };
  IssueArbiterModule.lsq.lqTailSnapshot = [this]() {
    return static_cast<uint32_t>(LQModule.getTailSnapshot());
  };
  IssueArbiterModule.lsq.sqTailSnapshot = [this]() {
    return static_cast<uint32_t>(SQModule.getTailSnapshot());
  };
  IssueArbiterModule.rat.s1.ready = [this]() {
    return RATModule.readOperand(DecodeUnitModule.headRs1()).ready ? 1u : 0u;
  };
  IssueArbiterModule.rat.s1.phy = [this]() {
    return RATModule.readOperand(DecodeUnitModule.headRs1()).phyRegIndex;
  };
  IssueArbiterModule.rat.s1.value = [this]() {
    return static_cast<uint32_t>(
        RATModule.readOperand(DecodeUnitModule.headRs1()).value);
  };
  IssueArbiterModule.rat.s2.ready = [this]() {
    return RATModule.readOperand(DecodeUnitModule.headRs2()).ready ? 1u : 0u;
  };
  IssueArbiterModule.rat.s2.phy = [this]() {
    return RATModule.readOperand(DecodeUnitModule.headRs2()).phyRegIndex;
  };
  IssueArbiterModule.rat.s2.value = [this]() {
    return static_cast<uint32_t>(
        RATModule.readOperand(DecodeUnitModule.headRs2()).value);
  };
  IssueArbiterModule.rat.rdOldPhy = [this]() {
    return RATModule.readRAT_PRF(DecodeUnitModule.headRd());
  };
  IssueArbiterModule.squashNeed = [this]() {
    return static_cast<bool>(recoveryActive) || static_cast<bool>(exitCommitted) || static_cast<bool>(flushArbiter.needSquash) || ROBModule.isTrapCommitted();
  };
  // Wire RS's Input Wires: issue selection + flattened push payloads sample
  // the IssueArbiter's Output groups; dispatch release reads the
  // DispatchArbiter's grants; squash reads FlushArbiter; the storeValue
  // ready-release reads PRF readiness over this RS's own committed data.
  RSModule.sel.valid = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.valid);
  };
  RSModule.sel.hasInteger = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.hasInteger);
  };
  RSModule.sel.hasLoad = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.hasLoad);
  };
  RSModule.sel.hasStore = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.hasStore);
  };
  RSModule.sel.hasBranch = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.hasBranch);
  };
  RSModule.sel.hasMultiply = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.hasMultiply);
  };
  RSModule.sel.hasDivide = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.hasDivide);
  };
  RSModule.sel.integerSlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.integerSlot);
  };
  RSModule.sel.loadSlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.loadSlot);
  };
  RSModule.sel.storeAddrSlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.storeAddrSlot);
  };
  RSModule.sel.storeValueSlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.storeValueSlot);
  };
  RSModule.sel.branchSlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.branchSlot);
  };
  RSModule.sel.multiplySlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.multiplySlot);
  };
  RSModule.sel.divideSlot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.select.divideSlot);
  };
  RSModule.data.intP.op = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.intP.op);
  };
  RSModule.data.intP.s1Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.intP.s1Tag);
  };
  RSModule.data.intP.s1Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.intP.s1Imm);
  };
  RSModule.data.intP.s2Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.intP.s2Tag);
  };
  RSModule.data.intP.s2Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.intP.s2Imm);
  };
  RSModule.data.intP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.intP.robTag);
  };
  RSModule.data.loadP.op = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.op);
  };
  RSModule.data.loadP.s1Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.s1Tag);
  };
  RSModule.data.loadP.s1Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.s1Imm);
  };
  RSModule.data.loadP.s2Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.s2Tag);
  };
  RSModule.data.loadP.s2Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.s2Imm);
  };
  RSModule.data.loadP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.robTag);
  };
  RSModule.data.loadP.memIndex = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.loadP.memIndex);
  };
  RSModule.data.saP.op = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.op);
  };
  RSModule.data.saP.s1Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.s1Tag);
  };
  RSModule.data.saP.s1Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.s1Imm);
  };
  RSModule.data.saP.s2Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.s2Tag);
  };
  RSModule.data.saP.s2Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.s2Imm);
  };
  RSModule.data.saP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.robTag);
  };
  RSModule.data.saP.memIndex = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.saP.memIndex);
  };
  RSModule.data.svP.dataTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.svP.dataTag);
  };
  RSModule.data.svP.dataImm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.svP.dataImm);
  };
  RSModule.data.svP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.svP.robTag);
  };
  RSModule.data.svP.memIndex = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.svP.memIndex);
  };
  RSModule.data.brP.op = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.op);
  };
  RSModule.data.brP.s1Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.s1Tag);
  };
  RSModule.data.brP.s1Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.s1Imm);
  };
  RSModule.data.brP.s2Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.s2Tag);
  };
  RSModule.data.brP.s2Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.s2Imm);
  };
  RSModule.data.brP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.robTag);
  };
  RSModule.data.brP.imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.imm);
  };
  RSModule.data.brP.pc = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.brP.pc);
  };
  RSModule.data.mulP.op = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.mulP.op);
  };
  RSModule.data.mulP.s1Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.mulP.s1Tag);
  };
  RSModule.data.mulP.s1Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.mulP.s1Imm);
  };
  RSModule.data.mulP.s2Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.mulP.s2Tag);
  };
  RSModule.data.mulP.s2Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.mulP.s2Imm);
  };
  RSModule.data.mulP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.mulP.robTag);
  };
  RSModule.data.divP.op = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.divP.op);
  };
  RSModule.data.divP.s1Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.divP.s1Tag);
  };
  RSModule.data.divP.s1Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.divP.s1Imm);
  };
  RSModule.data.divP.s2Tag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.divP.s2Tag);
  };
  RSModule.data.divP.s2Imm = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.divP.s2Imm);
  };
  RSModule.data.divP.robTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.divP.robTag);
  };
  RSModule.dispatch.aluValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.alu.valid);
  };
  RSModule.dispatch.aluIdx = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.alu.rsIndex);
  };
  RSModule.dispatch.aguValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.agu.valid);
  };
  RSModule.dispatch.aguIdx = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.agu.rsIndex);
  };
  RSModule.dispatch.aguIsLoad = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.agu.rsType) ==
           static_cast<uint32_t>(RSType::Load);
  };
  RSModule.dispatch.bruValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.bru.valid);
  };
  RSModule.dispatch.bruIdx = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.bru.rsIndex);
  };
  RSModule.dispatch.mulValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.mul.valid);
  };
  RSModule.dispatch.mulIdx = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.mul.rsIndex);
  };
  RSModule.dispatch.divValid = [this]() {
    return static_cast<bool>(DispatchArbiterModule.div.valid);
  };
  RSModule.dispatch.divIdx = [this]() {
    return static_cast<uint32_t>(DispatchArbiterModule.div.rsIndex);
  };
  RSModule.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  RSModule.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  for (int i = 0; i < STORERS_CAP; ++i) {
    RSModule.prf.svReady[i] = [this, i]() {
      return PRFModule.isOperandReady(RSModule.getSvData(i));
    };
  }

  // ---- Wire DCache's Input Wires (decision from the MemArbiter Output;
  // squash from FlushArbiter; adapter completion observed through the AXIData
  // bridge accessors reading _M_old -- the DCache is now the only memory
  // client) ----
  DCacheModule.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  DCacheModule.squashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  DCacheModule.decisionValid = [this]() {
    return memoryDispatchValid() ? 1u : 0u;
  };
  DCacheModule.decisionOp = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.op);
  };
  DCacheModule.decisionValue = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.value);
  };
  DCacheModule.decisionAddr = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.address);
  };
  DCacheModule.decisionIsSigned = [this]() {
    return MemArbiterModule.isSigned ? 1u : 0u;
  };
  DCacheModule.decisionNBytes = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.nEnc);
  };
  DCacheModule.decisionRobTag = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.robTag);
  };
  DCacheModule.decisionMemIndex = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.memIndex);
  };
  DCacheModule.dmemCredits = [this]() {
    return AXIDataModule.freeCredits();
  };
  DCacheModule.dmemWriteReady = [this]() {
    return AXIDataModule.freeCredits() != 0;
  };
  DCacheModule.dmemReplyReady = [this]() {
    return AXIDataModule.isReplyReady() ? 1u : 0u;
  };
  DCacheModule.dmemWriteReplyReady = [this]() { return AXIDataModule.isWriteReplyReady(); };
  DCacheModule.replyAddress = [this]() { return AXIDataModule.replyAddress(); };
  DCacheModule.replyTransaction = [this]() { return AXIDataModule.replyTransaction(); };
  DCacheModule.writeAddress = [this]() { return AXIDataModule.writeReplyAddress(); };
  DCacheModule.writeTransaction = [this]() { return AXIDataModule.writeReplyTransaction(); };
  DCacheModule.exitReady = [this]() { return AXIDataModule.exitReady(); };
  for (int i = 0; i < DCACHE_BLOCK_CAP; ++i) {
    DCacheModule.dmemLineData[i] = [this, i]() {
      return AXIDataModule.lineByte(i);
    };
  }

  // Wire the data-side AXI adapter's Input Wires (dual-port line access:
  // readValid/readAddress for fills, writeValid/writeAddress/writeLineData
  // for dirty writebacks -- the DCache is the only producer).
  AXIDataModule.readValid = [this]() {
    return static_cast<uint32_t>(DCacheModule.reqReadValid);
  };
  AXIDataModule.readAddress = [this]() {
    return static_cast<uint32_t>(DCacheModule.reqReadAddr);
  };
  AXIDataModule.readTransaction = [this]() { return static_cast<uint32_t>(DCacheModule.reqReadTransaction); };
  AXIDataModule.writeTransaction = [this]() { return static_cast<uint32_t>(DCacheModule.reqWriteTransaction); };
  AXIDataModule.writeConsumed = [this]() { return static_cast<bool>(DCacheModule.writeConsumed); };
  AXIDataModule.exitValid = [this]() { return static_cast<bool>(DCacheModule.exitValid); };
  AXIDataModule.exitValue = [this]() { return static_cast<uint32_t>(DCacheModule.exitValue); };
  AXIDataModule.rResp = [this]() { return static_cast<uint32_t>(AXIBusModule.dataRResp); };
  AXIDataModule.bResp = [this]() { return static_cast<uint32_t>(AXIBusModule.dataBResp); };
  AXIDataModule.writeValid = [this]() {
    return static_cast<uint32_t>(DCacheModule.reqWriteValid);
  };
  AXIDataModule.writeAddress = [this]() {
    return static_cast<uint32_t>(DCacheModule.reqWriteAddr);
  };
  for (int i = 0; i < DCACHE_BLOCK_CAP; ++i) {
    AXIDataModule.writeLineData[i] = [this, i]() {
      return static_cast<uint32_t>(DCacheModule.reqWriteLineData[i]);
    };
  }
  AXIDataModule.replyConsumed = [this]() {
    return static_cast<uint32_t>(DCacheModule.fillFire);
  };

  // ---- Wire the AXI4-Lite interconnect: two masters in (instruction fetch
  // and data), one slave port out. Every channel is a Wire over committed
  // state; the bus owns the AR round-robin and the R owner FIFO. ----
  AXIBusModule.fetchArValid = [this]() {
    return static_cast<uint32_t>(AXIFetchModule.arValid);
  };
  AXIBusModule.fetchArAddr = [this]() {
    return static_cast<uint32_t>(AXIFetchModule.arAddr);
  };
  AXIBusModule.fetchRReady = [this]() {
    return static_cast<uint32_t>(AXIFetchModule.rReady);
  };
  AXIBusModule.dataArValid = [this]() {
    return static_cast<uint32_t>(AXIDataModule.arValid);
  };
  AXIBusModule.dataArAddr = [this]() {
    return static_cast<uint32_t>(AXIDataModule.arAddr);
  };
  AXIBusModule.dataRReady = [this]() {
    return static_cast<uint32_t>(AXIDataModule.rReady);
  };
  AXIBusModule.dataAwValid = [this]() {
    return static_cast<uint32_t>(AXIDataModule.awValid);
  };
  AXIBusModule.dataAwAddr = [this]() {
    return static_cast<uint32_t>(AXIDataModule.awAddr);
  };
  AXIBusModule.dataWValid = [this]() {
    return static_cast<uint32_t>(AXIDataModule.wValid);
  };
  AXIBusModule.dataWData = [this]() {
    return static_cast<uint32_t>(AXIDataModule.wData);
  };
  AXIBusModule.dataWStrb = [this]() {
    return static_cast<uint32_t>(AXIDataModule.wStrb);
  };
  AXIBusModule.dataBReady = [this]() {
    return static_cast<uint32_t>(AXIDataModule.bReady);
  };
  AXIBusModule.arReady = [this]() {
    return static_cast<uint32_t>(SlaveModule.arReady);
  };
  AXIBusModule.rValid = [this]() {
    return static_cast<uint32_t>(SlaveModule.rValid);
  };
  AXIBusModule.rData = [this]() {
    return static_cast<uint32_t>(SlaveModule.rData);
  };
  AXIBusModule.rResp = [this]() {
    return static_cast<uint32_t>(SlaveModule.rResp);
  };
  AXIBusModule.awReady = [this]() {
    return static_cast<uint32_t>(SlaveModule.awReady);
  };
  AXIBusModule.wReady = [this]() {
    return static_cast<uint32_t>(SlaveModule.wReady);
  };
  AXIBusModule.bValid = [this]() {
    return static_cast<uint32_t>(SlaveModule.bValid);
  };
  AXIBusModule.bResp = [this]() {
    return static_cast<uint32_t>(SlaveModule.bResp);
  };
  AXIFetchModule.arReady = [this]() {
    return static_cast<uint32_t>(AXIBusModule.fetchArReady);
  };
  AXIFetchModule.rValid = [this]() {
    return static_cast<uint32_t>(AXIBusModule.fetchRValid);
  };
  AXIFetchModule.rData = [this]() {
    return static_cast<uint32_t>(AXIBusModule.fetchRData);
  };
  AXIDataModule.arReady = [this]() {
    return static_cast<uint32_t>(AXIBusModule.dataArReady);
  };
  AXIDataModule.rValid = [this]() {
    return static_cast<uint32_t>(AXIBusModule.dataRValid);
  };
  AXIDataModule.rData = [this]() {
    return static_cast<uint32_t>(AXIBusModule.dataRData);
  };
  AXIDataModule.awReady = [this]() {
    return static_cast<uint32_t>(AXIBusModule.dataAwReady);
  };
  AXIDataModule.wReady = [this]() {
    return static_cast<uint32_t>(AXIBusModule.dataWReady);
  };
  AXIDataModule.bValid = [this]() {
    return static_cast<uint32_t>(AXIBusModule.dataBValid);
  };
  // Wire the external slave's Input Wires from the single AXI master port.
  SlaveModule.arValid = [this]() {
    return static_cast<uint32_t>(AXIBusModule.arValid);
  };
  SlaveModule.arAddr = [this]() {
    return static_cast<uint32_t>(AXIBusModule.arAddr);
  };
  SlaveModule.rReady = [this]() {
    return static_cast<uint32_t>(AXIBusModule.rReady);
  };
  SlaveModule.awValid = [this]() {
    return static_cast<uint32_t>(AXIBusModule.awValid);
  };
  SlaveModule.awAddr = [this]() {
    return static_cast<uint32_t>(AXIBusModule.awAddr);
  };
  SlaveModule.wValid = [this]() {
    return static_cast<uint32_t>(AXIBusModule.wValid);
  };
  SlaveModule.wData = [this]() {
    return static_cast<uint32_t>(AXIBusModule.wData);
  };
  SlaveModule.wStrb = [this]() {
    return static_cast<uint32_t>(AXIBusModule.wStrb);
  };
  SlaveModule.bReady = [this]() {
    return static_cast<uint32_t>(AXIBusModule.bReady);
  };

  LQModule.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  LQModule.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  LQModule.issue.issueValid = [this]() {
    return (static_cast<bool>(IssueArbiterModule.core.valid) &&
            static_cast<bool>(IssueArbiterModule.core.isLoad))
               ? 1u
               : 0u;
  };
  LQModule.issue.issueLoad = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.isLoad);
  };
  LQModule.issue.issueTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.robTag);
  };
  LQModule.issue.issueBytes = [this]() {
    const uint32_t n = static_cast<uint32_t>(IssueArbiterModule.core.nBytes);
    return (n == 1 ? 0u : (n == 2 ? 1u : 2u));
  };
  LQModule.issue.issueIsUnsigned = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.isUnsigned);
  };
  LQModule.cdb.lsqSetCDB = [this]() {
    // lqCDB.valid already embeds the squash guard on the lsq candidate
    // (LqCDBArbiter passes only candidates older than the squash point), so
    // the former re-check here is a redundant idempotent gate.
    return SharedCDBModule.lqGrant ? 1u : 0u;
  };
  LQModule.cdb.cdbMemIndex = [this]() {
    return static_cast<uint32_t>(LqCDBArbiterModule.memIndex);
  };
  LQModule.agu.isAGUEmpty = [this]() { return bool(recoveryActive) || bool(exitCommitted) || AGUModule.isEmpty(); };
  LQModule.agu.aguHeadMemIndex = [this]() { return AGUModule.headMemIndex(); };
  LQModule.agu.aguHeadRobTag = [this]() { return AGUModule.headRobTag(); };
  LQModule.agu.aguHeadValue = [this]() { return AGUModule.headValue(); };
  LQModule.agu.sqReplyValid = [this]() {
    return static_cast<uint32_t>(SQModule.reply.valid);
  };
  LQModule.agu.sqReplyValue = [this]() {
    return static_cast<uint32_t>(SQModule.reply.value);
  };
  LQModule.rob.isROBEmpty = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.isEmpty);
  };
  LQModule.rob.robWillCommit = [this]() { return ROBModule.willCommit() ? 1u : 0u; };
  LQModule.rob.robHeadTag = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.head);
  };
  LQModule.rob.squashLQTailSnapshot = [this]() {
    const RobTag tag = static_cast<uint32_t>(flushArbiter.SquashTag);
    if (!ROBModule.matchesTag(tag))
      return 0u;
    return static_cast<uint32_t>(ROBModule.entry.lqTailSnapshot[robSlot(tag)]);
  };
  LQModule.rob.squashTagMatch = [this]() {
    return ROBModule.matchesTag(static_cast<uint32_t>(flushArbiter.SquashTag))
               ? 1u
               : 0u;
  };
  // loadResp now comes from the DCache (hit self-answer or fill serve); the
  // squash guard lives inside DCache::wire_output (valid && (!needSquash ||
  // isOlder)).
  LQModule.loadResp.loadRespValid = [this]() {
    return static_cast<uint32_t>(DCacheModule.loadRespValid);
  };
  LQModule.loadResp.loadRespMemIndex = [this]() {
    return static_cast<uint32_t>(DCacheModule.loadRespMemIndex);
  };
  LQModule.loadResp.loadRespRobTag = [this]() {
    return static_cast<uint32_t>(DCacheModule.loadRespRobTag);
  };
  LQModule.loadResp.loadRespValue = [this]() {
    return static_cast<uint32_t>(DCacheModule.loadRespValue);
  };
  LQModule.memDispatch.memDispatchValid = [this]() {
    return MemArbiterModule.valid ? 1u : 0u;
  };
  LQModule.memDispatch.memDispatchIsLoad = [this]() {
    return static_cast<bool>(MemArbiterModule.valid) &&
           static_cast<uint32_t>(MemArbiterModule.op) ==
               static_cast<uint32_t>(Operation::Load);
  };
  LQModule.memDispatch.memDispatchMemIndex = [this]() {
    return static_cast<uint32_t>(MemArbiterModule.memIndex);
  };
  for (int i = 0; i < STORERS_CAP; ++i) {
    LQModule.storeNotifies.snValid[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.valid[i]);
    };
    LQModule.storeNotifies.snStoreTag[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.storeTag[i]);
    };
    LQModule.storeNotifies.snAddr[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.addr[i]);
    };
    LQModule.storeNotifies.snValue[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.value[i]);
    };
    LQModule.storeNotifies.snNBytes[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.nBytes[i]);
    };
    LQModule.storeNotifies.snFoundKnownSame[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.foundKnownSame[i]);
    };
    LQModule.storeNotifies.snKnownTag[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.knownTag[i]);
    };
    LQModule.storeNotifies.snFoundUnknown[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.foundUnknown[i]);
    };
    LQModule.storeNotifies.snUnknownTag[i] = [this, i]() {
      return static_cast<uint32_t>(SQModule.data.unknownTag[i]);
    };
  }
  LQModule.storeNotifies.sanValid = [this]() {
    return static_cast<uint32_t>(SQModule.addr.valid);
  };
  LQModule.storeNotifies.sanStoreTag = [this]() {
    return static_cast<uint32_t>(SQModule.addr.storeTag);
  };
  LQModule.storeNotifies.sanAddr = [this]() {
    return static_cast<uint32_t>(SQModule.addr.addr);
  };
  LQModule.storeNotifies.sanValue = [this]() {
    return static_cast<uint32_t>(SQModule.addr.value);
  };
  LQModule.storeNotifies.sanNBytes = [this]() {
    return static_cast<uint32_t>(SQModule.addr.nBytes);
  };
  LQModule.storeNotifies.sanFoundKnownSame = [this]() {
    return static_cast<uint32_t>(SQModule.addr.foundKnownSame);
  };
  LQModule.storeNotifies.sanKnownTag = [this]() {
    return static_cast<uint32_t>(SQModule.addr.knownTag);
  };
  LQModule.storeNotifies.sanFoundUnknown = [this]() {
    return static_cast<uint32_t>(SQModule.addr.foundUnknown);
  };
  LQModule.storeNotifies.sanUnknownTag = [this]() {
    return static_cast<uint32_t>(SQModule.addr.unknownTag);
  };

  // ---- Wire SQ's Input Wires ----
  SQModule.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  SQModule.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(flushArbiter.SquashTag);
  };
  SQModule.issue.issueValid = [this]() {
    return (!static_cast<bool>(exitCommitted) && static_cast<bool>(IssueArbiterModule.core.valid) &&
            static_cast<bool>(IssueArbiterModule.core.isStore))
               ? 1u
               : 0u;
  };
  SQModule.issue.issueStore = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.isStore);
  };
  SQModule.issue.issueTag = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.robTag);
  };
  SQModule.issue.issueBytes = [this]() {
    const uint32_t n = static_cast<uint32_t>(IssueArbiterModule.core.nBytes);
    return (n == 1 ? 0u : (n == 2 ? 1u : 2u));
  };
  SQModule.memDispatch.memDispatchValid = [this]() {
    return memoryDispatchValid() ? 1u : 0u;
  };
  SQModule.memDispatch.memDispatchIsStore = [this]() {
    return static_cast<bool>(MemArbiterModule.valid) &&
           static_cast<uint32_t>(MemArbiterModule.op) ==
               static_cast<uint32_t>(Operation::Store);
  };
  SQModule.rob.squashSQTailSnapshot = [this]() {
    const RobTag tag = static_cast<uint32_t>(flushArbiter.SquashTag);
    if (!ROBModule.matchesTag(tag))
      return 0u;
    return static_cast<uint32_t>(ROBModule.entry.sqTailSnapshot[robSlot(tag)]);
  };
  SQModule.rob.squashTagMatch = [this]() {
    return ROBModule.matchesTag(static_cast<uint32_t>(flushArbiter.SquashTag))
               ? 1u
               : 0u;
  };
  SQModule.rob.robHeadTag = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.head);
  };
  SQModule.rob.storeWillCommit = [this]() {
    return !static_cast<bool>(exitCommitted) && ROBModule.storeWillCommit() ? 1u : 0u;
  };
  SQModule.agu.isAGUEmpty = [this]() { return static_cast<bool>(recoveryActive) || AGUModule.isEmpty(); };
  SQModule.agu.aguHeadMemIndex = [this]() { return AGUModule.headMemIndex(); };
  SQModule.agu.aguHeadRobTag = [this]() { return AGUModule.headRobTag(); };
  SQModule.agu.aguHeadValue = [this]() { return AGUModule.headValue(); };
  for (int i = 0; i < STORERS_CAP; ++i) {
    SQModule.prf.svWriteValid[i] = [this, i]() {
      if (static_cast<bool>(recoveryActive) || RSModule.isSvFree(i) ||
          !PRFModule.isOperandReady(RSModule.getSvData(i)))
        return 0u;
      if (static_cast<bool>(flushArbiter.needSquash) &&
          !ROB::isOlder(RSModule.getSvRobTag(i),
                        static_cast<uint32_t>(flushArbiter.SquashTag)))
        return 0u;
      return 1u;
    };
    SQModule.prf.svValue[i] = [this, i]() {
      return static_cast<uint32_t>(
          PRFModule.getOperandValue(RSModule.getSvData(i)));
    };
    SQModule.prf.svMemIndex[i] = [this, i]() {
      return static_cast<uint32_t>(RSModule.getSvMemIndex(i));
    };
  }

  // ---- Wire ROB's Input Wires (already-converted consumers feed Output Wire
  // view) ----
  ROBModule.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash) ? 1u : 0u;
  };
  ROBModule.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };
  ROBModule.cdbOfALU.cdbValid = [this]() {
    return AluCDBArbiterModule.valid ? 1u : 0u;
  };
  ROBModule.cdbOfALU.cdbRobTag = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.robTag);
  };
  ROBModule.cdbOfShared.cdbValid = [this]() {
    return SharedCDBModule.valid ? 1u : 0u;
  };
  ROBModule.cdbOfShared.cdbRobTag = [this]() {
    return static_cast<uint32_t>(SharedCDBModule.robTag);
  };
  ROBModule.bru.isBRUEmpty = [this]() { return BRUModule.isEmpty() ? 1u : 0u; };
  ROBModule.bru.bruHeadRobTag = [this]() {
    return BRUModule.isEmpty() ? 0u
                               : static_cast<uint32_t>(BRUModule.headRobTag());
  };
  ROBModule.sq.readyMask = [this]() {
    return static_cast<uint32_t>(SQModule.completion.readyMask);
  };
  ROBModule.sq.epochMask = [this]() {
    return static_cast<uint32_t>(SQModule.completion.epochMask);
  };
  ROBModule.issue.issueValid = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.core.valid);
  };
  ROBModule.issue.entry.type = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.type);
  };
  ROBModule.issue.entry.isCommitReady = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.isCommitReady);
  };
  ROBModule.issue.entry.dest = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.dest);
  };
  ROBModule.issue.entry.halt = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.halt);
  };
  ROBModule.issue.entry.exception = [this]() { return uint32_t(IssueArbiterModule.robEntry.exception); };
  ROBModule.issue.entry.isCall = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.isCall);
  };
  ROBModule.issue.entry.isRet = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.isRet);
  };
  ROBModule.issue.entry.ckptId = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.ckptId);
  };
  ROBModule.issue.entry.predictedPC = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.predictedPC);
  };
  ROBModule.issue.entry.pc = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.pc);
  };
  ROBModule.issue.entry.lqTailSnapshot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.lqTailSnapshot);
  };
  ROBModule.issue.entry.sqTailSnapshot = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.sqTailSnapshot);
  };
  ROBModule.issue.entry.newPhy = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.newPhy);
  };
  ROBModule.issue.entry.oldPhy = [this]() {
    return static_cast<uint32_t>(IssueArbiterModule.robEntry.oldPhy);
  };

  // ---- Wire FlushArbiter's Input Wires (detection lives here, reads
  // bridge/_M_old) ----
  flushArbiter.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash) ? 1u : 0u;
  };
  flushArbiter.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };
  flushArbiter.bru.isBRUEmpty = [this]() {
    return BRUModule.isEmpty() ? 1u : 0u;
  };
  flushArbiter.bru.bruHeadRobTag = [this]() {
    return static_cast<uint32_t>(BRUModule.headRobTag());
  };
  flushArbiter.bru.bruHeadPCResult = [this]() {
    return BRUModule.headPCResult();
  };
  flushArbiter.bru.bruHeadPCFrom = [this]() { return BRUModule.headPCFrom(); };
  // JALR mispredict detection consumes only the ALU bus (control results;
  // loads never produce control), so the LQ bus is not wired here -- an
  // area/port saving over a shared single CDB.
  flushArbiter.cdb.cdbValid = [this]() {
    return AluCDBArbiterModule.valid ? 1u : 0u;
  };
  flushArbiter.cdb.cdbValue = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.value);
  };
  flushArbiter.cdb.cdbRobTag = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.robTag);
  };
  flushArbiter.cdb.cdbIsControl = [this]() {
    return AluCDBArbiterModule.isControl ? 1u : 0u;
  };
  flushArbiter.rob.isROBEmpty = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.isEmpty);
  };
  flushArbiter.rob.robHeadTag = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.head);
  };
  for (int i = 0; i < ROB_CAP; ++i) {
    flushArbiter.rob.robTag[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.tag[i]);
    };
    flushArbiter.rob.robPredictPC[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.predictedPC[i]);
    };
    flushArbiter.rob.robPC[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.pc[i]);
    };
    flushArbiter.rob.robCkptId[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.ckptId[i]);
    };
  }
  // ---- Wire BPU's Input Wires (BRU=EX/投机口, CDB=commit/表口) ----
  BPUModule.squash.needSquash = [this]() {
    return static_cast<bool>(flushArbiter.needSquash) ? 1u : 0u;
  };
  BPUModule.squash.SquashTag = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.SquashTag));
  };
  BPUModule.squash.SquashCkpt = [this]() {
    return static_cast<uint32_t>(static_cast<uint32_t>(flushArbiter.CkptId));
  };
  // BPU training consumes only the ALU bus (its cdb port gates on
  // cdbValid && cdbIsControl; loads never produce control), so the LQ bus is
  // not wired here -- an area/port saving over a shared single CDB.
  BPUModule.cdb.cdbValid = [this]() {
    return AluCDBArbiterModule.valid ? 1u : 0u;
  };
  BPUModule.cdb.cdbValue = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.value);
  };
  BPUModule.cdb.cdbRobTag = [this]() {
    return static_cast<uint32_t>(AluCDBArbiterModule.robTag);
  };
  BPUModule.cdb.cdbIsControl = [this]() {
    return AluCDBArbiterModule.isControl ? 1u : 0u;
  };
  BPUModule.bru.isBRUEmpty = [this]() { return static_cast<bool>(recoveryActive) || BRUModule.isEmpty() ? 1u : 0u; };
  BPUModule.bru.bruHeadRobTag = [this]() {
    return static_cast<uint32_t>(BRUModule.headRobTag());
  };
  BPUModule.bru.bruHeadPCResult = [this]() { return BRUModule.headPCResult(); };
  BPUModule.bru.bruHeadActualTaken = [this]() { return BRUModule.headActualTaken(); };
  BPUModule.bru.bruHeadPCFrom = [this]() { return BRUModule.headPCFrom(); };
  BPUModule.rob.isROBEmpty = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.isEmpty);
  };
  BPUModule.rob.robHeadTag = [this]() {
    return static_cast<uint32_t>(ROBModule.headView.head);
  };
  BPUModule.rob.robNextTag = [this]() {
    return static_cast<uint32_t>(ROBModule.getNextTag());
  };
  BPUModule.rob.robWillCommit = [this]() {
    return ROBModule.willCommit() ? 1u : 0u;
  };
  for (int i = 0; i < ROB_CAP; ++i) {
    BPUModule.rob.robTag[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.tag[i]);
    };
    BPUModule.rob.robPredictPC[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.predictedPC[i]);
    };
    BPUModule.rob.robPC[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.pc[i]);
    };
    BPUModule.rob.robIsCall[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.isCall[i]);
    };
    BPUModule.rob.robIsRet[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.isRet[i]);
    };
    BPUModule.rob.robCkptId[i] = [this, i]() {
      return static_cast<uint32_t>(ROBModule.entry.ckptId[i]);
    };
  }
  // Fetch-context ports: the prediction bundle is produced inside the BPU
  // (wire_output() calls this->predict, single evaluation point), so only
  // the fetch context needs wiring here.
  BPUModule.fetchCtx.pc = [this]() {
    return static_cast<uint32_t>(FetchUnitModule.programCounter);
  };
  BPUModule.fetchCtx.squashNeed = [this]() {
    return static_cast<bool>(flushArbiter.needSquash);
  };
  BPUModule.fetchCtx.haltFetched = [this]() {
    return static_cast<bool>(FetchUnitModule.haltFetched) ||
           static_cast<bool>(FetchUnitModule.haltSignal);
  };
  BPUModule.fetchCtx.fqCanAccept = [this]() {
    return FQModule.canAccept(1u, decodePopCount()) ? 1u : 0u;
  };
  BPUModule.fetchCtx.fqCanAccept2 = [this]() {
    return FQModule.canAccept(2u, decodePopCount()) ? 1u : 0u;
  };
  BPUModule.fetchCtx.memCanAccept = [this]() {
    return !static_cast<bool>(recoveryActive) && !static_cast<bool>(exitCommitted) && static_cast<bool>(initialized) && (!ICacheModule.isRequestFull() || static_cast<bool>(ICacheModule.popConsume)) &&
           (!AXIFetchModule.isRequestFull() || ICacheModule.refillHit(
               static_cast<uint32_t>(FetchUnitModule.programCounter))) &&
           !static_cast<bool>(ICacheModule.readBlocked);
  };
  // fetchInfo: scan the FQ's registered last* (the push record of the
  // PREVIOUS cycle). Mirrors the main tree, which reads pushCache in comb()
  // (also the previous cycle's FQ.tick write). Do NOT use the ICache return
  // bundle directly -- it is the current-cycle value and races FQ push gating.
  BPUModule.fetchInfo.FetchValid = [this]() {
    return scanJump(FQModule.getLastValid(), FQModule.getLastRaw(),
                    FQModule.getLastPC())
                   .valid
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.isFetchCall = [this]() {
    return scanJump(FQModule.getLastValid(), FQModule.getLastRaw(),
                    FQModule.getLastPC())
                   .isCall
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.isFetchRet = [this]() {
    return scanJump(FQModule.getLastValid(), FQModule.getLastRaw(),
                    FQModule.getLastPC())
                   .isRet
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.FetchJALTargetValid = [this]() {
    return scanJump(FQModule.getLastValid(), FQModule.getLastRaw(),
                    FQModule.getLastPC())
                   .jalTargetValid
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.FetchPC = [this]() {
    return scanJump(FQModule.getLastValid(), FQModule.getLastRaw(),
                    FQModule.getLastPC())
        .pc;
  };
  BPUModule.fetchInfo.FetchJALTarget = [this]() {
    return scanJump(FQModule.getLastValid(), FQModule.getLastRaw(),
                    FQModule.getLastPC())
        .jalTarget;
  };
  BPUModule.fetchInfo.FetchValid1 = [this]() {
    return scanJump(FQModule.getLastValid1(), FQModule.getLastRaw1(),
                    FQModule.getLastPC1())
                   .valid
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.isFetchCall1 = [this]() {
    return scanJump(FQModule.getLastValid1(), FQModule.getLastRaw1(),
                    FQModule.getLastPC1())
                   .isCall
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.isFetchRet1 = [this]() {
    return scanJump(FQModule.getLastValid1(), FQModule.getLastRaw1(),
                    FQModule.getLastPC1())
                   .isRet
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.FetchJALTargetValid1 = [this]() {
    return scanJump(FQModule.getLastValid1(), FQModule.getLastRaw1(),
                    FQModule.getLastPC1())
                   .jalTargetValid
               ? 1u
               : 0u;
  };
  BPUModule.fetchInfo.FetchPC1 = [this]() {
    return scanJump(FQModule.getLastValid1(), FQModule.getLastRaw1(),
                    FQModule.getLastPC1())
        .pc;
  };
  BPUModule.fetchInfo.FetchJALTarget1 = [this]() {
    return scanJump(FQModule.getLastValid1(), FQModule.getLastRaw1(),
                    FQModule.getLastPC1())
        .jalTarget;
  };

  // ---- Wire SQ's Output Wires (notify/reply production, single point) ----
  for (int i = 0; i < STORERS_CAP; ++i) {
    SQModule.data.valid[i] = [this, i]() {
      if (static_cast<bool>(recoveryActive) || RSModule.isSvFree(i) ||
          !PRFModule.isOperandReady(RSModule.getSvData(i)))
        return 0u;
      if (static_cast<bool>(flushArbiter.needSquash) &&
          !ROB::isOlder(RSModule.getSvRobTag(i),
                        static_cast<uint32_t>(flushArbiter.SquashTag)))
        return 0u;
      return SQModule.planDataForward(
                         memSlot(RSModule.getSvMemIndex(i)),
                         PRFModule.getOperandValue(RSModule.getSvData(i)))
                     .valid
                 ? 1u
                 : 0u;
    };
    SQModule.data.storeTag[i] = [this, i]() {
      return SQModule
          .planDataForward(memSlot(RSModule.getSvMemIndex(i)),
                           PRFModule.getOperandValue(RSModule.getSvData(i)))
          .storeTag;
    };
    SQModule.data.addr[i] = [this, i]() {
      return SQModule
          .planDataForward(memSlot(RSModule.getSvMemIndex(i)),
                           PRFModule.getOperandValue(RSModule.getSvData(i)))
          .addr;
    };
    SQModule.data.value[i] = [this, i]() {
      return static_cast<uint32_t>(
          SQModule
              .planDataForward(memSlot(RSModule.getSvMemIndex(i)),
                               PRFModule.getOperandValue(RSModule.getSvData(i)))
              .value);
    };
    SQModule.data.foundKnownSame[i] = [this, i]() {
      return SQModule.planDataForward(
                         memSlot(RSModule.getSvMemIndex(i)),
                         PRFModule.getOperandValue(RSModule.getSvData(i)))
                     .foundKnownSame
                 ? 1u
                 : 0u;
    };
    SQModule.data.nBytes[i] = [this, i]() {
      return SQModule
          .planDataForward(memSlot(RSModule.getSvMemIndex(i)),
                           PRFModule.getOperandValue(RSModule.getSvData(i)))
          .nBytes;
    };
    SQModule.data.knownTag[i] = [this, i]() {
      return SQModule
          .planDataForward(memSlot(RSModule.getSvMemIndex(i)),
                           PRFModule.getOperandValue(RSModule.getSvData(i)))
          .knownSameAddressOldestTag;
    };
    SQModule.data.foundUnknown[i] = [this, i]() {
      return SQModule.planDataForward(
                         memSlot(RSModule.getSvMemIndex(i)),
                         PRFModule.getOperandValue(RSModule.getSvData(i)))
                     .foundUnknown
                 ? 1u
                 : 0u;
    };
    SQModule.data.unknownTag[i] = [this, i]() {
      return SQModule
          .planDataForward(memSlot(RSModule.getSvMemIndex(i)),
                           PRFModule.getOperandValue(RSModule.getSvData(i)))
          .unknownOldestTag;
    };
  }
  SQModule.addr.valid = [this]() {
    if (static_cast<bool>(recoveryActive) || AGUModule.isEmpty() || !isStoreMem(AGUModule.headMemIndex()))
      return 0u;
    if (static_cast<bool>(flushArbiter.needSquash) &&
        !ROB::isOlder(AGUModule.headRobTag(),
                      static_cast<uint32_t>(flushArbiter.SquashTag)))
      return 0u;
    return SQModule.planAddressForward(memSlot(AGUModule.headMemIndex()),
                                       AGUModule.headValue(), AGUModule.headRobTag())
                   .valid
               ? 1u
               : 0u;
  };
  SQModule.addr.storeTag = [this]() {
    return SQModule
        .planAddressForward(memSlot(AGUModule.headMemIndex()),
                            AGUModule.headValue(), AGUModule.headRobTag())
        .storeTag;
  };
  SQModule.addr.addr = [this]() {
    return SQModule
        .planAddressForward(memSlot(AGUModule.headMemIndex()),
                            AGUModule.headValue(), AGUModule.headRobTag())
        .addr;
  };
  SQModule.addr.value = [this]() {
    return static_cast<uint32_t>(
        SQModule
            .planAddressForward(memSlot(AGUModule.headMemIndex()),
                                AGUModule.headValue(), AGUModule.headRobTag())
            .value);
  };
  SQModule.addr.foundKnownSame = [this]() {
    return SQModule.planAddressForward(memSlot(AGUModule.headMemIndex()),
                                       AGUModule.headValue(), AGUModule.headRobTag())
                   .foundKnownSame
               ? 1u
               : 0u;
  };
  SQModule.addr.nBytes = [this]() {
    return SQModule
        .planAddressForward(memSlot(AGUModule.headMemIndex()),
                            AGUModule.headValue(), AGUModule.headRobTag())
        .nBytes;
  };
  SQModule.addr.knownTag = [this]() {
    return SQModule
        .planAddressForward(memSlot(AGUModule.headMemIndex()),
                            AGUModule.headValue(), AGUModule.headRobTag())
        .knownSameAddressOldestTag;
  };
  SQModule.addr.foundUnknown = [this]() {
    return SQModule.planAddressForward(memSlot(AGUModule.headMemIndex()),
                                       AGUModule.headValue(), AGUModule.headRobTag())
                   .foundUnknown
               ? 1u
               : 0u;
  };
  SQModule.addr.unknownTag = [this]() {
    return SQModule
        .planAddressForward(memSlot(AGUModule.headMemIndex()),
                            AGUModule.headValue(), AGUModule.headRobTag())
        .unknownOldestTag;
  };
  SQModule.reply.valid = [this]() {
    if (static_cast<bool>(recoveryActive) || AGUModule.isEmpty() || isStoreMem(AGUModule.headMemIndex()))
      return 0u;
    auto r = SQModule.replyToLoadRequest(
        AGUModule.headValue(), LQModule.getNBytes(memSlot(AGUModule.headMemIndex())),
        AGUModule.headRobTag());
    return r.valid ? 1u : 0u;
  };
  SQModule.reply.value = [this]() {
    auto r = SQModule.replyToLoadRequest(
        AGUModule.headValue(), LQModule.getNBytes(memSlot(AGUModule.headMemIndex())),
        AGUModule.headRobTag());
    return r.valid ? static_cast<uint32_t>(r.value) : 0u;
  };
}

OperandWriteback CPU::grantedWritebacks() const {
  OperandWriteback ports{};
  if (!bool(recoveryActive) && !bool(exitCommitted)) {
    const auto &alu = PRFModule.cdbOfALU;
    const auto &shared = PRFModule.cdbOfShared;
    const uint8_t aluPhy = uint32_t(alu.cdbNewPhy);
    const uint8_t sharedPhy = uint32_t(shared.cdbNewPhy);
    ports[0] = {bool(alu.cdbValid) && !bool(alu.cdbIsControl) && aluPhy != InvalidPhy,
                aluPhy, int32_t(uint32_t(alu.cdbValue))};
    ports[1] = {bool(shared.cdbValid) && sharedPhy != InvalidPhy,
                sharedPhy, int32_t(uint32_t(shared.cdbValue))};
  }
  return ports;
}

unsigned CPU::decodePopCount() const {
  const bool issueValid = static_cast<bool>(IssueArbiterModule.core.valid);
  const unsigned cap = DecodeUnitModule.pushCapacity(issueValid);
  const unsigned avail = FQModule.count();
  unsigned n = avail < cap ? avail : cap;
  const unsigned limit = static_cast<unsigned>(DECODE_WIDTH);
  if (n > limit)
    n = limit;
  return n;
}

bool CPU::exitRetiring() const {
  bool retiring = false;
  for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane)
    retiring |= bool(ROBModule.retirement[lane].valid) && bool(ROBModule.retirement[lane].exits);
  return retiring;
}

bool CPU::memoryDispatchValid() const {
  return static_cast<bool>(MemArbiterModule.valid) && (!static_cast<bool>(exitCommitted) ||
      (static_cast<Operation>(static_cast<uint32_t>(MemArbiterModule.op)) == Operation::Store &&
       !SQModule.isEmpty() && SQModule.isCommitted(SQModule.getHead() & SQ_MASK)));
}

void CPU::clock(bool shuffle) {
  initialized <= 1;
  const bool frozen = static_cast<bool>(exitCommitted);
  const bool recovering = static_cast<bool>(recoveryActive);
  if (!frozen) {
    if (static_cast<bool>(flushArbiter.needSquash)) {
      const RobTag head = static_cast<uint32_t>(ROBModule.headView.head);
      recoveryActive <= 1;
      recoveryBeat <= 0;
      recoveryOffset <= 0;
      recoveryNextTag <= head;
      recoveryBoundary <= static_cast<uint32_t>(flushArbiter.SquashTag);
      recoveryCount <= ((ROBModule.getNextTag() - head) & ROB_TAG_MASK);
    } else if (recovering) {
      RobTag next = static_cast<uint32_t>(recoveryNextTag);
      for (unsigned lane = 0; lane < RECOVERY_WIDTH; ++lane) next = robNextTag(next);
      recoveryNextTag <= next;
      recoveryOffset <= (static_cast<uint32_t>(recoveryOffset) + RECOVERY_WIDTH);
      if (uint32_t(recoveryOffset) + RECOVERY_WIDTH >= uint32_t(recoveryCount)) recoveryActive <= 0;
      else recoveryBeat <= (static_cast<uint32_t>(recoveryBeat) + 1u);
    }
  }
  if (exitRetiring()) exitCommitted <= 1;
  if (frozen || recovering) {
    ++dcpu.cycles;
    AXIFetchModule.work(); SQModule.work(); DCacheModule.work();
    AXIDataModule.work(); AXIBusModule.work(); SlaveModule.work();
    if (recovering && !frozen) {
      ICacheModule.work(); LQModule.work(); PRFModule.work(); BPUModule.work(); RATModule.work();
    }
    // Frozen sequential state holds, but ALL Wire caches still invalidate at
    // this edge. No transport/SRAM response is allowed to bypass sync().
    dark::ModuleBase *modules[] = {&FetchUnitModule, &AXIFetchModule, &ICacheModule, &FQModule,
        &LQModule, &SQModule, &ROBModule, &PRFModule, &ALUModule, &MULModule, &DIVModule,
        &AGUModule, &BRUModule, &BPUModule, &DCacheModule, &AXIDataModule, &AXIBusModule,
        &SlaveModule, &RSModule, &RATModule, &AluCDBArbiterModule, &LqCDBArbiterModule,
        &MulCDBModule, &DivCDBModule, &SharedCDBModule, &MemArbiterModule, &DispatchArbiterModule,
        &IssueArbiterModule, &flushArbiter, &DecodeUnitModule};
    for (auto *module : modules) module->sync();
  } else if (shuffle)
    dcpu.run_once_shuffle();
  else
    dcpu.run_once();
  // The CPU's own registers are not registered modules. Commit them exactly
  // once at the same edge, after every module has completed work().
  dark::sync_member(initialized);
  dark::sync_member(exitCommitted);
  dark::sync_member(recoveryActive);
  dark::sync_member(recoveryBeat);
  dark::sync_member(recoveryOffset);
  dark::sync_member(recoveryNextTag);
  dark::sync_member(recoveryBoundary);
  dark::sync_member(recoveryCount);
}

void CPU::run(bool shuffle) {
  bool finish = false;
  uint64_t initCycles = 0;
  while (!static_cast<bool>(initialized)) { clock();++initCycles; }
  // host-only: core IPC freezes on the HALT-marker commit. The marker itself
  // is not an executed architectural instruction, while its commit cycle is
  // part of the measured interval.
  uint64_t retired = 0;
  uint64_t ipcRetired = 0;
  uint64_t ipcCycles = 0;
  bool ipcFrozen = false;
  while (!finish) {
    // Retirement samples OLD state. Stop on the HALT commit edge, with no
    // SQ/cache/DMEM drain or extra idle termination tick.
    bool s_halt = ROBModule.isHaltCommitted();
    const uint32_t headBefore = static_cast<uint32_t>(ROBModule.headView.head);
    const bool haltBefore = s_halt;
    const bool exitNow = exitRetiring();
    clock(shuffle);
    const uint32_t headAfter = static_cast<uint32_t>(ROBModule.headView.head);
    const bool haltAfter = ROBModule.isHaltCommitted();
    if (ROBModule.isTrapCommitted()) throw std::runtime_error("illegal instruction committed");
    unsigned committed = 0;
    RobTag cursor = headBefore;
    for (unsigned lane = 0; lane < COMMIT_WIDTH; ++lane) {
      if (cursor != headAfter) { ++committed; cursor = robNextTag(cursor); }
    }
    assert(cursor == headAfter);
    const bool haltCommitted = !haltBefore && haltAfter;
    assert(!haltCommitted || committed != 0);
    if (!ipcFrozen) {
      retired += committed - static_cast<uint32_t>(haltCommitted);
      if (haltCommitted || exitNow) {
        ipcRetired = retired;
        ipcCycles = dcpu.cycles - initCycles;
        ipcFrozen = true;
      }
    }
    // User contract: stop at HALT commit, without SQ/cache/DMEM drain.
    finish = haltAfter || SlaveModule.isDone();
  }
  // host-only: the three debug::print blocks below are end-of-run reports for
  // the human (double percentage math + stdio formatting). They are not part
  // of the modelled datapath, which is why `*` and `/` are legal here.
  if (debug::enabled(debug::TOPIC_ICACHE)) {
    uint64_t ih = ICacheModule.statHits;
    uint64_t im = ICacheModule.statMisses;
    uint64_t it = ih + im;
    debug::print("icache: hits=%llu misses=%llu total=%llu hit-rate=%.2f%%\n",
                 ih, im, it,
                 it ? 100.0 * static_cast<double>(ih) / static_cast<double>(it)
                    : 0.0);
    uint64_t dh = DCacheModule.statHits;
    uint64_t dm = DCacheModule.statMisses;
    uint64_t dt = dh + dm;
    debug::print("dcache: hits=%llu misses=%llu total=%llu hit-rate=%.2f%%\n",
                 dh, dm, dt,
                 dt ? 100.0 * static_cast<double>(dh) / static_cast<double>(dt)
                    : 0.0);
  }
  if (debug::enabled(debug::TOPIC_CLOCK)) {
    debug::print("clock: %llu\n", dcpu.cycles - initCycles);
    debug::print("timing: init=%llu core=%llu drain=%llu total=%llu\n",
                 initCycles, ipcCycles, dcpu.cycles - initCycles - ipcCycles, dcpu.cycles - initCycles);
    debug::print("ipc: %.6f retired=%llu cycles=%llu\n",
                 ipcCycles ? static_cast<double>(ipcRetired) /
                                 static_cast<double>(ipcCycles)
                           : 0.0,
                 ipcRetired, ipcCycles);
  }
  if (debug::enabled(debug::TOPIC_BRANCH))
    debug::print("branch: %llu/%llu correct (%.2f%%)\n",
                 BPUModule.getBranchCorrect(), BPUModule.getBranchTotal(),
                 BPUModule.getBranchTotal()
                     ? 100.0 * BPUModule.getBranchCorrect() /
                           BPUModule.getBranchTotal()
                     : 0.0);
  const uint32_t halt_x10 = SlaveModule.isDone() ? SlaveModule.exitResult() : static_cast<uint32_t>(
      PRFModule.getValue(RATModule.readRAT_PRF(ROBModule.getHaltRd())));
  const bool print_full_x10 = std::getenv("RESULT_FULL") != nullptr;
  std::cout << std::dec << ((print_full_x10 || SlaveModule.isDone()) ? halt_x10 : (halt_x10 & 0xFF))
            << std::endl;
}
