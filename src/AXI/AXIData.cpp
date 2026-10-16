#include "../include/AXIData.hpp"
#include <cstdint>

AXIData::AXIData() {
    arValid = [this]() -> uint32_t {
        return (static_cast<bool>(readActive) &&
                static_cast<uint32_t>(readIssued) < DCACHE_WORDS_PER_LINE)
                   ? 1u
                   : 0u;
    };
    arAddr = [this]() -> uint32_t {
        return static_cast<uint32_t>(readAddr) +
               (static_cast<uint32_t>(readIssued) << RV32_WORD_BYTE_BITS);
    };
    rReady = [this]() -> uint32_t { return 1u; };
    awValid = [this]() -> uint32_t {
        return (static_cast<bool>(writeActive) &&
                static_cast<uint32_t>(writeAw) < DCACHE_WORDS_PER_LINE)
                   ? 1u
                   : 0u;
    };
    awAddr = [this]() -> uint32_t {
        return static_cast<uint32_t>(writeAddr) +
               (static_cast<uint32_t>(writeAw) << RV32_WORD_BYTE_BITS);
    };
    wValid = [this]() -> uint32_t {
        return (static_cast<bool>(writeActive) &&
                static_cast<uint32_t>(writeW) < DCACHE_WORDS_PER_LINE)
                   ? 1u
                   : 0u;
    };
    wData = [this]() -> uint32_t {
        const uint32_t idx = static_cast<uint32_t>(writeW);
        return idx < DCACHE_WORDS_PER_LINE
                   ? static_cast<uint32_t>(writeWords[idx])
                   : 0u;
    };
    // The latched victim line is a full cache line: every byte lane is valid.
    wStrb = [this]() -> uint32_t { return 0xfu; };
    bReady = [this]() -> uint32_t {
        return static_cast<bool>(writeActive) ? 1u : 0u;
    };
}

uint8_t AXIData::lineByte(int i) const {
    const uint32_t word =
        static_cast<uint32_t>(readWords[i >> RV32_WORD_BYTE_BITS]);
    return static_cast<uint8_t>(word >> ((i & (RV32_WORD_BYTES - 1)) << 3));
}

void AXIData::work() {
    const bool fillAccept = static_cast<bool>(readValid) &&
                            !static_cast<bool>(readActive) &&
                            !static_cast<bool>(replyValid);
    const bool writeAccept =
        static_cast<bool>(writeValid) && !static_cast<bool>(writeActive);
    const bool arFire =
        static_cast<bool>(arValid) && static_cast<bool>(arReady);
    const bool rBeat = static_cast<bool>(rValid) && static_cast<bool>(rReady);
    const bool awFire =
        static_cast<bool>(awValid) && static_cast<bool>(awReady);
    const bool wFire = static_cast<bool>(wValid) && static_cast<bool>(wReady);
    const bool bFire = static_cast<bool>(bValid) && static_cast<bool>(bReady);

    // Fill port: latch the line read, burst it on AR, buffer the reply until
    // the DCache drain (replyConsumed) releases the port.
    if (fillAccept) {
        readActive <= 1;
        readAddr <= static_cast<uint32_t>(readAddress);
        readIssued <= 0;
        readCollected <= 0;
    }
    if (arFire)
        readIssued <= static_cast<uint32_t>(readIssued) + 1u;
    if (rBeat) {
        const uint32_t c = static_cast<uint32_t>(readCollected);
        readWords[c] <= static_cast<uint32_t>(rData);
        readCollected <= c + 1u;
        if (c + 1u == DCACHE_WORDS_PER_LINE) {
            replyValid <= 1;
            readActive <= 0;
        }
    }
    if (static_cast<bool>(replyValid) && static_cast<bool>(replyConsumed))
        replyValid <= 0;

    // Writeback port: latch the victim line, burst it on AW/W, and release the
    // port once the last write response returns.
    if (writeAccept) {
        writeActive <= 1;
        writeAddr <= static_cast<uint32_t>(writeAddress);
        writeAw <= 0;
        writeW <= 0;
        writeB <= 0;
        for (int i = 0; i < DCACHE_WORDS_PER_LINE; ++i) {
            uint32_t w = 0;
            for (int b = 0; b < RV32_WORD_BYTES; ++b)
                w |= static_cast<uint32_t>(
                         writeLineData[(i << RV32_WORD_BYTE_BITS) + b])
                     << (b << 3);
            writeWords[i] <= w;
        }
    }
    if (awFire)
        writeAw <= static_cast<uint32_t>(writeAw) + 1u;
    if (wFire)
        writeW <= static_cast<uint32_t>(writeW) + 1u;
    if (bFire) {
        const uint32_t done = static_cast<uint32_t>(writeB) + 1u;
        writeB <= done;
        if (done == DCACHE_WORDS_PER_LINE)
            writeActive <= 0;
    }
}
