#include "../include/StaticArbiter.hpp"
#include <cassert>
#include <cstdint>

// ---- MemArbiter: stateless combinational memory-request arbiter ----
// Store priority + load disambiguation semantics are verbatim from the
// inlined MemRequestArbiter that used to live in CPU::comb().
bool MemArbiter::storeSelected() const {
  if (static_cast<bool>(sqEmpty) || static_cast<bool>(dmemBusy))
    return false;
  RobTag storeTag = static_cast<uint32_t>(sqHeadRobTag);
  RobTag head = static_cast<uint32_t>(robHeadTag);
  bool committed = static_cast<bool>(sqHeadCommitted);
  bool atHeadReady = !committed &&
                     static_cast<bool>(robStoreWillCommit);
  return committed || atHeadReady;
}

bool MemArbiter::loadSelected() const {
  if (storeSelected() || static_cast<bool>(dmemBusy) ||
      !static_cast<bool>(loadValid) || !static_cast<bool>(loadCanSendToCache))
    return false;
  return !static_cast<bool>(squashNeed) ||
         ROB::isOlder(static_cast<uint32_t>(loadRobTag),
                      static_cast<uint32_t>(squashTag));
}

void MemArbiter::wire_output() {
  valid = [this]() -> uint32_t {
    return (storeSelected() || loadSelected()) ? 1u : 0u;
  };
  op = [this]() -> uint32_t {
    if (storeSelected())
      return static_cast<uint32_t>(Operation::Store);
    if (loadSelected())
      return static_cast<uint32_t>(Operation::Load);
    return 0u;
  };
  // Load value travels through the CDB return path, not the request.
  value = [this]() -> uint32_t {
    return storeSelected() ? static_cast<uint32_t>(sqHeadValue) : 0u;
  };
  address = [this]() -> uint32_t {
    if (storeSelected())
      return static_cast<uint32_t>(sqHeadAddr);
    if (loadSelected())
      return static_cast<uint32_t>(loadAddr);
    return 0u;
  };
  // Store is never signed (verbatim memDispatchSigned = false).
  isSigned = [this]() -> uint32_t {
    return loadSelected() ? static_cast<uint32_t>(loadIsSigned) : 0u;
  };
  nEnc = [this]() -> uint32_t {
    if (storeSelected())
      return static_cast<uint32_t>(sqHeadNEnc);
    if (loadSelected())
      return static_cast<uint32_t>(loadNEnc);
    return 0u;
  };
  robTag = [this]() -> uint32_t {
    if (storeSelected())
      return static_cast<uint32_t>(sqHeadRobTag);
    if (loadSelected())
      return static_cast<uint32_t>(loadRobTag);
    return 0u;
  };
  memIndex = [this]() -> uint32_t {
    if (storeSelected())
      return static_cast<uint32_t>(MEM_STORE_BIT) |
             static_cast<uint32_t>(sqHead);
    if (loadSelected())
      return static_cast<uint32_t>(loadIndex);
    return 0u;
  };
}

// ---- DispatchArbiter: stateless combinational dispatch (RTL style) ----
// Ports are buses; the arbiter indexes the PRF ready bitmap with per-slot
// source tags itself. Selection is a folded priority chain; a grant gates on
// destination admission and the squash point (squash suppresses valid only --
// the selected idx/tag stay driven, consumers gate on valid).
bool DispatchArbiter::readyOf(uint32_t tag, bool bypass) const {
  return tag == InvalidPhy || (bypass ? static_cast<bool>(bypassReady[tag])
                                      : static_cast<bool>(prdReady[tag]));
}

template <std::size_t N>
DispatchArbiter::WinResult DispatchArbiter::selectOldest(
    const std::array<Wire<1>, N> &busy,
    const std::array<Wire<PHY_TAG_WIDTH>, N> &src1Tag,
    const std::array<Wire<PHY_TAG_WIDTH>, N> &src2Tag,
    const std::array<Wire<ROB_TAG_WIDTH>, N> &tags, bool bypass) const {
  WinResult w{false, 0, 0};
  for (uint32_t i = 0; i < N; ++i) {
    bool req = static_cast<bool>(busy[i]) &&
                readyOf(static_cast<uint32_t>(src1Tag[i]), bypass) &&
                readyOf(static_cast<uint32_t>(src2Tag[i]), bypass);
    RobTag tag = static_cast<uint32_t>(tags[i]);
    if (req && (!w.v || ROB::isOlder(tag, w.tag)))
      w = {true, tag, i};
  }
  return w;
}

DispatchArbiter::WinResult DispatchArbiter::aluSelect() const {
  return selectOldest(intBusy, intSrc1Tag, intSrc2Tag, intRobTag, true);
}

DispatchArbiter::WinResult DispatchArbiter::bruSelect() const {
  return selectOldest(brBusy, brSrc1Tag, brSrc2Tag, brRobTag, true);
}

DispatchArbiter::WinResult DispatchArbiter::mulSelect() const {
  return selectOldest(mulBusy, mulSrc1Tag, mulSrc2Tag, mulRobTag);
}

DispatchArbiter::WinResult DispatchArbiter::divSelect() const {
  return selectOldest(divBusy, divSrc1Tag, divSrc2Tag, divRobTag);
}

// load[0..3] ++ sa[0..3] concatenated into a single 8-slot pass -- verbatim
// iteration order and fold condition of the former software double loop
// (store-addr carries a single operand; equal tags keep the earlier slot).
DispatchArbiter::WinResult DispatchArbiter::aguSelect() const {
  WinResult w{false, 0, 0};
  for (int i = 0; i < LOADRS_CAP + STORERS_CAP; ++i) {
    bool req;
    RobTag tag;
    if (i < LOADRS_CAP) {
      req = static_cast<bool>(loadBusy[i]) &&
            readyOf(static_cast<uint32_t>(loadSrc1Tag[i]), true) &&
            readyOf(static_cast<uint32_t>(loadSrc2Tag[i]), true);
      tag = static_cast<uint32_t>(loadRobTag[i]);
    } else {
      int j = i - LOADRS_CAP;
      req = static_cast<bool>(saBusy[j]) &&
            readyOf(static_cast<uint32_t>(saSrc1Tag[j]));
      tag = static_cast<uint32_t>(saRobTag[j]);
    }
    if (req && (!w.v || ROB::isOlder(tag, w.tag)))
      w = {true, tag, static_cast<uint32_t>(i)};
  }
  return w;
}

void DispatchArbiter::wire_output() {
  // win = dest_accept & selected; grant = win & ~squash_kill. Fields (idx/tag/
  // type) are driven by win (not grant) -- verbatim: the former code wrote
  // them before flipping valid off under squash.
  alu.valid = [this]() -> uint32_t {
    WinResult w = aluSelect();
    return static_cast<bool>(aluCanAccept) && w.v &&
                   (!static_cast<bool>(squashNeed) ||
                    ROB::isOlder(w.tag, static_cast<uint32_t>(squashTag)))
               ? 1u
               : 0u;
  };
  alu.rsIndex = [this]() -> uint32_t {
    WinResult w = aluSelect();
    return static_cast<bool>(aluCanAccept) && w.v ? w.idx : 0u;
  };
  alu.robTag = [this]() -> uint32_t {
    WinResult w = aluSelect();
    return static_cast<bool>(aluCanAccept) && w.v ? w.tag : 0u;
  };
  agu.valid = [this]() -> uint32_t {
    WinResult w = aguSelect();
    return static_cast<bool>(aguCanAccept) && w.v &&
                   (!static_cast<bool>(squashNeed) ||
                    ROB::isOlder(w.tag, static_cast<uint32_t>(squashTag)))
               ? 1u
               : 0u;
  };
  agu.rsIndex = [this]() -> uint32_t {
    WinResult w = aguSelect();
    return static_cast<bool>(aguCanAccept) && w.v
               ? (w.idx < LOADRS_CAP ? w.idx : w.idx - LOADRS_CAP)
               : 0u;
  };
  agu.robTag = [this]() -> uint32_t {
    WinResult w = aguSelect();
    return static_cast<bool>(aguCanAccept) && w.v ? w.tag : 0u;
  };
  agu.rsType = [this]() -> uint32_t {
    WinResult w = aguSelect();
    return static_cast<bool>(aguCanAccept) && w.v
               ? (w.idx < LOADRS_CAP ? static_cast<uint32_t>(RSType::Load)
                                     : static_cast<uint32_t>(RSType::StoreAddr))
               : static_cast<uint32_t>(RSType::Integer);
  };
  bru.valid = [this]() -> uint32_t {
    WinResult w = bruSelect();
    return !static_cast<bool>(bruFull) && w.v &&
                   (!static_cast<bool>(squashNeed) ||
                    ROB::isOlder(w.tag, static_cast<uint32_t>(squashTag)))
               ? 1u
               : 0u;
  };
  bru.rsIndex = [this]() -> uint32_t {
    WinResult w = bruSelect();
    return !static_cast<bool>(bruFull) && w.v ? w.idx : 0u;
  };
  bru.robTag = [this]() -> uint32_t {
    WinResult w = bruSelect();
    return !static_cast<bool>(bruFull) && w.v ? w.tag : 0u;
  };
  mul.valid = [this]() -> uint32_t {
    WinResult w = mulSelect();
    return static_cast<bool>(mulCanAccept) && w.v &&
                   (!static_cast<bool>(squashNeed) ||
                    ROB::isOlder(w.tag, static_cast<uint32_t>(squashTag)))
               ? 1u
               : 0u;
  };
  mul.rsIndex = [this]() -> uint32_t {
    WinResult w = mulSelect();
    return static_cast<bool>(mulCanAccept) && w.v ? w.idx : 0u;
  };
  mul.robTag = [this]() -> uint32_t {
    WinResult w = mulSelect();
    return static_cast<bool>(mulCanAccept) && w.v ? w.tag : 0u;
  };
  // DIV channel: same fold as the MUL channel over the divideRS pool, but the
  // destination gate is the unit's own canAccept() instead of an isFull -- the
  // divider is a single iterative unit with no output buffer, so it must not be
  // handed new work while a finished result is still waiting to be broadcast
  // (verbatim from the main tree's div channel).
  div.valid = [this]() -> uint32_t {
    WinResult w = divSelect();
    return static_cast<bool>(divAccept) && w.v &&
                   (!static_cast<bool>(squashNeed) ||
                    ROB::isOlder(w.tag, static_cast<uint32_t>(squashTag)))
               ? 1u
               : 0u;
  };
  div.rsIndex = [this]() -> uint32_t {
    WinResult w = divSelect();
    return static_cast<bool>(divAccept) && w.v ? w.idx : 0u;
  };
  div.robTag = [this]() -> uint32_t {
    WinResult w = divSelect();
    return static_cast<bool>(divAccept) && w.v ? w.tag : 0u;
  };
}

