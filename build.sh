#!/bin/sh

BASIC_CXX_FLAGS="-std=c++20 -U_FORTIFY_SOURCE"
BASIC_LINK_FLAGS=""
BASIC_WARN_FLAGS="-Wall -Wextra -pedantic"

PIE_FLAGS="-pie -fno-plt"
FORTIFY_FLAGS="-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"
RELRO_FLAGS="-Wl,-z,relro,-z,now"

INCLUDES="-I./containers"

# build tests
g++ $BASIC_CXX_FLAGS $BASIC_LINK_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests

g++ $BASIC_CXX_FLAGS $BASIC_LINK_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  -fsanitize=address -fno-omit-frame-pointer \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests_asan

g++ $BASIC_CXX_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  -fsanitize=thread \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests_tsan
