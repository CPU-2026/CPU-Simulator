#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include "tools.h"
using RobTag = uint8_t;
inline bool legacyHaltEnabled = false; // Host configuration for archived images only.

// Allocation width is independent of fetch, decode and execution bandwidth.
// Keep the old rename spelling as an exact compile-time alias, like RTL.
#ifndef CPU_ISSUE_WIDTH
#ifdef CPU_RENAME_WIDTH
#define CPU_ISSUE_WIDTH CPU_RENAME_WIDTH
#else
#define CPU_ISSUE_WIDTH 1
#endif
#endif
#ifndef CPU_RENAME_WIDTH
#define CPU_RENAME_WIDTH CPU_ISSUE_WIDTH
#endif
static_assert(CPU_ISSUE_WIDTH == CPU_RENAME_WIDTH, "conflicting issue/rename widths");
constexpr unsigned ISSUE_WIDTH = CPU_ISSUE_WIDTH;
static_assert(ISSUE_WIDTH == 1 || ISSUE_WIDTH == 2 || ISSUE_WIDTH == 4);

constexpr int INTEGERRS_CAP = 4;
constexpr int STORERS_CAP = 2;
constexpr int LOADRS_CAP = 2;
constexpr int BRANCHRS_CAP = 4;
constexpr int LQ_CAP = 8;
constexpr int SQ_CAP = 8;
constexpr int LQ_MASK = LQ_CAP - 1;
constexpr int SQ_MASK = SQ_CAP - 1;
constexpr int LQ_SEQ_MASK = (LQ_CAP << 1) - 1;
constexpr int SQ_SEQ_MASK = (SQ_CAP << 1) - 1;
// Tight hardware carrier widths for the queue pointer domains: each is the
// smallest field holding 0..CAP-1, and every wrap uses & (CAP-1), so a
// power-of-two CAP needs exactly bit_width(CAP-1) bits.
constexpr int LQ_PTR_WIDTH = std::bit_width(static_cast<uint32_t>(LQ_CAP - 1));
constexpr int SQ_PTR_WIDTH = std::bit_width(static_cast<uint32_t>(SQ_CAP - 1));
constexpr int LQ_SEQ_WIDTH = LQ_PTR_WIDTH + 1;
constexpr int SQ_SEQ_WIDTH = SQ_PTR_WIDTH + 1;
static_assert((1u << LQ_PTR_WIDTH) == static_cast<uint32_t>(LQ_CAP));
static_assert((1u << SQ_PTR_WIDTH) == static_cast<uint32_t>(SQ_CAP));
constexpr int MEMQ_SCAN_WINDOW = SQ_CAP < 8 ? SQ_CAP : 8;
constexpr uint8_t MEM_STORE_BIT = 0x40;
inline bool isStoreMem(uint8_t m) { return (m & MEM_STORE_BIT) != 0; }
inline uint8_t memSlot(uint8_t m) { return m & 0x3F; }
constexpr uint32_t ROB_CAP = ISSUE_WIDTH == 4 ? 32 : 16;
constexpr unsigned RECOVERY_BEATS = 4;
constexpr unsigned RECOVERY_WIDTH = (ROB_CAP + 3u) >> 2;
struct RecoveryStep {
  bool active = false;
  uint8_t beat = 0, offset = 0, liveCount = 0;
  RobTag nextTag = 0, squashTag = 0;
  bool first() const { return beat == 0; }
};
// Packed tag = {1-bit epoch, slot field}. The slot field holds 0..ROB_CAP-1,
// so its width is bit_width(ROB_CAP-1); the extra top bit is the epoch.
// ROB_CAP is not required to be a power of two: invalid slot codes between
// ROB_CAP and the field mask are never allocated (see robNextTag).
template <typename T> constexpr int ROB_TAG_BITWIDTH(T cap) {
  return std::bit_width(cap - 1) + 1;
}
constexpr int ROB_TAG_WIDTH = ROB_TAG_BITWIDTH(ROB_CAP);
constexpr int ROB_INDEX_MASK = (1 << (ROB_TAG_WIDTH - 1)) - 1;
constexpr int ROB_TAG_MASK = (1 << ROB_TAG_WIDTH) - 1;
inline constexpr uint8_t robSlot(RobTag tag) { return tag & ROB_INDEX_MASK; }
// Successor in the packed tag space: slots advance 0..ROB_CAP-1, then the
// epoch bit flips and the slot restarts at 0. Mask-only, no divider.
inline constexpr RobTag robNextTag(RobTag tag) {
  return (tag & ROB_INDEX_MASK) == static_cast<int>(ROB_CAP) - 1
             ? static_cast<RobTag>((~tag & ROB_TAG_MASK) & ~ROB_INDEX_MASK)
             : static_cast<RobTag>((tag + 1) & ROB_TAG_MASK);
}
static_assert(ROB_CAP >= 2, "ROB needs at least two slots for age ordering");
static_assert(ROB_TAG_WIDTH <= 8,
               "RobTag is uint8_t: packed tag must fit in 8 bits");