namespace {
// Verbatim from the former IssueArbiter::decodeOp: pure multi-level switch
// over the decoded head fields (comb-helper return exemption applies).
Operation decodeOp(uint32_t type, uint32_t opcode, uint32_t funct3,
                   uint32_t funct7) {
  if (type == static_cast<uint32_t>(RISC_V::R)) {
    int link_funct = (static_cast<int>(funct3) << 7) | static_cast<int>(funct7);
    switch (link_funct) {
    case 0b0000000000:
      return Operation::ADD;
    case 0b0000100000:
      return Operation::SUB;
    case 0b0010000000:
      return Operation::SL;
    case 0b0100000000:
      return Operation::SLT;
    case 0b0110000000:
      return Operation::SLTU;
    case 0b1000000000:
      return Operation::XOR;
    case 0b1010000000:
      return Operation::SRL;
    case 0b1010100000:
      return Operation::SRA;
    case 0b1100000000:
      return Operation::OR;
    case 0b1110000000:
      return Operation::AND;
    default:
      return Operation::OP_INVALID;
    }
  }
  if (type == static_cast<uint32_t>(RISC_V::M)) {
    // funct7 == 0b0000001 (guaranteed by the decoder's M classification).
    // funct3 0..3 are the multiply ops (multiplyRS), funct3 4..7 the DIV/REM
    // family (divideRS); the divider is a real iterative SRT unit, so all
    // eight funct3 values decode (mirrors the main tree).
    switch (funct3) {
    case 0b000:
      return Operation::MUL;
    case 0b001:
      return Operation::MULH;
    case 0b010:
      return Operation::MULHSU;
    case 0b011:
      return Operation::MULHU;
    case 0b100:
      return Operation::DIV;
    case 0b101:
      return Operation::DIVU;
    case 0b110:
      return Operation::REM;
    case 0b111:
      return Operation::REMU;
    default:
      return Operation::OP_INVALID;
    }
  }
  if (type == static_cast<uint32_t>(RISC_V::I)) {
    if (opcode == 0b0000011)
      return Operation::Load;
    if (opcode == 0b1100111)
      return Operation::JALR;
    switch (funct3) {
    case 0b000:
      return Operation::ADD;
    case 0b010:
      return Operation::SLT;
    case 0b011:
      return Operation::SLTU;
    case 0b100:
      return Operation::XOR;
    case 0b110:
      return Operation::OR;
    case 0b111:
      return Operation::AND;
    default:
      return Operation::OP_INVALID;
    }
  }
  if (type == static_cast<uint32_t>(RISC_V::Istar)) {
    if (funct3 == 1)
      return Operation::SL;
    if (funct3 == 5)
      return (funct7 == 0) ? Operation::SRL : Operation::SRA;
    return Operation::OP_INVALID;
  }
  if (type == static_cast<uint32_t>(RISC_V::S)) {
    return Operation::Store;
  }
  if (type == static_cast<uint32_t>(RISC_V::B)) {
    switch (funct3) {
    case 0b000:
      return Operation::EQ;
    case 0b001:
      return Operation::NE;
    case 0b100:
      return Operation::LT;
    case 0b101:
      return Operation::GE;
    case 0b110:
      return Operation::LTU;
    case 0b111:
      return Operation::GEU;
    default:
      return Operation::OP_INVALID;
    }
  }
  if (type == static_cast<uint32_t>(RISC_V::U)) {
    if (opcode == 0b0010111)
      return Operation::AUIPC;
    if (opcode == 0b0110111)
      return Operation::LUI;
    return Operation::OP_INVALID;
  }
  if (type == static_cast<uint32_t>(RISC_V::J)) {
    return Operation::JALR;
  }
  return Operation::OP_INVALID;
}
} // namespace

// Raw issue class of the decoded head, mirroring the former build() switch
// (pre-resource-check). Note: I/U opcodes outside the handled families fall
// to none (0), exactly like the original switch's fall-through default.
uint32_t IssueArbiter::issueClass(const IssueArbInputDec &d) const {
  const uint32_t type = static_cast<uint32_t>(d.type);
  const uint32_t opcode = static_cast<uint32_t>(d.opcode);
  if (type == static_cast<uint32_t>(RISC_V::R))
    return 1u;
  if (type == static_cast<uint32_t>(RISC_V::M)) {
    const Operation mOp =
        decodeOp(type, opcode, static_cast<uint32_t>(d.funct3),
                 static_cast<uint32_t>(d.funct7));
    return (mOp == Operation::DIV || mOp == Operation::DIVU ||
            mOp == Operation::REM || mOp == Operation::REMU)
               ? 9u
               : 8u;
  }
  if (type == static_cast<uint32_t>(RISC_V::I)) {
    if (opcode == 0x13u)
      return static_cast<bool>(d.isHalt) ? 2u : 1u;
    if (opcode == 0x67u)
      return 1u;
    if (opcode == 0x03u)
      return 3u;
    return 0u;
  }
  if (type == static_cast<uint32_t>(RISC_V::Istar))
    return 1u;
  if (type == static_cast<uint32_t>(RISC_V::S))
    return 4u;
  if (type == static_cast<uint32_t>(RISC_V::B))
    return 5u;
  if (type == static_cast<uint32_t>(RISC_V::U))
    return (opcode == 0x17u || opcode == 0x37u) ? 6u : 0u;
  if (type == static_cast<uint32_t>(RISC_V::J))
    return 6u;
  return 7u; // RV_INVALID
}

// Verbatim from the former resolveSrc: register source -> Operand. Not
// superset-safe (the P0-dead assert), so every caller gates it on its
// branch actually winning -- same shape as the former per-branch call sites.
Operand IssueArbiter::resolveSrc(const IssueArbOperandView &v) const {
  if (static_cast<bool>(v.ready)) // x0: constant zero
    return {.tag = InvalidPhy,
            .imm = static_cast<int32_t>(static_cast<uint32_t>(v.value))};
  assert(static_cast<uint32_t>(v.phy) != InvalidPhy);
  return {.tag = static_cast<int>(static_cast<uint32_t>(v.phy)), .imm = 0};
}

Operand IssueArbiter::resolveSrcFwd(const IssueArbOperandView &v,
                                    uint32_t srcReg, bool fwdAlloc,
                                    uint32_t fwdDest, uint32_t fwdPhy) const {
  // Intra-group RAW: lane1's source reads lane0's destination -> forward to
  // lane0's new physical register (tag, not ready) instead of the old RAT.
  if (fwdAlloc && srcReg == fwdDest)
    return {.tag = static_cast<int>(fwdPhy), .imm = 0};
  return resolveSrc(v);
}

