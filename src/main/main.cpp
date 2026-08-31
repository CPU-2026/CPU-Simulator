#include "../include/CPU.hpp"
int main() {
  Memory mem;
  mem.load_ins();
  CPU cpu(mem);
  cpu.run();
}