#ifndef CPU_FETCH_WIDTH
#define CPU_FETCH_WIDTH 1
#endif
#ifndef CPU_DECODE_WIDTH
#define CPU_DECODE_WIDTH 1
#endif
#ifndef CPU_RENAME_WIDTH
#define CPU_RENAME_WIDTH 1
#endif
constexpr unsigned FETCH_WIDTH = CPU_FETCH_WIDTH;
constexpr unsigned DECODE_WIDTH = CPU_DECODE_WIDTH;
static_assert(FETCH_WIDTH == 1 || FETCH_WIDTH == 2 || FETCH_WIDTH == 4);
static_assert(DECODE_WIDTH == 1 || DECODE_WIDTH == 2 || DECODE_WIDTH == 4);
#ifdef CPU_FQ_ENTRIES
constexpr int FQ_CAP = CPU_FQ_ENTRIES;
#else
constexpr int FQ_CAP = FETCH_WIDTH << 1;
#endif
#ifdef CPU_IQ_ENTRIES
constexpr int IQ_CAP = CPU_IQ_ENTRIES;
#else
constexpr int IQ_CAP = DECODE_WIDTH << 1;
#endif
constexpr int ISSUE_COUNT_WIDTH = std::bit_width(ISSUE_WIDTH);
constexpr int DECODE_COUNT_WIDTH = std::bit_width(DECODE_WIDTH);
constexpr int FQ_PTR_WIDTH = std::bit_width(static_cast<uint32_t>(FQ_CAP - 1));
constexpr int IQ_PTR_WIDTH = std::bit_width(static_cast<uint32_t>(IQ_CAP - 1));
constexpr int FQ_SEQ_WIDTH = FQ_PTR_WIDTH + 1;
constexpr int IQ_SEQ_WIDTH = IQ_PTR_WIDTH + 1;
constexpr int FQ_SEQ_MASK = (FQ_CAP << 1) - 1;
constexpr int IQ_SEQ_MASK = (IQ_CAP << 1) - 1;
static_assert((1u << FQ_PTR_WIDTH) == static_cast<uint32_t>(FQ_CAP));
static_assert((1u << IQ_PTR_WIDTH) == static_cast<uint32_t>(IQ_CAP));
constexpr int REGISTER_CAP = 32;
constexpr int FLUSHARBITER_CAP = 4;
constexpr int ALU_CAP = 1;
constexpr int MUL_CAP = 1; // Final register of the three-stage elastic MUL.
constexpr int MULTIPLYRS_CAP = 1;
constexpr int DIVIDERS_CAP = 1;
// Dedicated LSQ address port consumes or cancels the old result every cycle.
constexpr int AGU_CAP = 1;
constexpr int BRU_CAP = 4;
constexpr int BTB_CAP = 32;
static_assert(BTB_CAP >= 2 && BTB_CAP <= 64 && std::has_single_bit(uint32_t(BTB_CAP)));
constexpr int BTB_TAG_SHIFT = 2 + std::bit_width(uint32_t(BTB_CAP - 1));
constexpr int BTB_TAG_WIDTH = 32 - BTB_TAG_SHIFT;
constexpr int BHT_CAP = 1 << 8;
constexpr int SELECTOR_CAP = 1 << 8;
constexpr int CONDSEEN_CAP = 1 << 9; // "this PC is a conditional" filter
// GHR lives entirely in the direction-table index domain: every consumer
// folds it in as ((PC>>2) ^ GHR) & (CAP-1), so only the low GHR_WIDTH bits
// are observable. The shift feedback and the checkpoint carrier are sized
// to this width; growing BHT_CAP/SELECTOR_CAP past 2^GHR_WIDTH requires
// growing GHR with them (guarded below).
constexpr int GHR_WIDTH = 8;
static_assert((1 << GHR_WIDTH) >= BHT_CAP &&
                  (1 << GHR_WIDTH) >= SELECTOR_CAP,
              "GHR must cover the direction-table index width");