const WideIssueDecision &IssueArbiter::wideDecision(unsigned requested) const {
  if (!wideCache.valid) {
    wideCache.lanes = {};
    auto remaining = std::array<uint32_t, 7>{};
    for (unsigned pool = 0; pool < 7; ++pool)
      remaining[pool] = uint32_t(availableRows[pool]);
    unsigned physical = 0, loads = 0, stores = 0;
    RobTag tag = uint32_t(rob.nextTag);
    bool open = !bool(squashNeed);
    for (unsigned lane = 0; lane < 4; ++lane) {
      const auto &d = wideDec[lane];
      auto &p = wideCache.lanes[lane];
      const unsigned cls = issueClass(d);
      const bool writes = bool(d.allocDest) && uint32_t(d.rd) != 0 &&
                          (cls == 1 || cls == 3 || cls == 6 || cls == 8 || cls == 9);
      const int pool = cls == 1 || cls == 6 ? 0 : cls == 8 ? 1 :
                       cls == 9 ? 2 : cls == 5 ? 3 : cls == 3 ? 4 : cls == 4 ? 5 : -1;
      bool can = open && !bool(d.isEmpty) && cls != 0 && lane < uint32_t(robCredits) &&
                 (!writes || physical < uint32_t(physicalCredits));
      if (pool >= 0) can &= remaining[pool] != 0;
      if (cls == 3) can &= loads < uint32_t(loadCredits);
      if (cls == 4) can &= stores < uint32_t(storeCredits) && remaining[6] != 0;
      open = can && cls != 2;
      if (can) {
        p.valid = true;
        p.cls = cls;
        p.op = uint32_t(decodeOp(uint32_t(d.type), uint32_t(d.opcode),
                                 uint32_t(d.funct3), uint32_t(d.funct7)));
        p.rd = uint32_t(d.rd);
        p.pc = uint32_t(d.pc);
        p.predictedPC = uint32_t(d.predictedPC);
        p.ckptId = uint32_t(d.ckptId);
        p.imm = uint32_t(d.imm);
        p.tag = tag;
        p.alloc = writes;
        p.phy = writes ? uint32_t(wideFreePhy[physical]) : 0;
        p.oldPhy = writes ? uint32_t(wideRat[lane].rdOldPhy) : 0;
        p.control = (cls == 1 && uint32_t(d.opcode) == 0x67) ||
                    (cls == 6 && uint32_t(d.type) == uint32_t(RISC_V::J));
        p.call = p.control && (p.rd == 1 || p.rd == 5);
        p.ret = cls == 1 && p.control && (uint32_t(d.rs1) == 1 || uint32_t(d.rs1) == 5);
        p.intImmediate = uint32_t(d.type) != uint32_t(RISC_V::R);
        p.upperPC = cls == 6 && (p.control || uint32_t(d.opcode) == 0x17);
        // The latest *actual* writer wins for each source and for WAW oldPhy.
        auto source = [&](const IssueArbOperandView &view, unsigned reg) {
          Operand operand = resolveSrc(view);
          for (unsigned older = 0; older < lane; ++older) {
            const auto &previous = wideCache.lanes[older];
            if (previous.alloc && reg != 0 && reg == previous.rd)
              operand = previous.control ? Operand{InvalidPhy, int32_t(previous.pc + 4u)}
                                         : Operand{int(previous.phy), 0};
          }
          return operand;
        };
        p.s1 = source(wideRat[lane].s1, uint32_t(d.rs1));
        p.s2 = source(wideRat[lane].s2, uint32_t(d.rs2));
        for (unsigned older = 0; older < lane; ++older) {
          const auto &previous = wideCache.lanes[older];
          if (writes && previous.alloc && p.rd == previous.rd) p.oldPhy = previous.phy;
        }
        auto takeRow = [&](unsigned which) {
          unsigned slot = 0;
          bool found = false;
          for (unsigned row = 0; row < 32; ++row) {
            if (!found && ((remaining[which] >> row) & 1u)) {
              slot = row;
              found = true;
            }
          }
          p.slots[which] = slot;
          remaining[which] &= ~(1u << slot);
        };
        if (pool >= 0) takeRow(unsigned(pool));
        if (cls == 4) takeRow(6);
        if (cls == 3) p.memIndex = (uint32_t(lsq.lqTail) + loads) & LQ_MASK;
        if (cls == 4) p.memIndex = MEM_STORE_BIT | ((uint32_t(lsq.sqTail) + stores) & SQ_MASK);
        if (cls == 3 || cls == 4) {
          const unsigned size = uint32_t(d.funct3) & 3u;
          p.nBytes = size == 0 ? 1u : size == 1 ? 2u : 4u;
          p.unsignedLoad = cls == 3 && (uint32_t(d.funct3) & 4u);
        }
        loads += cls == 3;
        stores += cls == 4;
        p.lqSnapshot = (uint32_t(lsq.lqTail) + loads) & LQ_SEQ_MASK;
        p.sqSnapshot = (uint32_t(lsq.sqTail) + stores) & SQ_SEQ_MASK;
        physical += writes;
        tag = robNextTag(tag);
      }
    }
    wideCache.valid = true;
  }
  return wideCache.lanes[requested];
}

void IssueArbiter::wire_wide_output() {
  for (unsigned lane = 0; lane < 4; ++lane) {
    auto &out = lane == 0 ? lane0 : lane == 1 ? lane1 : lane == 2 ? lane2 : lane3;
    out.core.valid = [this, lane]() { return wideDecision(lane).valid; };
    out.core.allocDest = [this, lane]() { return wideDecision(lane).alloc; };
    out.core.phy = [this, lane]() { return wideDecision(lane).phy; };
    out.core.robTag = [this, lane]() { return wideDecision(lane).tag; };
    out.core.isLoad = [this, lane]() { return wideDecision(lane).cls == 3; };
    out.core.isStore = [this, lane]() { return wideDecision(lane).cls == 4; };
    out.core.isControl = [this, lane]() { return wideDecision(lane).control; };
    out.core.nBytes = [this, lane]() { return wideDecision(lane).nBytes; };
    out.core.isUnsigned = [this, lane]() { return wideDecision(lane).unsignedLoad; };
    out.core.pc = [this, lane]() { return wideDecision(lane).pc; };
    out.select.hasInteger = [this, lane]() { auto c = wideDecision(lane).cls; return c == 1 || c == 6; };
    out.select.hasMultiply = [this, lane]() { return wideDecision(lane).cls == 8; };
    out.select.hasDivide = [this, lane]() { return wideDecision(lane).cls == 9; };
    out.select.hasBranch = [this, lane]() { return wideDecision(lane).cls == 5; };
    out.select.hasLoad = [this, lane]() { return wideDecision(lane).cls == 3; };
    out.select.hasStore = [this, lane]() { return wideDecision(lane).cls == 4; };
    auto slot = [this, lane](auto &wire, unsigned pool) {
      wire = [this, lane, pool]() { return wideDecision(lane).slots[pool]; };
    };
    slot(out.select.integerSlot, 0); slot(out.select.multiplySlot, 1);
    slot(out.select.divideSlot, 2); slot(out.select.branchSlot, 3);
    slot(out.select.loadSlot, 4); slot(out.select.storeAddrSlot, 5);
    slot(out.select.storeValueSlot, 6);
    auto payload = [this, lane](auto &port, unsigned cls) {
      port.op = [this, lane, cls]() {
        const auto &p = wideDecision(lane);
        return p.cls == cls || (cls == 1 && p.cls == 6) ? p.op : uint32_t(Operation::OP_INVALID);
      };
      port.s1Tag = [this, lane]() { const auto &p = wideDecision(lane); return p.cls == 6 ? 0u : unsigned(p.s1.tag); };
      port.s1Imm = [this, lane]() { const auto &p = wideDecision(lane); return p.cls == 6 ? (p.upperPC ? p.pc : 0u) : uint32_t(p.s1.imm); };
      port.s2Tag = [this, lane]() {
        const auto &p = wideDecision(lane);
        return p.cls == 6 || p.cls == 3 || p.cls == 4 || (p.cls == 1 && p.intImmediate) ? 0u : unsigned(p.s2.tag);
      };
      port.s2Imm = [this, lane]() {
        const auto &p = wideDecision(lane);
        return p.cls == 6 || p.cls == 3 || p.cls == 4 || (p.cls == 1 && p.intImmediate) ? p.imm : uint32_t(p.s2.imm);
      };
      port.robTag = [this, lane]() { return wideDecision(lane).tag; };
    };
    payload(out.intP, 1); payload(out.mulP, 8); payload(out.divP, 9);
    payload(out.brP, 5); payload(out.loadP, 3); payload(out.saP, 4);
    out.loadP.memIndex = [this, lane]() { return wideDecision(lane).memIndex; };
    out.saP.memIndex = [this, lane]() { return wideDecision(lane).memIndex; };
    out.svP.memIndex = [this, lane]() { return wideDecision(lane).memIndex; };
    out.svP.robTag = [this, lane]() { return wideDecision(lane).tag; };
    out.svP.dataTag = [this, lane]() { return unsigned(wideDecision(lane).s2.tag); };
    out.svP.dataImm = [this, lane]() { return uint32_t(wideDecision(lane).s2.imm); };
    out.brP.imm = [this, lane]() { return wideDecision(lane).imm; };
    out.brP.pc = [this, lane]() { return wideDecision(lane).pc; };
    out.robEntry.type = [this, lane]() {
      const auto &p = wideDecision(lane);
      return uint32_t(p.control ? ROBType::LINK : p.cls == 4 ? ROBType::STORE : p.cls == 5 ? ROBType::BRANCH : ROBType::REGISTER);
    };
    out.robEntry.isCommitReady = [this, lane]() { auto c = wideDecision(lane).cls; return c == 2 || c == 7; };
    // Legacy HALT does not allocate a destination, but retirement still needs
    // its architectural rd to select the result (a0), not x0.
    out.robEntry.dest = [this, lane]() { const auto &p = wideDecision(lane); return p.alloc || p.cls == 2 ? p.rd : 0u; };
    out.robEntry.halt = [this, lane]() { return wideDecision(lane).cls == 2; };
    out.robEntry.exception = [this, lane]() { return wideDecision(lane).cls == 7; };
    out.robEntry.isCall = [this, lane]() { return wideDecision(lane).call; };
    out.robEntry.isRet = [this, lane]() { return wideDecision(lane).ret; };
    out.robEntry.ckptId = [this, lane]() { return wideDecision(lane).ckptId; };
    out.robEntry.predictedPC = [this, lane]() { return wideDecision(lane).predictedPC; };
    out.robEntry.pc = [this, lane]() { return wideDecision(lane).pc; };
    out.robEntry.lqTailSnapshot = [this, lane]() { return wideDecision(lane).lqSnapshot; };
    out.robEntry.sqTailSnapshot = [this, lane]() { return wideDecision(lane).sqSnapshot; };
    out.robEntry.newPhy = [this, lane]() { return wideDecision(lane).phy; };
    out.robEntry.oldPhy = [this, lane]() { return wideDecision(lane).oldPhy; };
  }
}

