#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "AGU.hpp"
#include "ALU.hpp"
#include "AXIBus.hpp"
#include "AXIData.hpp"
#include "AXIFetch.hpp"
#include "DynamicArbiter.hpp"
#include "StaticArbiter.hpp"
#include "BRU.hpp"
#include "BPU.hpp"
#include "CDB.hpp"
#include "DCache.hpp"
#include "Decoder.hpp"
#include "InstructBuffer.hpp"
#include "FetchUnit.hpp"
#include "ICache.hpp"
#include "LQ.hpp"
#include "MUL.hpp"
#include "DIV.hpp"
#include "SQ.hpp"
#include "Slave.hpp"
#include "Memory.hpp"
#include "PRF.hpp"
#include "RAT.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "cpu.h"
#include "common.h"
#include <cstring>

class CPU {
 private:
  void clock(bool shuffle = false);
  // Framework runner: run_once() = ++cycles -> registered work() sequence ->
  // sync_all(); run_once_shuffle() randomizes the work order (order
  // independence is structural: every cross-module read is a Wire tied to
  // the committed _M_old view). Registration order mirrors the former
  // hand-written run loop exactly.
  dark::CPU dcpu;
  RSUnit RSModule;
  RAT RATModule;
  ROB ROBModule;
  ALU ALUModule;
  MUL MULModule;
  DIV DIVModule;
  AGU AGUModule;
  BRU BRUModule;
  LQ LQModule;
  SQ SQModule;
  InstructBuffer FQModule;
  ICache ICacheModule;
  DecodeUnit DecodeUnitModule;
  PRF PRFModule;
  AXIData AXIDataModule;
  DCache DCacheModule;
  AXIFetch AXIFetchModule;
  AXIBus AXIBusModule;
  Slave SlaveModule;
  BPU BPUModule;
  FlushArbiter flushArbiter;
  AluCDB AluCDBArbiterModule;
  LqCDB LqCDBArbiterModule;
  MulCDB MulCDBModule;
  DivCDB DivCDBModule;
  SharedCDB SharedCDBModule;
  MemArbiter MemArbiterModule;
  DispatchArbiter DispatchArbiterModule;
  IssueArbiter IssueArbiterModule;
  FetchUnit FetchUnitModule;
  // Top-level state is written once before work(), committed after all work().
  Register<1> initialized;
  Register<1> exitCommitted;
  Register<1> recoveryActive;
  Register<2> recoveryBeat;
  Register<ROB_TAG_WIDTH> recoveryOffset, recoveryCount, recoveryNextTag, recoveryBoundary;
  bool exitRetiring() const;
  bool memoryDispatchValid() const;
  bool frontendCanRun() const;
  OperandWriteback grantedWritebacks() const;
  // 0..DECODE_WIDTH: decoded uops moved FQ -> IQ this cycle (min of FQ
  // availability, IQ push capacity after the single-issue pop, and width).
  unsigned decodePopCount() const;
public:
  CPU(const Memory &mem);
  void wire();
  void run(bool shuffle = false);
};

#endif // CPU_HPP