constexpr uint16_t HISTORY_MASK = (1u << GHR_WIDTH) - 1;
// Arch + speculative RAS pair (mirrors the main tree): the tops are
// non-wrapping depths in [0, RAS_CAP]. Recovery restores the committed
// baseline and replays the surviving ROB prefix through the squash tag.
constexpr int RAS_CAP = 8;
static_assert(RAS_CAP > 0 && RAS_CAP <= 0xFF,
              "RAS top is an 8-bit non-wrapping depth");
constexpr uint8_t PRF_CAP = ROB_CAP + REGISTER_CAP;
// Smallest carrier for a physical-register tag: real tags are 1..PRF_CAP-1
// (P0 is the InvalidPhy sentinel), so bit_width(PRF_CAP-1) bits suffice.
constexpr int PHY_TAG_WIDTH =
    std::bit_width(static_cast<uint32_t>(PRF_CAP - 1));
static_assert(static_cast<uint32_t>(PRF_CAP) <= (1u << PHY_TAG_WIDTH),
              "physical-tag carrier too narrow for PRF_CAP");
// Packed free-list sequence = {1-bit epoch, index}. Only indices
// 0..PRF_CAP-1 are allocated; codes PRF_CAP..PRF_INDEX_MASK are holes.
// For power-of-two capacities the helpers below reduce exactly to masked
// increment and subtraction. Non-power-of-two capacities skip the holes and
// reconstruct logical distance from the epoch and index fields.
template <typename T> constexpr uint8_t PRF_SEQ_BITWIDTH(T cap) {
  return std::bit_width(static_cast<uint32_t>(cap - 1)) + 1;
}
using PrfSeq = uint8_t;
constexpr uint8_t PRF_SEQ_WIDTH = PRF_SEQ_BITWIDTH(PRF_CAP);
constexpr uint8_t PRF_INDEX_WIDTH = PRF_SEQ_WIDTH - 1;
constexpr int PRF_INDEX_MASK = (1 << PRF_INDEX_WIDTH) - 1;
constexpr int PRF_SEQ_MASK = (1 << PRF_SEQ_WIDTH) - 1;

constexpr uint32_t prfSlot(PrfSeq seq) { return seq & PRF_INDEX_MASK; }

constexpr PrfSeq prfSeqNext(PrfSeq seq) {
  return prfSlot(seq) == static_cast<uint32_t>(PRF_CAP) - 1
             ? static_cast<PrfSeq>((~seq & PRF_SEQ_MASK) & ~PRF_INDEX_MASK)
             : static_cast<PrfSeq>((seq + 1) & PRF_SEQ_MASK);
}