// ---- Single issue port: `win` picks the branch, every Output field is a
// small mux keyed on it. Failure paths reproduce the former default packet
// field by field (IssuePacket{} defaults: valid=false, phy/oldPhy/newPhy=
// InvalidPhy, tags=InvalidPhy, ops=OP_INVALID, slots/robTag/memIndex=0;
// slots now 0-default + has-gated per the retired -1 sentinel). ----
void IssueArbiter::wire_output() {
  // ---- branch parameters (pure functions of the decoded head) ----
  intImmAsVk = [this]() -> uint32_t {
    if (issueClass(dec) != 1u)
      return 0u;
    return static_cast<uint32_t>(dec.type) == static_cast<uint32_t>(RISC_V::R)
               ? 0u
               : 1u;
  };
  intIsControl = [this]() -> uint32_t {
    return (issueClass(dec) == 1u && static_cast<uint32_t>(dec.type) ==
                                         static_cast<uint32_t>(RISC_V::I) &&
            static_cast<uint32_t>(dec.opcode) == 0x67u)
               ? 1u
               : 0u;
  };
  ujHasPC = [this]() -> uint32_t {
    if (issueClass(dec) != 6u)
      return 0u;
    const uint32_t type = static_cast<uint32_t>(dec.type);
    return (type == static_cast<uint32_t>(RISC_V::J) ||
            static_cast<uint32_t>(dec.opcode) == 0x17u)
               ? 1u
               : 0u;
  };
  ujIsControl = [this]() -> uint32_t {
    return (issueClass(dec) == 6u && static_cast<uint32_t>(dec.type) ==
                                         static_cast<uint32_t>(RISC_V::J))
               ? 1u
               : 0u;
  };
  opDec = [this]() -> uint32_t {
    return static_cast<uint32_t>(decodeOp(
        static_cast<uint32_t>(dec.type), static_cast<uint32_t>(dec.opcode),
        static_cast<uint32_t>(dec.funct3), static_cast<uint32_t>(dec.funct7)));
  };

  // ---- first-fit free-slot scans over the busy bitmaps (lane0) ----
  intFree = [this]() -> uint32_t {
    for (int i = 0; i < INTEGERRS_CAP; i++)
      if (!static_cast<bool>(rs.intBusy[i]))
        return 1u;
    return static_cast<bool>(rs.intReuseValid) ? 1u : 0u;
  };
  intSlot = [this]() -> uint32_t {
    for (int i = 0; i < INTEGERRS_CAP; i++)
      if (!static_cast<bool>(rs.intBusy[i]))
        return static_cast<uint32_t>(i);
    return static_cast<bool>(rs.intReuseValid) ? static_cast<uint32_t>(rs.intReuseSlot) : 0u;
  };
  loadFree = [this]() -> uint32_t {
    for (int i = 0; i < LOADRS_CAP; i++)
      if (!static_cast<bool>(rs.loadBusy[i]))
        return 1u;
    return static_cast<bool>(rs.loadReuseValid) ? 1u : 0u;
  };
  loadSlot = [this]() -> uint32_t {
    for (int i = 0; i < LOADRS_CAP; i++)
      if (!static_cast<bool>(rs.loadBusy[i]))
        return static_cast<uint32_t>(i);
    return static_cast<bool>(rs.loadReuseValid) ? static_cast<uint32_t>(rs.loadReuseSlot) : 0u;
  };
  saFree = [this]() -> uint32_t {
    for (int i = 0; i < STORERS_CAP; i++)
      if (!static_cast<bool>(rs.saBusy[i]))
        return 1u;
    return 0u;
  };
  saSlot = [this]() -> uint32_t {
    for (int i = 0; i < STORERS_CAP; i++)
      if (!static_cast<bool>(rs.saBusy[i]))
        return static_cast<uint32_t>(i);
    return 0u;
  };
  svFree = [this]() -> uint32_t {
    for (int i = 0; i < STORERS_CAP; i++)
      if (!static_cast<bool>(rs.svBusy[i]))
        return 1u;
    return 0u;
  };
  svSlot = [this]() -> uint32_t {
    for (int i = 0; i < STORERS_CAP; i++)
      if (!static_cast<bool>(rs.svBusy[i]))
        return static_cast<uint32_t>(i);
    return 0u;
  };
  brFree = [this]() -> uint32_t {
    for (int i = 0; i < BRANCHRS_CAP; i++)
      if (!static_cast<bool>(rs.brBusy[i]))
        return 1u;
    return static_cast<bool>(rs.brReuseValid) ? 1u : 0u;
  };
  brSlot = [this]() -> uint32_t {
    for (int i = 0; i < BRANCHRS_CAP; i++)
      if (!static_cast<bool>(rs.brBusy[i]))
        return static_cast<uint32_t>(i);
    return static_cast<bool>(rs.brReuseValid) ? static_cast<uint32_t>(rs.brReuseSlot) : 0u;
  };
  mulFree = [this]() -> uint32_t {
    for (int i = 0; i < MULTIPLYRS_CAP; i++)
      if (!static_cast<bool>(rs.mulBusy[i]))
        return 1u;
    return 0u;
  };
  mulSlot = [this]() -> uint32_t {
    for (int i = 0; i < MULTIPLYRS_CAP; i++)
      if (!static_cast<bool>(rs.mulBusy[i]))
        return static_cast<uint32_t>(i);
    return 0u;
  };
  divFree = [this]() -> uint32_t {
    for (int i = 0; i < DIVIDERS_CAP; i++)
      if (!static_cast<bool>(rs.divBusy[i]))
        return 1u;
    return 0u;
  };
  divSlot = [this]() -> uint32_t {
    for (int i = 0; i < DIVIDERS_CAP; i++)
      if (!static_cast<bool>(rs.divBusy[i]))
        return static_cast<uint32_t>(i);
    return 0u;
  };

  win = [this]() -> uint32_t {
    if (static_cast<bool>(squashNeed) || static_cast<bool>(dec.isEmpty))
      return 0u;
    const uint32_t cls = issueClass(dec);
    const bool robFull = static_cast<bool>(rob.isFull);
    switch (cls) {
    case 1u: // INT
      return (!robFull && static_cast<bool>(intFree) &&
              (!static_cast<bool>(dec.allocDest) ||
               !static_cast<bool>(prf.freeListEmpty)))
                 ? 1u
                 : 0u;
    case 2u: // HALT
      return !robFull ? 2u : 0u;
    case 3u: // LOAD
      return (!robFull && !static_cast<bool>(lsq.lqFull) &&
              static_cast<bool>(loadFree) &&
              (!static_cast<bool>(dec.allocDest) ||
               !static_cast<bool>(prf.freeListEmpty)))
                 ? 3u
                 : 0u;
    case 4u: // STORE
      return (!robFull && !static_cast<bool>(lsq.sqFull) &&
              static_cast<bool>(saFree) && static_cast<bool>(svFree))
                 ? 4u
                 : 0u;
    case 5u: // BR
      return (!robFull && static_cast<bool>(brFree)) ? 5u : 0u;
    case 6u: // UJ
      return (!robFull && static_cast<bool>(intFree) &&
              (!static_cast<bool>(dec.allocDest) ||
               !static_cast<bool>(prf.freeListEmpty)))
                 ? 6u
                 : 0u;
    case 7u: // RV_INVALID
      return !robFull ? 7u : 0u;
    case 8u: // MUL
      return (!robFull && static_cast<bool>(mulFree) &&
              (!static_cast<bool>(dec.allocDest) ||
               !static_cast<bool>(prf.freeListEmpty)) &&
              static_cast<uint32_t>(opDec) !=
                  static_cast<uint32_t>(Operation::OP_INVALID))
                 ? 8u
                 : 0u;
    case 9u: // DIV
      return (!robFull && static_cast<bool>(divFree) &&
              (!static_cast<bool>(dec.allocDest) ||
               !static_cast<bool>(prf.freeListEmpty)) &&
              static_cast<uint32_t>(opDec) !=
                  static_cast<uint32_t>(Operation::OP_INVALID))
                 ? 9u
                 : 0u;
    default:
      return 0u;
    }
  };

  // ---- lane0 core fields ----
  lane0.core.valid = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) != 0u ? 1u : 0u;
  };
  lane0.core.allocDest = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
      return 0u;
    return (static_cast<bool>(dec.allocDest) &&
            !static_cast<bool>(prf.freeListEmpty))
               ? 1u
               : 0u;
  };
  lane0.core.phy = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
      return static_cast<uint32_t>(InvalidPhy);
    return (static_cast<bool>(dec.allocDest) &&
            !static_cast<bool>(prf.freeListEmpty))
               ? static_cast<uint32_t>(prf.freePhy)
               : static_cast<uint32_t>(InvalidPhy);
  };
  lane0.core.robTag = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return ((w >= 1u && w <= 6u) || w == 8u || w == 9u)
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };
  lane0.core.isLoad = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u ? 1u : 0u;
  };
  lane0.core.isStore = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u ? 1u : 0u;
  };
  lane0.core.isControl = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return ((w == 1u && static_cast<bool>(intIsControl)) ||
            (w == 6u && static_cast<bool>(ujIsControl)))
               ? 1u
               : 0u;
  };
  lane0.core.nBytes = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w != 3u && w != 4u)
      return 0u;
    const uint32_t f3 = static_cast<uint32_t>(dec.funct3);
    if (w == 3u) {
      switch (f3) {
      case 0: return 1u;
      case 1: return 2u;
      case 2: return 4u;
      case 4: return 1u;
      case 5: return 2u;
      default: return 4u;
      }
    }
    switch (f3) {
    case 0: return 1u;
    case 1: return 2u;
    case 2: return 4u;
    default: return 4u;
    }
  };
  lane0.core.isUnsigned = [this]() -> uint32_t {
    const uint32_t f3 = static_cast<uint32_t>(dec.funct3);
    return (static_cast<uint32_t>(win) == 3u && (f3 == 4u || f3 == 5u)) ? 1u
                                                                        : 0u;
  };
  lane0.core.pc = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return ((w == 1u && static_cast<bool>(intIsControl)) ||
            (w == 6u && static_cast<bool>(ujIsControl)))
               ? static_cast<uint32_t>(dec.pc)
               : 0u;
  };

  // ---- lane0 select group ----
  lane0.select.hasInteger = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w == 1u || w == 6u) ? 1u : 0u;
  };
  lane0.select.hasLoad = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u ? 1u : 0u;
  };
  lane0.select.hasStore = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u ? 1u : 0u;
  };
  lane0.select.hasBranch = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 5u ? 1u : 0u;
  };
  lane0.select.hasMultiply = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 8u ? 1u : 0u;
  };
  lane0.select.integerSlot = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w == 1u || w == 6u) ? static_cast<uint32_t>(intSlot) : 0u;
  };
  lane0.select.loadSlot = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u ? static_cast<uint32_t>(loadSlot)
                                            : 0u;
  };
  lane0.select.storeAddrSlot = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u ? static_cast<uint32_t>(saSlot)
                                            : 0u;
  };
  lane0.select.storeValueSlot = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u ? static_cast<uint32_t>(svSlot)
                                            : 0u;
  };
  lane0.select.branchSlot = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 5u ? static_cast<uint32_t>(brSlot)
                                            : 0u;
  };
  lane0.select.multiplySlot = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 8u ? static_cast<uint32_t>(mulSlot)
                                            : 0u;
  };
  lane0.select.hasDivide = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 9u ? 1u : 0u;
  };
  lane0.select.divideSlot = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 9u ? static_cast<uint32_t>(divSlot)
                                            : 0u;
  };

  // ---- lane0 intP payload ----
  lane0.intP.op = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w == 1u || w == 6u)
               ? static_cast<uint32_t>(opDec)
               : static_cast<uint32_t>(Operation::OP_INVALID);
  };
  lane0.intP.s1Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 1u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s1).tag);
  };
  lane0.intP.s1Imm = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w == 1u)
      return static_cast<uint32_t>(resolveSrc(rat.s1).imm);
    if (w == 6u)
      return static_cast<bool>(ujHasPC) ? static_cast<uint32_t>(dec.pc) : 0u;
    return 0u;
  };
  lane0.intP.s2Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 1u)
      return static_cast<uint32_t>(InvalidPhy);
    if (static_cast<bool>(intImmAsVk))
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s2).tag);
  };
  lane0.intP.s2Imm = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w == 1u) {
      if (static_cast<bool>(intImmAsVk))
        return static_cast<uint32_t>(dec.imm);
      return static_cast<uint32_t>(resolveSrc(rat.s2).imm);
    }
    if (w == 6u)
      return static_cast<uint32_t>(dec.imm);
    return 0u;
  };
  lane0.intP.robTag = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w == 1u || w == 6u) ? static_cast<uint32_t>(rob.nextTag) : 0u;
  };

  // ---- lane0 loadP payload ----
  lane0.loadP.op = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u
               ? static_cast<uint32_t>(opDec)
               : static_cast<uint32_t>(Operation::OP_INVALID);
  };
  lane0.loadP.s1Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 3u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s1).tag);
  };
  lane0.loadP.s1Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 3u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s1).imm);
  };
  lane0.loadP.s2Tag = [this]() -> uint32_t {
    return static_cast<uint32_t>(InvalidPhy);
  };
  lane0.loadP.s2Imm = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u ? static_cast<uint32_t>(dec.imm)
                                            : 0u;
  };
  lane0.loadP.robTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };
  lane0.loadP.memIndex = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 3u
               ? (static_cast<uint32_t>(lsq.lqTail) & LQ_MASK)
               : 0u;
  };

  // ---- lane0 saP / svP payloads ----
  lane0.saP.op = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u
               ? static_cast<uint32_t>(opDec)
               : static_cast<uint32_t>(Operation::OP_INVALID);
  };
  lane0.saP.s1Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 4u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s1).tag);
  };
  lane0.saP.s1Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 4u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s1).imm);
  };
  lane0.saP.s2Tag = [this]() -> uint32_t {
    return static_cast<uint32_t>(InvalidPhy);
  };
  lane0.saP.s2Imm = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u ? static_cast<uint32_t>(dec.imm)
                                            : 0u;
  };
  lane0.saP.robTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };
  lane0.saP.memIndex = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u
               ? ((static_cast<uint32_t>(lsq.sqTail) & SQ_MASK) |
                  static_cast<uint32_t>(MEM_STORE_BIT))
               : 0u;
  };
  lane0.svP.dataTag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 4u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s2).tag);
  };
  lane0.svP.dataImm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 4u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s2).imm);
  };
  lane0.svP.robTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };
  lane0.svP.memIndex = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 4u
               ? ((static_cast<uint32_t>(lsq.sqTail) & SQ_MASK) |
                  static_cast<uint32_t>(MEM_STORE_BIT))
               : 0u;
  };

  // ---- lane0 brP payload ----
  lane0.brP.op = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 5u
               ? static_cast<uint32_t>(opDec)
               : static_cast<uint32_t>(Operation::OP_INVALID);
  };
  lane0.brP.s1Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 5u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s1).tag);
  };
  lane0.brP.s1Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 5u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s1).imm);
  };
  lane0.brP.s2Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 5u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s2).tag);
  };
  lane0.brP.s2Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 5u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s2).imm);
  };
  lane0.brP.robTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 5u
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };
  lane0.brP.imm = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 5u ? static_cast<uint32_t>(dec.imm)
                                            : 0u;
  };
  lane0.brP.pc = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 5u ? static_cast<uint32_t>(dec.pc)
                                            : 0u;
  };

  // ---- lane0 mulP payload ----
  lane0.mulP.op = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 8u
               ? static_cast<uint32_t>(opDec)
               : static_cast<uint32_t>(Operation::OP_INVALID);
  };
  lane0.mulP.s1Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 8u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s1).tag);
  };
  lane0.mulP.s1Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 8u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s1).imm);
  };
  lane0.mulP.s2Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 8u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s2).tag);
  };
  lane0.mulP.s2Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 8u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s2).imm);
  };
  lane0.mulP.robTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 8u
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };

  // ---- lane0 divP payload ----
  lane0.divP.op = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 9u
               ? static_cast<uint32_t>(opDec)
               : static_cast<uint32_t>(Operation::OP_INVALID);
  };
  lane0.divP.s1Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 9u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s1).tag);
  };
  lane0.divP.s1Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 9u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s1).imm);
  };
  lane0.divP.s2Tag = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 9u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<uint32_t>(resolveSrc(rat.s2).tag);
  };
  lane0.divP.s2Imm = [this]() -> uint32_t {
    if (static_cast<uint32_t>(win) != 9u)
      return 0u;
    return static_cast<uint32_t>(resolveSrc(rat.s2).imm);
  };
  lane0.divP.robTag = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 9u
               ? static_cast<uint32_t>(rob.nextTag)
               : 0u;
  };

  // ---- lane0 robEntry group ----
  lane0.robEntry.type = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w == 1u)
      return static_cast<bool>(intIsControl)
                 ? static_cast<uint32_t>(ROBType::LINK)
                 : static_cast<uint32_t>(ROBType::REGISTER);
    if (w == 6u)
      return static_cast<bool>(ujIsControl)
                 ? static_cast<uint32_t>(ROBType::LINK)
                 : static_cast<uint32_t>(ROBType::REGISTER);
    if (w == 4u)
      return static_cast<uint32_t>(ROBType::STORE);
    if (w == 5u)
      return static_cast<uint32_t>(ROBType::BRANCH);
    return static_cast<uint32_t>(ROBType::REGISTER);
  };
  lane0.robEntry.isCommitReady = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 2u || static_cast<uint32_t>(win) == 7u ? 1u : 0u;
  };
  lane0.robEntry.dest = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w == 1u || w == 2u || w == 3u || w == 6u || w == 8u || w == 9u)
               ? static_cast<uint32_t>(dec.rd)
               : 0u;
  };
  lane0.robEntry.halt = [this]() -> uint32_t {
    return static_cast<uint32_t>(win) == 2u ? 1u : 0u;
  };
  lane0.robEntry.exception = [this]() -> uint32_t { return static_cast<uint32_t>(win) == 7u; };
  lane0.robEntry.isCall = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    const uint32_t rd = static_cast<uint32_t>(dec.rd);
    const bool rdLink = (rd == 1u || rd == 5u);
    const bool jalrCall =
        (w == 1u && static_cast<bool>(intIsControl) && rdLink);
    const bool jalCall =
        (w == 6u && static_cast<bool>(ujIsControl) && rdLink);
    return (jalrCall || jalCall) ? 1u : 0u;
  };
  lane0.robEntry.isRet = [this]() -> uint32_t {
    const uint32_t rs1 = static_cast<uint32_t>(dec.rs1);
    const bool rs1Link = (rs1 == 1u || rs1 == 5u);
    return (static_cast<uint32_t>(win) == 1u &&
             static_cast<bool>(intIsControl) && rs1Link)
               ? 1u
               : 0u;
  };
  lane0.robEntry.ckptId = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w >= 1u && w <= 9u)
               ? static_cast<uint32_t>(dec.ckptId)
               : 0u;
  };
  lane0.robEntry.predictedPC = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w == 1u || w == 2u || w == 5u || w == 6u || w == 7u || w == 8u || w == 9u)
               ? static_cast<uint32_t>(dec.predictedPC)
               : 0u;
  };
  lane0.robEntry.pc = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    return (w >= 1u && w <= 9u)
               ? static_cast<uint32_t>(dec.pc)
               : 0u;
  };
  lane0.robEntry.lqTailSnapshot = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w == 1u || w == 4u || w == 5u || w == 6u || w == 8u || w == 9u)
      return static_cast<uint32_t>(lsq.lqTail);
    if (w == 3u)
      return static_cast<uint32_t>(lsq.lqTailSnapshot);
    return 0u;
  };
  lane0.robEntry.sqTailSnapshot = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w == 1u || w == 3u || w == 5u || w == 6u || w == 8u || w == 9u)
      return static_cast<uint32_t>(lsq.sqTail);
    if (w == 4u)
      return static_cast<uint32_t>(lsq.sqTailSnapshot);
    return 0u;
  };
  lane0.robEntry.newPhy = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
      return static_cast<uint32_t>(InvalidPhy);
    return (static_cast<bool>(dec.allocDest) &&
            !static_cast<bool>(prf.freeListEmpty))
               ? static_cast<uint32_t>(prf.freePhy)
               : static_cast<uint32_t>(InvalidPhy);
  };
  lane0.robEntry.oldPhy = [this]() -> uint32_t {
    const uint32_t w = static_cast<uint32_t>(win);
    if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
      return static_cast<uint32_t>(InvalidPhy);
    return static_cast<bool>(dec.allocDest)
               ? static_cast<uint32_t>(rat.rdOldPhy)
               : static_cast<uint32_t>(InvalidPhy);
  };

  // ===================== lane 1 (ISSUE_WIDTH == 2) =====================
  if constexpr (ISSUE_WIDTH >= 2) {
    // branch parameters for dec1
    intImmAsVk1 = [this]() -> uint32_t {
      if (issueClass(dec1) != 1u)
        return 0u;
      return static_cast<uint32_t>(dec1.type) == static_cast<uint32_t>(RISC_V::R)
                 ? 0u
                 : 1u;
    };
    intIsControl1 = [this]() -> uint32_t {
      return (issueClass(dec1) == 1u && static_cast<uint32_t>(dec1.type) ==
                                            static_cast<uint32_t>(RISC_V::I) &&
              static_cast<uint32_t>(dec1.opcode) == 0x67u)
                 ? 1u
                 : 0u;
    };
    ujHasPC1 = [this]() -> uint32_t {
      if (issueClass(dec1) != 6u)
        return 0u;
      const uint32_t type = static_cast<uint32_t>(dec1.type);
      return (type == static_cast<uint32_t>(RISC_V::J) ||
              static_cast<uint32_t>(dec1.opcode) == 0x17u)
                 ? 1u
                 : 0u;
    };
    ujIsControl1 = [this]() -> uint32_t {
      return (issueClass(dec1) == 6u && static_cast<uint32_t>(dec1.type) ==
                                            static_cast<uint32_t>(RISC_V::J))
                 ? 1u
                 : 0u;
    };
    opDec1 = [this]() -> uint32_t {
      return static_cast<uint32_t>(decodeOp(
          static_cast<uint32_t>(dec1.type), static_cast<uint32_t>(dec1.opcode),
          static_cast<uint32_t>(dec1.funct3), static_cast<uint32_t>(dec1.funct7)));
    };

    // second-lane free-slot scans (a free slot != lane0's chosen slot, when
    // lane0 actually consumed that pool).
    intFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 1u || static_cast<uint32_t>(win) == 6u;
      for (int i = 0; i < INTEGERRS_CAP; i++)
        if (!static_cast<bool>(rs.intBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(intSlot)))
          return 1u;
      return bool(rs.intReuseValid) && (!l0 || uint32_t(rs.intReuseSlot) != uint32_t(intSlot)) ? 1u : 0u;
    };
    intSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 1u || static_cast<uint32_t>(win) == 6u;
      for (int i = 0; i < INTEGERRS_CAP; i++)
        if (!static_cast<bool>(rs.intBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(intSlot)))
          return static_cast<uint32_t>(i);
      return bool(rs.intReuseValid) && (!l0 || uint32_t(rs.intReuseSlot) != uint32_t(intSlot))
                 ? uint32_t(rs.intReuseSlot) : 0u;
    };
    loadFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 3u;
      for (int i = 0; i < LOADRS_CAP; i++)
        if (!static_cast<bool>(rs.loadBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(loadSlot)))
          return 1u;
      return bool(rs.loadReuseValid) && (!l0 || uint32_t(rs.loadReuseSlot) != uint32_t(loadSlot)) ? 1u : 0u;
    };
    loadSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 3u;
      for (int i = 0; i < LOADRS_CAP; i++)
        if (!static_cast<bool>(rs.loadBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(loadSlot)))
          return static_cast<uint32_t>(i);
      return bool(rs.loadReuseValid) && (!l0 || uint32_t(rs.loadReuseSlot) != uint32_t(loadSlot))
                 ? uint32_t(rs.loadReuseSlot) : 0u;
    };
    saFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 4u;
      for (int i = 0; i < STORERS_CAP; i++)
        if (!static_cast<bool>(rs.saBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(saSlot)))
          return 1u;
      return 0u;
    };
    saSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 4u;
      for (int i = 0; i < STORERS_CAP; i++)
        if (!static_cast<bool>(rs.saBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(saSlot)))
          return static_cast<uint32_t>(i);
      return 0u;
    };
    svFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 4u;
      for (int i = 0; i < STORERS_CAP; i++)
        if (!static_cast<bool>(rs.svBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(svSlot)))
          return 1u;
      return 0u;
    };
    svSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 4u;
      for (int i = 0; i < STORERS_CAP; i++)
        if (!static_cast<bool>(rs.svBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(svSlot)))
          return static_cast<uint32_t>(i);
      return 0u;
    };
    brFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 5u;
      for (int i = 0; i < BRANCHRS_CAP; i++)
        if (!static_cast<bool>(rs.brBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(brSlot)))
          return 1u;
      return bool(rs.brReuseValid) && (!l0 || uint32_t(rs.brReuseSlot) != uint32_t(brSlot)) ? 1u : 0u;
    };
    brSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 5u;
      for (int i = 0; i < BRANCHRS_CAP; i++)
        if (!static_cast<bool>(rs.brBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(brSlot)))
          return static_cast<uint32_t>(i);
      return bool(rs.brReuseValid) && (!l0 || uint32_t(rs.brReuseSlot) != uint32_t(brSlot))
                 ? uint32_t(rs.brReuseSlot) : 0u;
    };
    mulFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 8u;
      for (int i = 0; i < MULTIPLYRS_CAP; i++)
        if (!static_cast<bool>(rs.mulBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(mulSlot)))
          return 1u;
      return 0u;
    };
    mulSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 8u;
      for (int i = 0; i < MULTIPLYRS_CAP; i++)
        if (!static_cast<bool>(rs.mulBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(mulSlot)))
          return static_cast<uint32_t>(i);
      return 0u;
    };
    divFree1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 9u;
      for (int i = 0; i < DIVIDERS_CAP; i++)
        if (!static_cast<bool>(rs.divBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(divSlot)))
          return 1u;
      return 0u;
    };
    divSlot1 = [this]() -> uint32_t {
      const bool l0 = static_cast<uint32_t>(win) == 9u;
      for (int i = 0; i < DIVIDERS_CAP; i++)
        if (!static_cast<bool>(rs.divBusy[i]) &&
            (!l0 || static_cast<uint32_t>(i) != static_cast<uint32_t>(divSlot)))
          return static_cast<uint32_t>(i);
      return 0u;
    };

    win1 = [this]() -> uint32_t {
      // lane1 issues only when lane0 issued (in-order) and its own resources
      // (after lane0's allocation) are available.
      if (static_cast<uint32_t>(win) == 0u || static_cast<bool>(dec1.isEmpty))
        return 0u;
      const uint32_t cls = issueClass(dec1);
      const bool robFull1 = static_cast<bool>(rob.isFull1);
      switch (cls) {
      case 1u: // INT
        return (!robFull1 && static_cast<bool>(intFree1) &&
                (!static_cast<bool>(dec1.allocDest) ||
                 !static_cast<bool>(prf.freeListEmpty1)))
                   ? 1u
                   : 0u;
      case 2u: // HALT
        return !robFull1 ? 2u : 0u;
      case 3u: // LOAD
        return (!robFull1 && !static_cast<bool>(lsq.lqFull1) &&
                static_cast<bool>(loadFree1) &&
                (!static_cast<bool>(dec1.allocDest) ||
                 !static_cast<bool>(prf.freeListEmpty1)))
                   ? 3u
                   : 0u;
      case 4u: // STORE
        return (!robFull1 && !static_cast<bool>(lsq.sqFull1) &&
                static_cast<bool>(saFree1) && static_cast<bool>(svFree1))
                   ? 4u
                   : 0u;
      case 5u: // BR
        return (!robFull1 && static_cast<bool>(brFree1)) ? 5u : 0u;
      case 6u: // UJ
        return (!robFull1 && static_cast<bool>(intFree1) &&
                (!static_cast<bool>(dec1.allocDest) ||
                 !static_cast<bool>(prf.freeListEmpty1)))
                   ? 6u
                   : 0u;
      case 7u: // RV_INVALID
        return !robFull1 ? 7u : 0u;
      case 8u: // MUL
        return (!robFull1 && static_cast<bool>(mulFree1) &&
                (!static_cast<bool>(dec1.allocDest) ||
                 !static_cast<bool>(prf.freeListEmpty1)) &&
                static_cast<uint32_t>(opDec1) !=
                    static_cast<uint32_t>(Operation::OP_INVALID))
                   ? 8u
                   : 0u;
      case 9u: // DIV
        return (!robFull1 && static_cast<bool>(divFree1) &&
                (!static_cast<bool>(dec1.allocDest) ||
                 !static_cast<bool>(prf.freeListEmpty1)) &&
                static_cast<uint32_t>(opDec1) !=
                    static_cast<uint32_t>(Operation::OP_INVALID))
                   ? 9u
                   : 0u;
      default:
        return 0u;
      }
    };

    // lane0's forwarding context is read directly via lane0.core / dec.

    // ---- lane1 core fields ----
    lane1.core.valid = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) != 0u ? 1u : 0u;
    };
    lane1.core.allocDest = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
        return 0u;
      return (static_cast<bool>(dec1.allocDest) &&
              !static_cast<bool>(prf.freeListEmpty1))
                 ? 1u
                 : 0u;
    };
    lane1.core.phy = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
        return static_cast<uint32_t>(InvalidPhy);
      return (static_cast<bool>(dec1.allocDest) &&
              !static_cast<bool>(prf.freeListEmpty1))
                 ? static_cast<uint32_t>(prf.freePhy1)
                 : static_cast<uint32_t>(InvalidPhy);
    };
    lane1.core.robTag = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return ((w >= 1u && w <= 6u) || w == 8u || w == 9u)
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };
    lane1.core.isLoad = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u ? 1u : 0u;
    };
    lane1.core.isStore = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u ? 1u : 0u;
    };
    lane1.core.isControl = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return ((w == 1u && static_cast<bool>(intIsControl1)) ||
              (w == 6u && static_cast<bool>(ujIsControl1)))
                 ? 1u
                 : 0u;
    };
    lane1.core.nBytes = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w != 3u && w != 4u)
        return 0u;
      const uint32_t f3 = static_cast<uint32_t>(dec1.funct3);
      if (w == 3u) {
        switch (f3) {
        case 0: return 1u;
        case 1: return 2u;
        case 2: return 4u;
        case 4: return 1u;
        case 5: return 2u;
        default: return 4u;
        }
      }
      switch (f3) {
      case 0: return 1u;
      case 1: return 2u;
      case 2: return 4u;
      default: return 4u;
      }
    };
    lane1.core.isUnsigned = [this]() -> uint32_t {
      const uint32_t f3 = static_cast<uint32_t>(dec1.funct3);
      return (static_cast<uint32_t>(win1) == 3u && (f3 == 4u || f3 == 5u)) ? 1u
                                                                          : 0u;
    };
    lane1.core.pc = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return ((w == 1u && static_cast<bool>(intIsControl1)) ||
              (w == 6u && static_cast<bool>(ujIsControl1)))
                 ? static_cast<uint32_t>(dec1.pc)
                 : 0u;
    };

    // ---- lane1 select group ----
    lane1.select.hasInteger = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w == 1u || w == 6u) ? 1u : 0u;
    };
    lane1.select.hasLoad = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u ? 1u : 0u;
    };
    lane1.select.hasStore = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u ? 1u : 0u;
    };
    lane1.select.hasBranch = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 5u ? 1u : 0u;
    };
    lane1.select.hasMultiply = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 8u ? 1u : 0u;
    };
    lane1.select.integerSlot = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w == 1u || w == 6u) ? static_cast<uint32_t>(intSlot1) : 0u;
    };
    lane1.select.loadSlot = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u ? static_cast<uint32_t>(loadSlot1)
                                               : 0u;
    };
    lane1.select.storeAddrSlot = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u ? static_cast<uint32_t>(saSlot1)
                                               : 0u;
    };
    lane1.select.storeValueSlot = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u ? static_cast<uint32_t>(svSlot1)
                                               : 0u;
    };
    lane1.select.branchSlot = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 5u ? static_cast<uint32_t>(brSlot1)
                                               : 0u;
    };
    lane1.select.multiplySlot = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 8u ? static_cast<uint32_t>(mulSlot1)
                                               : 0u;
    };
    lane1.select.hasDivide = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 9u ? 1u : 0u;
    };
    lane1.select.divideSlot = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 9u ? static_cast<uint32_t>(divSlot1)
                                               : 0u;
    };

    // ---- lane1 intP payload (intra-group RAW forwarding) ----
    lane1.intP.op = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w == 1u || w == 6u)
                 ? static_cast<uint32_t>(opDec1)
                 : static_cast<uint32_t>(Operation::OP_INVALID);
    };
    lane1.intP.s1Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 1u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.intP.s1Imm = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w == 1u)
        return static_cast<uint32_t>(
            resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                          static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
      if (w == 6u)
        return static_cast<bool>(ujHasPC1) ? static_cast<uint32_t>(dec1.pc) : 0u;
      return 0u;
    };
    lane1.intP.s2Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 1u)
        return static_cast<uint32_t>(InvalidPhy);
      if (static_cast<bool>(intImmAsVk1))
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.intP.s2Imm = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w == 1u) {
        if (static_cast<bool>(intImmAsVk1))
          return static_cast<uint32_t>(dec1.imm);
        return static_cast<uint32_t>(
            resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                          static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
      }
      if (w == 6u)
        return static_cast<uint32_t>(dec1.imm);
      return 0u;
    };
    lane1.intP.robTag = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w == 1u || w == 6u) ? static_cast<uint32_t>(rob.nextTag1) : 0u;
    };

    // ---- lane1 loadP payload ----
    lane1.loadP.op = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u
                 ? static_cast<uint32_t>(opDec1)
                 : static_cast<uint32_t>(Operation::OP_INVALID);
    };
    lane1.loadP.s1Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 3u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.loadP.s1Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 3u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.loadP.s2Tag = [this]() -> uint32_t {
      return static_cast<uint32_t>(InvalidPhy);
    };
    lane1.loadP.s2Imm = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u ? static_cast<uint32_t>(dec1.imm)
                                               : 0u;
    };
    lane1.loadP.robTag = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };
    lane1.loadP.memIndex = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 3u
                 ? (static_cast<uint32_t>(lsq.lqTail1) & LQ_MASK)
                 : 0u;
    };

    // ---- lane1 saP / svP payloads ----
    lane1.saP.op = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u
                 ? static_cast<uint32_t>(opDec1)
                 : static_cast<uint32_t>(Operation::OP_INVALID);
    };
    lane1.saP.s1Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 4u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.saP.s1Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 4u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.saP.s2Tag = [this]() -> uint32_t {
      return static_cast<uint32_t>(InvalidPhy);
    };
    lane1.saP.s2Imm = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u ? static_cast<uint32_t>(dec1.imm)
                                               : 0u;
    };
    lane1.saP.robTag = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };
    lane1.saP.memIndex = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u
                 ? ((static_cast<uint32_t>(lsq.sqTail1) & SQ_MASK) |
                    static_cast<uint32_t>(MEM_STORE_BIT))
                 : 0u;
    };
    lane1.svP.dataTag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 4u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.svP.dataImm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 4u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.svP.robTag = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };
    lane1.svP.memIndex = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 4u
                 ? ((static_cast<uint32_t>(lsq.sqTail1) & SQ_MASK) |
                    static_cast<uint32_t>(MEM_STORE_BIT))
                 : 0u;
    };

    // ---- lane1 brP payload ----
    lane1.brP.op = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 5u
                 ? static_cast<uint32_t>(opDec1)
                 : static_cast<uint32_t>(Operation::OP_INVALID);
    };
    lane1.brP.s1Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 5u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.brP.s1Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 5u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.brP.s2Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 5u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.brP.s2Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 5u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.brP.robTag = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 5u
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };
    lane1.brP.imm = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 5u ? static_cast<uint32_t>(dec1.imm)
                                               : 0u;
    };
    lane1.brP.pc = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 5u ? static_cast<uint32_t>(dec1.pc)
                                               : 0u;
    };

    // ---- lane1 mulP payload ----
    lane1.mulP.op = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 8u
                 ? static_cast<uint32_t>(opDec1)
                 : static_cast<uint32_t>(Operation::OP_INVALID);
    };
    lane1.mulP.s1Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 8u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.mulP.s1Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 8u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.mulP.s2Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 8u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.mulP.s2Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 8u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.mulP.robTag = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 8u
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };

    // ---- lane1 divP payload ----
    lane1.divP.op = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 9u
                 ? static_cast<uint32_t>(opDec1)
                 : static_cast<uint32_t>(Operation::OP_INVALID);
    };
    lane1.divP.s1Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 9u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.divP.s1Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 9u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s1, static_cast<uint32_t>(dec1.rs1), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.divP.s2Tag = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 9u)
        return static_cast<uint32_t>(InvalidPhy);
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).tag);
    };
    lane1.divP.s2Imm = [this]() -> uint32_t {
      if (static_cast<uint32_t>(win1) != 9u)
        return 0u;
      return static_cast<uint32_t>(
          resolveSrcFwd(rat1.s2, static_cast<uint32_t>(dec1.rs2), static_cast<bool>(lane0.core.allocDest),
                        static_cast<uint32_t>(dec.rd), static_cast<uint32_t>(lane0.core.phy)).imm);
    };
    lane1.divP.robTag = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 9u
                 ? static_cast<uint32_t>(rob.nextTag1)
                 : 0u;
    };

    // ---- lane1 robEntry group (intra-group WAW: oldPhy follows lane0) ----
    lane1.robEntry.type = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w == 1u)
        return static_cast<bool>(intIsControl1)
                   ? static_cast<uint32_t>(ROBType::LINK)
                   : static_cast<uint32_t>(ROBType::REGISTER);
      if (w == 6u)
        return static_cast<bool>(ujIsControl1)
                   ? static_cast<uint32_t>(ROBType::LINK)
                   : static_cast<uint32_t>(ROBType::REGISTER);
      if (w == 4u)
        return static_cast<uint32_t>(ROBType::STORE);
      if (w == 5u)
        return static_cast<uint32_t>(ROBType::BRANCH);
      return static_cast<uint32_t>(ROBType::REGISTER);
    };
    lane1.robEntry.isCommitReady = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 2u || static_cast<uint32_t>(win1) == 7u ? 1u : 0u;
    };
    lane1.robEntry.dest = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w == 1u || w == 2u || w == 3u || w == 6u || w == 8u || w == 9u)
                 ? static_cast<uint32_t>(dec1.rd)
                 : 0u;
    };
    lane1.robEntry.halt = [this]() -> uint32_t {
      return static_cast<uint32_t>(win1) == 2u ? 1u : 0u;
    };
    lane1.robEntry.exception = [this]() -> uint32_t { return static_cast<uint32_t>(win1) == 7u; };
    lane1.robEntry.isCall = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      const uint32_t rd = static_cast<uint32_t>(dec1.rd);
      const bool rdLink = (rd == 1u || rd == 5u);
      const bool jalrCall =
          (w == 1u && static_cast<bool>(intIsControl1) && rdLink);
      const bool jalCall =
          (w == 6u && static_cast<bool>(ujIsControl1) && rdLink);
      return (jalrCall || jalCall) ? 1u : 0u;
    };
    lane1.robEntry.isRet = [this]() -> uint32_t {
      const uint32_t rs1 = static_cast<uint32_t>(dec1.rs1);
      const bool rs1Link = (rs1 == 1u || rs1 == 5u);
      return (static_cast<uint32_t>(win1) == 1u &&
               static_cast<bool>(intIsControl1) && rs1Link)
                 ? 1u
                 : 0u;
    };
    lane1.robEntry.ckptId = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w >= 1u && w <= 9u)
                 ? static_cast<uint32_t>(dec1.ckptId)
                 : 0u;
    };
    lane1.robEntry.predictedPC = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w == 1u || w == 2u || w == 5u || w == 6u || w == 7u || w == 8u || w == 9u)
                 ? static_cast<uint32_t>(dec1.predictedPC)
                 : 0u;
    };
    lane1.robEntry.pc = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      return (w >= 1u && w <= 9u)
                 ? static_cast<uint32_t>(dec1.pc)
                 : 0u;
    };
    lane1.robEntry.lqTailSnapshot = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w == 1u || w == 4u || w == 5u || w == 6u || w == 8u || w == 9u)
        return static_cast<uint32_t>(lsq.lqTail1);
      if (w == 3u)
        return static_cast<uint32_t>(lsq.lqTailSnapshot1);
      return 0u;
    };
    lane1.robEntry.sqTailSnapshot = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w == 1u || w == 3u || w == 5u || w == 6u || w == 8u || w == 9u)
        return static_cast<uint32_t>(lsq.sqTail1);
      if (w == 4u)
        return static_cast<uint32_t>(lsq.sqTailSnapshot1);
      return 0u;
    };
    lane1.robEntry.newPhy = [this]() -> uint32_t {
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
        return static_cast<uint32_t>(InvalidPhy);
      return (static_cast<bool>(dec1.allocDest) &&
              !static_cast<bool>(prf.freeListEmpty1))
                 ? static_cast<uint32_t>(prf.freePhy1)
                 : static_cast<uint32_t>(InvalidPhy);
    };
    lane1.robEntry.oldPhy = [this]() -> uint32_t {
      // Intra-group WAW: lane1's old mapping is lane0's newPhy when
      // both write the same register; otherwise the old speculative RAT mapping.
      const uint32_t w = static_cast<uint32_t>(win1);
      if (w != 1u && w != 3u && w != 6u && w != 8u && w != 9u)
        return static_cast<uint32_t>(InvalidPhy);
      if (!static_cast<bool>(dec1.allocDest))
        return static_cast<uint32_t>(InvalidPhy);
      if (static_cast<bool>(lane0.core.allocDest) && static_cast<uint32_t>(dec1.rd) == static_cast<uint32_t>(dec.rd))
        return static_cast<uint32_t>(lane0.core.phy);
      return static_cast<uint32_t>(rat1.rdOldPhy);
    };
  }
}
