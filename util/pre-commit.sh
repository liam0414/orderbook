#!/bin/bash

# Check compilation
cmake -B build && cd build && make
if [[ "$?" != 0 ]]; then
  echo -e "\033[0;31m"
  echo "================================================================================"
  echo "= Failed to configure CMake and successfull build the code                     ="
  echo -n "$(tput bold)"
  echo "= Commit rejected                                                              ="
  echo -n "$(tput sgr0)"
  echo -e -n "\033[0;31m"
  echo "================================================================================"
  echo -e "\033[0m"
  exit 1
fi

# Check clang-format
find ../src -iname "*.h" -o -iname "*.cpp" | xargs clang-format -n -Werror
if [[ "$?" != 0 ]]; then
  echo -e "\033[0;31m"
  echo "================================================================================"
  echo "= Looks like clang-format errors. Solve them now to avoid having large commits ="
  echo -n "$(tput bold)"
  echo "= Commit rejected                                                              ="
  echo -n "$(tput sgr0)"
  echo -e -n "\033[0;31m"
  echo "================================================================================"
  echo -e "\033[0m"
  exit 1
fi