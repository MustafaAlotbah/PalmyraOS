
#pragma once

#include <cstdint>


namespace PalmyraOS::Userland::builtin::MenuBar
{

  /**
   * @brief Entry point for the MenuBar application.
   * This function serves as the main entry point for the MenuBar, similar to the menu bar in Linux.
   *
   * @param argc The number of command-line arguments.
   * @param argv The array of command-line arguments.
   * @return This function does not return as it is marked with [[noreturn]].
   */
  [[noreturn]] int main(uint32_t argc, char* argv[]);
}

