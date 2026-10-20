#pragma once
#include "common.h"
#include "module.h"
#include <array>

#ifndef CPU_COMMIT_WIDTH
#define CPU_COMMIT_WIDTH 2
#endif
constexpr unsigned COMMIT_WIDTH = CPU_COMMIT_WIDTH;
static_assert(COMMIT_WIDTH >= 1 && COMMIT_WIDTH <= 2);

// Every retirement consumer observes the same accepted, ordered prefix.
struct CommitPort {
  Wire<1> valid;
  Wire<ROB_TAG_WIDTH> tag;
  Wire<2> type;
  Wire<5> rd;
  Wire<PHY_TAG_WIDTH> newPhy;
  Wire<PHY_TAG_WIDTH> oldPhy;
  Wire<32> pc;
  Wire<1> halt;
  Wire<1> exception;
  Wire<1> pop;
  Wire<1> push;
  Wire<1> exits;
};

inline void connectCommit(CommitPort &dest, const CommitPort &source) {
  dest.valid = [&source]() { return uint32_t(source.valid); };
  dest.tag = [&source]() { return uint32_t(source.tag); };
  dest.type = [&source]() { return uint32_t(source.type); };
  dest.rd = [&source]() { return uint32_t(source.rd); };
  dest.newPhy = [&source]() { return uint32_t(source.newPhy); };
  dest.oldPhy = [&source]() { return uint32_t(source.oldPhy); };
  dest.pc = [&source]() { return uint32_t(source.pc); };
  dest.halt = [&source]() { return uint32_t(source.halt); };
  dest.exception = [&source]() { return uint32_t(source.exception); };
  dest.pop = [&source]() { return uint32_t(source.pop); };
  dest.push = [&source]() { return uint32_t(source.push); };
  dest.exits = [&source]() { return uint32_t(source.exits); };
}
