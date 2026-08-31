#pragma once
#ifndef MEMORY_HPP
#define MEMORY_HPP
#include <cstdint>
#include <cstring>
#include <iostream>

static constexpr uint32_t MEM_SIZE = 128 * 1024;

// Base class shared by IMEM (instruction memory) and DMEM (data memory).
// It owns only the byte-addressable storage array and the functions that
// operate on raw memory bytes. All pipeline state (busy/buffer/request
// tracking) belongs to the concrete DMEM/IMEM modules.
class Memory {
protected:
  uint8_t* mem;
  
public:
  Memory() : mem(new uint8_t[MEM_SIZE]()) {}
  ~Memory() { delete[] mem; }
  Memory(const Memory& other) : mem(new uint8_t[MEM_SIZE]) {
    std::memcpy(mem, other.mem, MEM_SIZE);
  }
  Memory& operator=(const Memory& other) {
    if (this != &other) {
      std::memcpy(mem, other.mem, MEM_SIZE);
    }
    return *this;
  }

  void load_ins() {
    while (!std::cin.eof()) {
      char sign = std::cin.get();
      if (sign == EOF) {
        return;
      }
      if (sign == '@') {
        char hex_address[9];
        std::cin >> hex_address;
        uint32_t cur_address = hex2uint32(8, hex_address);
        char hex_byte[3];
        while (std::cin >> hex_byte) {
          write_data(cur_address++, hex2uint32(2, hex_byte));
          while (std::cin.peek() == '\n' || std::cin.peek() == ' ') {
            std::cin.get();
          }
          if (std::cin.peek() == '@')
            break;
        }
      }
    }
  }

  inline uint32_t hex2uint32(int len, char hex[]) const;
  
  uint8_t read_data(uint32_t addr) const {
    return addr < MEM_SIZE ? mem[addr] : 0;
  }
  void write_data(uint32_t addr, uint8_t data) {
    if (addr < MEM_SIZE)
      mem[addr] = data;
  }
  bool operator==(const Memory &other) const {
    return std::memcmp(mem, other.mem, MEM_SIZE) == 0;
  }
};
inline uint32_t Memory::hex2uint32(int len, char hex[]) const {
  uint32_t result = 0;
  for (int i = 0; i < len; i++) {
    result *= 0x10;
    uint32_t value =
        (hex[i] >= '0' && hex[i] <= '9') ? (hex[i] - '0') : (hex[i] - 'A' + 10);
    result += value;
  }
  return result;
}
#endif // MEMORY_HPP