constexpr uint32_t prfSeqDistance(PrfSeq from, PrfSeq to) {
  const int32_t indexDistance = static_cast<int32_t>(prfSlot(to)) -
                                static_cast<int32_t>(prfSlot(from));
  const int32_t fromEpoch = (from >> PRF_INDEX_WIDTH) & 1;
  const int32_t toEpoch = (to >> PRF_INDEX_WIDTH) & 1;
  int32_t distance = indexDistance;
  if (toEpoch > fromEpoch)
    distance += PRF_CAP;
  else if (toEpoch < fromEpoch)
    distance -= PRF_CAP;
  if (distance < 0)
    distance += static_cast<int32_t>(PRF_CAP) << 1;
  return static_cast<uint32_t>(distance);
}
static_assert(prfSeqNext(static_cast<PrfSeq>(PRF_CAP - 1)) ==
              static_cast<PrfSeq>(1 << PRF_INDEX_WIDTH));
static_assert(prfSeqNext(static_cast<PrfSeq>(
                  (1 << PRF_INDEX_WIDTH) | (PRF_CAP - 1))) == 0);
static_assert(prfSeqDistance(static_cast<PrfSeq>(PRF_CAP - 1),
                             static_cast<PrfSeq>(1 << PRF_INDEX_WIDTH)) == 1);
static_assert(prfSeqDistance(0,
                             static_cast<PrfSeq>(1 << PRF_INDEX_WIDTH)) ==
              PRF_CAP);
static_assert(INTEGERRS_CAP > 0 && INTEGERRS_CAP <= 16);
static_assert(MULTIPLYRS_CAP > 0 &&
              (MULTIPLYRS_CAP & (MULTIPLYRS_CAP - 1)) == 0);
static_assert(DIVIDERS_CAP > 0 &&
              (DIVIDERS_CAP & (DIVIDERS_CAP - 1)) == 0);
static_assert(BRANCHRS_CAP > 0 && BRANCHRS_CAP <= 4);
static_assert(LQ_CAP >= 2 && LQ_CAP <= 64 && (LQ_CAP & LQ_MASK) == 0);
static_assert(SQ_CAP >= 2 && SQ_CAP <= 64 && (SQ_CAP & SQ_MASK) == 0);
static_assert(MEMQ_SCAN_WINDOW <= SQ_CAP);
static_assert(FQ_CAP >= 2 && (FQ_CAP & (FQ_CAP - 1)) == 0);
static_assert(IQ_CAP >= 2 && (IQ_CAP & (IQ_CAP - 1)) == 0);
static_assert(PRF_CAP > REGISTER_CAP);
static_assert(PRF_SEQ_WIDTH <= 8,
              "PrfSeq is uint8_t: packed sequence must fit in 8 bits");
static_assert(ROB_CAP < (static_cast<uint32_t>(PRF_CAP) << 1),
              "active ROB checkpoints must span less than two PRF rings");
// Sentinel for "no physical register" across the whole phy-tag domain
// (RAT entries, freeList empty slots, Operand.tag immediates, ROB
// oldPhy/newPhy, IssuePacket.phy). Load-bearing invariant: P0 is never
// allocated (freeList only ever holds 32..PRF_CAP-1) and never mapped
// (RAT binds x1-x31 at reset; rd==0 never allocates), so real tags are
// always in 1..PRF_CAP-1 and 0 is unambiguous. Guarded by asserts in PRF::pop,
// PRF::push, RAT mapping writers and IssueArbiter::resolveSrc.
inline constexpr int InvalidPhy = 0;
struct OperandWritePort {
  bool valid = false;
  uint8_t tag = 0;
  int32_t value = 0;
};
using OperandWriteback = std::array<OperandWritePort, 2>;
constexpr int IMEM_CAP = 4;
constexpr int CKPT_CAP = ISSUE_WIDTH == 4 ? 64 : 32;
// IDs are 0..CKPT_CAP-1 and CKPT_CAP is a power of two: derived width is exact.
constexpr int CKPT_ID_WIDTH =
    std::bit_width(static_cast<uint32_t>(CKPT_CAP - 1));
static_assert(static_cast<uint32_t>(CKPT_CAP) == (1u << CKPT_ID_WIDTH),
              "checkpoint carrier must hold every ID exactly");
