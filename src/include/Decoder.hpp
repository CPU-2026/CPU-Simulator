#pragma once
#include "common.h"
#include "tools.h"
#include <array>
#include <cstdint>

static_assert(IQ_CAP == (1 << IQ_PTR_WIDTH));    // head/tail carrier is exact
static_assert(CKPT_CAP == (1 << CKPT_ID_WIDTH)); // ckptId carrier is exact

class Decoder {
  public:
    static Uop decode(int32_t raw_inst);
    static inline int32_t signExtend(int32_t raw_data, int len) {
        return (raw_data << (32 - len)) >> (32 - len);
    }
};

struct DecodeInput {
    Wire<1> needSquash;
    Wire<2> issuePopCount;   // 0..2: entries popped by issue this cycle
    Wire<2> decodePushCount; // 0..2: entries decoded from FQ this cycle
    Wire<32> fqHeadRaw;
    Wire<32> fqHeadPc;
    Wire<32> fqHeadPredictedPC;
    Wire<CKPT_ID_WIDTH> fqHeadCkptId;
    Wire<32> fqHeadRaw1;
    Wire<32> fqHeadPc1;
    Wire<32> fqHeadPredictedPC1;
    Wire<CKPT_ID_WIDTH> fqHeadCkptId1;
};

struct UopEntry {
    Register<4> type; // RISC_V (R..RV_INVALID, 9 values)
    Register<7> opcode;
    Register<3> funct3;
    Register<7> funct7;
    Register<5> rd;
    Register<5> rs1;
    Register<5> rs2;
    Register<32> imm;
    Register<32> pc;
    Register<1> isHalt;
    Register<1> allocDest;
    Register<32> predictedPC;
    Register<CKPT_ID_WIDTH> ckptId;
};

struct IQOutput {
    Register<IQ_SEQ_WIDTH> head;
    Register<IQ_SEQ_WIDTH> tail;
};

struct IQInner {
    std::array<UopEntry, IQ_CAP> entries;
};

// Decoded-instruction queue (formerly InstructQueue wrapped by DecodeUnit).
struct DecodeUnit : dark::Module<DecodeInput, IQOutput, IQInner> {
    void work() override;

    // bridge accessors: combinational reads of the committed (_M_old) state
    unsigned count() const {
        return (static_cast<uint32_t>(tail) - static_cast<uint32_t>(head)) &
               IQ_SEQ_MASK;
    }
    bool isEmpty() const { return count() == 0u; }
    bool isFull() const { return count() == static_cast<unsigned>(IQ_CAP); }
    unsigned pushCapacity(bool willPop) const {
        return static_cast<unsigned>(IQ_CAP) - count() + (willPop ? 1u : 0u);
    }
    RISC_V headType() const {
        return static_cast<RISC_V>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].type));
    }
    int headOpcode() const {
        return static_cast<int>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].opcode));
    }
    int headFunct3() const {
        return static_cast<int>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].funct3));
    }
    int headFunct7() const {
        return static_cast<int>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].funct7));
    }
    int headRd() const {
        return static_cast<int>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].rd));
    }
    int headRs1() const {
        return static_cast<int>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].rs1));
    }
    int headRs2() const {
        return static_cast<int>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].rs2));
    }
    int32_t headImm() const {
        return static_cast<int32_t>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].imm));
    }
    uint32_t headPc() const {
        return static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].pc);
    }
    bool headIsHalt() const {
        return static_cast<bool>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].isHalt);
    }
    bool headAllocDest() const {
        return static_cast<bool>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].allocDest);
    }
    int32_t headPredictedPC() const {
        return static_cast<int32_t>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].predictedPC));
    }
    uint8_t headCkptId() const {
        return static_cast<uint8_t>(static_cast<uint32_t>(
            entries[static_cast<uint32_t>(head) & (IQ_CAP - 1)].ckptId));
    }
    // Second decoded head (ISSUE_WIDTH == 2): same fields at head+1.
    const UopEntry &entryAt(unsigned offset) const {
        return entries[(static_cast<uint32_t>(head) + offset) & (IQ_CAP - 1)];
    }
    RISC_V headType1() const {
        return static_cast<RISC_V>(static_cast<uint32_t>(entryAt(1).type));
    }
    int headOpcode1() const {
        return static_cast<int>(static_cast<uint32_t>(entryAt(1).opcode));
    }
    int headFunct31() const {
        return static_cast<int>(static_cast<uint32_t>(entryAt(1).funct3));
    }
    int headFunct71() const {
        return static_cast<int>(static_cast<uint32_t>(entryAt(1).funct7));
    }
    int headRd1() const {
        return static_cast<int>(static_cast<uint32_t>(entryAt(1).rd));
    }
    int headRs11() const {
        return static_cast<int>(static_cast<uint32_t>(entryAt(1).rs1));
    }
    int headRs21() const {
        return static_cast<int>(static_cast<uint32_t>(entryAt(1).rs2));
    }
    int32_t headImm1() const {
        return static_cast<int32_t>(static_cast<uint32_t>(entryAt(1).imm));
    }
    uint32_t headPc1() const { return static_cast<uint32_t>(entryAt(1).pc); }
    bool headIsHalt1() const { return static_cast<bool>(entryAt(1).isHalt); }
    bool headAllocDest1() const {
        return static_cast<bool>(entryAt(1).allocDest);
    }
    int32_t headPredictedPC1() const {
        return static_cast<int32_t>(
            static_cast<uint32_t>(entryAt(1).predictedPC));
    }
    uint8_t headCkptId1() const {
        return static_cast<uint8_t>(static_cast<uint32_t>(entryAt(1).ckptId));
    }
};
