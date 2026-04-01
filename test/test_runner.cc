/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "test_runner.h"

/*-----------------------------------------------------------------------------
 * Function: main
 *---------------------------------------------------------------------------*/
/**
 * @brief Run all registered tests and print pass/fail summary.
 */
int main() {
  int passed = 0, failed = 0;

  for (auto& t : __TestRegistry()) {
    try {
      t.fn();
      std::cout << "[PASS] " << t.name << "\n";
      ++passed;
    } catch (const std::exception& e) {
      std::cout << "[FAIL] " << t.name << " : " << e.what() << "\n";
      ++failed;
    } catch (...) {
      std::cout << "[FAIL] " << t.name << " : unknown error\n";
      ++failed;
    }
  }

  std::cout << "\nSummary: " << passed << " passed, " << failed << " failed\n";
  return failed ? 1 : 0;
}