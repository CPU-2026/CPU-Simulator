#pragma once
#include "module.h"
#include "tools.h"
#include "util.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

// sram_fakeram 的模板树模型，契约：RISC-V-CPU-2026/docs/sram.md。
// work() 采样字索引及单端口读写，sync() 后可读到结果；嵌套使用时须显式调用 work()。
// 写粒度受 Wire/Register 的 32 位限制；禁用/写操作不会产生新的有效读结果。
template <std::size_t DEPTH, std::size_t WIDTH, std::size_t WRITE_GRANULARITY = WIDTH>
struct SRAMInput {
  static constexpr std::size_t Lanes = WIDTH / WRITE_GRANULARITY;
  static constexpr std::size_t AddressWidth =
      std::max<std::size_t>(1, std::bit_width(DEPTH - 1));
  Wire<1> enable;
  Wire<1> writeEnable;
  std::array<Wire<1>, Lanes> writeMask;
  Wire<AddressWidth> addr;
  std::array<Wire<WRITE_GRANULARITY>, Lanes> writeData;
};
template <std::size_t WIDTH, std::size_t WRITE_GRANULARITY>
struct SRAMOutput {
  static constexpr std::size_t Lanes = WIDTH / WRITE_GRANULARITY;
  std::array<Register<WRITE_GRANULARITY>, Lanes> readData;
};
template <std::size_t DEPTH, std::size_t WIDTH, std::size_t WRITE_GRANULARITY>
struct SRAMInner {
  static constexpr std::size_t Lanes = WIDTH / WRITE_GRANULARITY;
  std::array<std::array<std::uint32_t, Lanes>, DEPTH> words{};
  // 阵列是普通状态，不参与 Register 的 sync()。
  void sync() {}
};
template <std::size_t DEPTH, std::size_t WIDTH, std::size_t WRITE_GRANULARITY = WIDTH>
struct SRAM : public dark::Module<SRAMInput<DEPTH, WIDTH, WRITE_GRANULARITY>,
                                  SRAMOutput<WIDTH, WRITE_GRANULARITY>,
                                  SRAMInner<DEPTH, WIDTH, WRITE_GRANULARITY>> {
  static constexpr std::size_t Lanes = WIDTH / WRITE_GRANULARITY;
  static_assert(DEPTH >= 1 && DEPTH <= 1048576 && WIDTH >= 1 && WIDTH <= 4096 &&
                    DEPTH <= (std::size_t{1} << 24) / WIDTH,
                "sram_fakeram: invalid depth, width or capacity");
  static_assert(WRITE_GRANULARITY >= 1 && WRITE_GRANULARITY <= 32 &&
                    WRITE_GRANULARITY <= WIDTH && WIDTH % WRITE_GRANULARITY == 0,
                "sram_fakeram: invalid write granularity");

public:
  void work() override {
    const std::uint32_t index = static_cast<std::uint32_t>(this->addr);
    const bool enable = static_cast<bool>(this->enable);
    const bool isWrite = static_cast<bool>(this->writeEnable);

    if (enable && index >= DEPTH) {
      debug::print("RAM library error: addr=%u outside sram_fakeram DEPTH=%llu\n",
                   index, static_cast<unsigned long long>(DEPTH));
      std::exit(EXIT_FAILURE);
    }
    if (enable && !isWrite) {
      for (std::size_t lane = 0; lane < Lanes; ++lane) {
        this->readData[lane] <= this->words[index][lane];
      }
    } else if (enable) {
      for (std::size_t lane = 0; lane < Lanes; ++lane) {
        if (static_cast<bool>(this->writeMask[lane])) {
          this->words[index][lane] = static_cast<std::uint32_t>(this->writeData[lane]);
        }
      }
    }
  }

  std::uint32_t readLane(std::size_t lane) const {
    return static_cast<std::uint32_t>(this->readData[lane]);
  }
};