// RV32 word geometry, shared by instruction-line packing and SRAM lanes.
constexpr int RV32_WORD_BYTES = sizeof(uint32_t);
constexpr int RV32_WORD_BITS = RV32_WORD_BYTES << 3;
constexpr int RV32_WORD_BYTE_BITS =
    std::bit_width(static_cast<uint32_t>(RV32_WORD_BYTES - 1));

// ICache geometry: direct-mapped, so each set is one SRAM row.
constexpr int ICACHE_BLOCK_CAP = 64; // 64 B per line
constexpr int NUM_OF_ICACHE_SETS = 16;
static_assert(ICACHE_BLOCK_CAP >= RV32_WORD_BYTES &&
                  std::has_single_bit(static_cast<uint32_t>(ICACHE_BLOCK_CAP)),
              "ICache line size must be a power of two holding an RV32 word");
static_assert(NUM_OF_ICACHE_SETS >= 2 &&
                  std::has_single_bit(static_cast<uint32_t>(NUM_OF_ICACHE_SETS)),
              "ICache set count must be a power of two with at least two sets");
constexpr int ICACHE_OFFSET_BITS =
    std::bit_width(static_cast<uint32_t>(ICACHE_BLOCK_CAP - 1));
constexpr uint32_t ICACHE_OFFSET_MASK = ICACHE_BLOCK_CAP - 1u;
constexpr int ICACHE_INDEX_BITS =
    std::bit_width(static_cast<uint32_t>(NUM_OF_ICACHE_SETS - 1));
constexpr uint32_t ICACHE_INDEX_MASK = NUM_OF_ICACHE_SETS - 1u;
constexpr int ICACHE_TAG_SHIFT = ICACHE_OFFSET_BITS + ICACHE_INDEX_BITS;
constexpr int ICACHE_TAG_WIDTH = RV32_WORD_BITS - ICACHE_TAG_SHIFT;
constexpr int ICACHE_LINE_BITS = ICACHE_BLOCK_CAP << 3;
constexpr int ICACHE_WORDS_PER_LINE = ICACHE_BLOCK_CAP >> RV32_WORD_BYTE_BITS;
constexpr uint32_t ICACHE_WORD_INDEX_MASK = ICACHE_WORDS_PER_LINE - 1u;
constexpr int ICACHE_WORD_BITS = ICACHE_WORDS_PER_LINE > 1
    ? std::bit_width(static_cast<uint32_t>(ICACHE_WORDS_PER_LINE - 1)) : 1;
static_assert(ICACHE_TAG_WIDTH > 0 && ICACHE_TAG_WIDTH <= RV32_WORD_BITS,
              "ICache address split must leave a valid tag width");
constexpr int REQUEST_CAP = 8;
constexpr int FETCH_REQUEST_CAP = (FETCH_WIDTH == 4) ? 2 : (4 >> (FETCH_WIDTH - 1));
static_assert(FETCH_REQUEST_CAP >= 2 && FETCH_REQUEST_CAP <= 8 && std::has_single_bit(uint32_t(FETCH_REQUEST_CAP)));
static_assert(REQUEST_CAP > 0 && REQUEST_CAP <= 128 &&
                  std::has_single_bit(static_cast<uint32_t>(REQUEST_CAP)),
              "DCache request queue must use power-of-two wrapping");
constexpr int DCACHE_REQUEST_BITS = REQUEST_CAP > 1
    ? std::bit_width(static_cast<uint32_t>(REQUEST_CAP - 1)) : 1;
constexpr int DCACHE_COUNT_BITS =
    std::bit_width(static_cast<uint32_t>(REQUEST_CAP));
constexpr int CKPT_LIVE_MAX =
    ROB_CAP + FETCH_REQUEST_CAP * FETCH_WIDTH + FQ_CAP + IQ_CAP;
static_assert(CKPT_CAP > 0 && (CKPT_CAP & (CKPT_CAP - 1)) == 0,
              "checkpoint wrap uses &(CKPT_CAP-1)");
static_assert(CKPT_CAP >= CKPT_LIVE_MAX,
              "checkpoint IDs must cover ROB + ICache + FQ + IQ");
static_assert(CKPT_CAP <= (1 << 6),
              "checkpoint IDs must fit the retained 6-bit carrier");
// DCache geometry (mirrors main tree common.hpp). Address fields and masks
// are derived from the line size and set count.
constexpr int DCACHE_BLOCK_CAP = 32; // 128 sets x 2 ways = 8 KiB
constexpr int NUM_OF_DCACHE_SETS = 128;
constexpr int NUM_OF_DCACHE_WAYS = 2;
constexpr int DCACHE_MSHR_CAP = 4;
static_assert(REQUEST_CAP <= 8 && DCACHE_MSHR_CAP <= 8);
constexpr int MEM_LATENCY = 20;
constexpr int DCACHE_LINE_BITS = DCACHE_BLOCK_CAP << 3;
// Data array is banked by RV32 word. A refill beat writes two adjacent
// 32-bit banks (64 bits); lookup still reads all banks in parallel.
constexpr int DCACHE_WORDS_PER_LINE =
    DCACHE_BLOCK_CAP >> RV32_WORD_BYTE_BITS;
constexpr int DCACHE_WORD_INDEX_BITS =
    DCACHE_WORDS_PER_LINE > 1
        ? std::bit_width(static_cast<uint32_t>(DCACHE_WORDS_PER_LINE - 1)) : 1;
constexpr int DCACHE_FILL_WORDS_PER_BEAT = DCACHE_WORDS_PER_LINE > 1 ? 2 : 1;
static_assert(DCACHE_WORDS_PER_LINE > 0 &&
                  std::has_single_bit(static_cast<uint32_t>(DCACHE_WORDS_PER_LINE)),
              "DCache line size must hold a whole number of RV32 words");
static_assert(DCACHE_WORDS_PER_LINE == 1 ||
                  (1 << DCACHE_WORD_INDEX_BITS) == DCACHE_WORDS_PER_LINE,
              "DCache word cursor must cover the line exactly");
static_assert(DCACHE_BLOCK_CAP >= RV32_WORD_BYTES &&
                  std::has_single_bit(static_cast<uint32_t>(DCACHE_BLOCK_CAP)),
              "DCache line size must be a power of two holding an RV32 word");
static_assert(NUM_OF_DCACHE_SETS > 0 &&
                  std::has_single_bit(static_cast<uint32_t>(NUM_OF_DCACHE_SETS)),
              "DCache set count must be a positive power of two");
static_assert(NUM_OF_DCACHE_WAYS == 1 || NUM_OF_DCACHE_WAYS == 2 ||
                  NUM_OF_DCACHE_WAYS == 4,
              "DCache supports direct mapping or 2/4-way tree-PLRU");
// Tree-PLRU victim/update helpers (mirror the main tree): 2-way keeps the
// MRU way in bit0; 4-way keeps MRU directions in bit2 (root), bit1 (left),
// bit0 (right), where a set bit means the right child is more recent.
inline uint32_t dcachePlruVictim(uint32_t bits) {
  if (NUM_OF_DCACHE_WAYS == 2) return (bits & 1u) ? 0u : 1u;
  if (NUM_OF_DCACHE_WAYS == 4) {
    const uint32_t root = (bits >> 2) & 1u;
    const uint32_t w1 = root ^ 1u;
    const uint32_t sub = w1 ? (bits & 1u) : ((bits >> 1) & 1u);
    return (w1 << 1) | (sub ^ 1u);
  }
  return 0;
}
inline uint32_t dcachePlruTouch(uint32_t bits, uint32_t way) {
  if (NUM_OF_DCACHE_WAYS == 2) return way & 1u;
  if (NUM_OF_DCACHE_WAYS == 4) {
    const uint32_t w1 = (way >> 1) & 1u;
    const uint32_t w0 = way & 1u;
    uint32_t out = (w1 << 2) | (bits & 0x3u);
    if (w1)
      out = (out & ~0x1u) | w0;
    else
      out = (out & ~0x2u) | (w0 << 1);
    return out;
  }
  return 0;
}
constexpr int DCACHE_OFFSET_BITS =
    std::bit_width(static_cast<uint32_t>(DCACHE_BLOCK_CAP - 1));
constexpr uint32_t DCACHE_OFFSET_MASK = DCACHE_BLOCK_CAP - 1u;
constexpr int DCACHE_INDEX_BITS =
    std::bit_width(static_cast<uint32_t>(NUM_OF_DCACHE_SETS - 1));
constexpr uint32_t DCACHE_INDEX_MASK = NUM_OF_DCACHE_SETS - 1u;
constexpr int DCACHE_CAPACITY_BYTES = DCACHE_BLOCK_CAP <<
    (DCACHE_INDEX_BITS + std::bit_width(static_cast<uint32_t>(NUM_OF_DCACHE_WAYS - 1)));
constexpr int DCACHE_TAG_SHIFT = DCACHE_OFFSET_BITS + DCACHE_INDEX_BITS;
constexpr int DCACHE_TAG_WIDTH = RV32_WORD_BITS - DCACHE_TAG_SHIFT;
constexpr int DCACHE_WAY_BITS =
    std::bit_width(static_cast<uint32_t>(NUM_OF_DCACHE_WAYS - 1));
constexpr int DCACHE_PLRU_BITS = NUM_OF_DCACHE_WAYS - 1;
static_assert(DCACHE_TAG_WIDTH > 0 && DCACHE_TAG_WIDTH <= RV32_WORD_BITS,
              "DCache address split must leave a valid tag width");
// Metadata SRAM holds tag+dirty only; line validity is a register array.
constexpr int DCACHE_META_BITS = DCACHE_TAG_WIDTH + 1;
static_assert(DCACHE_META_BITS <= RV32_WORD_BITS,
              "DCache metadata must fit one SRAM lane");
constexpr uint32_t DCACHE_META_DIRTY = 1u << DCACHE_TAG_WIDTH;
constexpr uint32_t DCACHE_META_TAG_MASK = DCACHE_META_DIRTY - 1u;

// AXI4-Lite memory subsystem (mirrors RISC-V-CPU-2026/docs/axi4-lite.md and
// scripts/sim.cpp): depth-16 FIFO per channel, fixed service latency,
// OKAY/SLVERR/DECERR response codes and the 0x80000000 exit MMIO address.
constexpr int AXI_DEPTH = 16; // External Slave channels; not master line credits.
constexpr int AXI_DATA_CAP = 8;
constexpr int AXI_ORDER_CAP = 16; // Preserve the RTL's 4-bit order-ring indices.
static_assert(AXI_DATA_CAP >= (DCACHE_MSHR_CAP << 1));
static_assert(IMEM_CAP >= FETCH_REQUEST_CAP && IMEM_CAP <= 8 && std::has_single_bit(uint32_t(IMEM_CAP)));
constexpr int AXI_LATENCY = 10;
constexpr uint32_t AXI_OKAY = 0;
constexpr uint32_t AXI_SLVERR = 2;
constexpr uint32_t AXI_DECERR = 3;
constexpr uint32_t AXI_EXIT_ADDR = 0x80000000u;
constexpr std::size_t AXI_RAM_BYTES = static_cast<std::size_t>(256) << 20;

// Select two words, rotate by byte, then truncate; no line-wide barrel shifter.
inline uint32_t extractLoadWord(uint32_t low, uint32_t high, unsigned offset,
                                unsigned nBytes) {
  uint32_t raw = low;
  switch (offset & 3u) {
  case 1: raw = (low >> 8) | (high << 24); break;
  case 2: raw = (low >> 16) | (high << 16); break;
  case 3: raw = (low >> 24) | (high << 8); break;
  default: break;
  }
  return nBytes == 1 ? raw & 0xffu : nBytes == 2 ? raw & 0xffffu : raw;
}

inline uint32_t extendLoad(uint32_t raw, unsigned nBytes, bool isUnsigned) {
  if (nBytes == 1) {
    raw &= 0xffu;
    if (!isUnsigned && (raw & 0x80u)) raw |= 0xffffff00u;
  } else if (nBytes == 2) {
    raw &= 0xffffu;
    if (!isUnsigned && (raw & 0x8000u)) raw |= 0xffff0000u;
  }
  return raw;
}

// Preserve unaligned accesses and unsigned address-wrap boundary semantics.
inline bool byteOverlap(uint32_t loadAddress, unsigned loadBytes,
                        uint32_t storeAddress, unsigned storeBytes) {
  const unsigned lo = loadAddress & 3u, so = storeAddress & 3u;
  const uint32_t lastWord = 0x3fffffffu;
  const uint32_t delta = ((loadAddress >> 2) - (storeAddress >> 2)) & lastWord;
  if (delta == 0) return lo < so + storeBytes && so < lo + loadBytes;
  if (delta == 1 && (storeAddress >> 2) != lastWord) return lo + 4u < so + storeBytes;
  if (delta == lastWord && (loadAddress >> 2) != lastWord) return so + 4u < lo + loadBytes;
  return false;
}

enum class ValueState : uint32_t{
  NOTREADY,
  FETCHING,
  READY,
};

enum class Operation {
  OP_INVALID,
  ADD,
  SUB,
  MUL,
  MULH,
  MULHU,
  MULHSU,
  DIV,
  DIVU,
  REM,
  REMU,
  AND,
  OR,
  XOR,
  SL,
  SRL,
  SRA,
  SLT,
  SLTU,
  AUIPC,
  LUI,
  EQ,
  GE,
  GEU,
  LT,
  LTU,
  NE,
  Load,
  Store,
  JALR,
};
constexpr bool isControlOp(Operation op) { return op == Operation::JALR; }
enum class RISC_V {
  R,
  I,
  M, // M-extension (opcode 0x33, funct7 == 1); values match the main tree
  Istar,
  S,
  B,
  U,
  J,
  RV_INVALID,
};

struct SquashInfo {
  bool needSquash = false;
  RobTag SquashTag = 0;
  uint32_t SquashPC = 0;
  uint8_t CkptId = 0;
};

struct Operand {
  int tag = InvalidPhy;
  int32_t imm = 0;
};

struct PredictInfo {
  bool taken;
  uint32_t predictPC;
  bool btbHit = false;
  bool unconditional = false;
  bool condSeen = false; // filter says this PC resolved as conditional before
};

struct BPUSnapshot {
  // RAS recovery starts from archRAS and replays the surviving ROB prefix, so
  // a branch checkpoint only needs the speculative direction history.
  uint8_t GHR_snapshot;
};

struct Uop {
  RISC_V type = RISC_V::RV_INVALID;
  int opcode = 0;
  int funct3 = 0;
  int funct7 = 0;
  int rd = 0;
  int rs1 = 0;
  int rs2 = 0;
  int32_t imm = 0;
  uint32_t pc = 0;
  bool isHalt = false;
  bool allocDest = false;
  int32_t predictedPC = 0;
  uint8_t ckptId = 0;
};
enum class RSType { Integer, Multiply, Divide, Branch, Load, StoreAddr };
class ROB;
class PRF;
struct BPU;

// IMEM -> ICache line-return bus: one complete instruction line as an RV32
// word bundle, combinational over committed producer state (Wire, not
// Register, so it adds no latency). Word count/index follow ICache geometry.
struct LineReturn {
  Wire<1> valid;
  Wire<32> lineAddr;
  std::array<Wire<RV32_WORD_BITS>, ICACHE_WORDS_PER_LINE> data;
};

// Pre-decode scan result carried from the FQ push to the BPU. Used for RAS
// maintenance and early BTB training, independent of prediction-table hits.
struct FetchTypeInfo {
  bool valid = false;
  bool isCall = false;
  bool isRet = false;
  bool jalTargetValid = false;
  uint32_t pc = 0;
  uint32_t jalTarget = 0;
};
