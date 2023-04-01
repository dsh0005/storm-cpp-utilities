#!/bin/sh

BASIC_CXX_FLAGS="-std=c++20 -U_FORTIFY_SOURCE"
BASIC_LINK_FLAGS=""
BASIC_WARN_FLAGS="-Wall -Wextra -pedantic"

PIE_FLAGS="-pie -fno-plt"
FORTIFY_FLAGS="-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"
RELRO_FLAGS="-Wl,-z,relro,-z,now"

INCLUDES="-I./containers"

# build tests

printf 'building normal tests: '
g++ $BASIC_CXX_FLAGS $BASIC_LINK_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests
printf 'done!\n'

printf 'building ASAN tests: '
g++ $BASIC_CXX_FLAGS $BASIC_LINK_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  -fsanitize=address -fno-omit-frame-pointer \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests_asan
printf 'done!\n'

printf 'building TSAN tests: '
g++ $BASIC_CXX_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  -fsanitize=thread \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests_tsan
printf 'done!\n'

printf 'building UBSAN tests: '
g++ $BASIC_CXX_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -Og -ggdb3 $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  -fsanitize=undefined \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests_ubsan
printf 'done!\n'

printf 'building microbenchmark: '
g++ $BASIC_CXX_FLAGS $BASIC_WARN_FLAGS $INCLUDES \
  -O2 -g $FORTIFY_FLAGS $PIE_FLAGS $RELRO_FLAGS \
  test/mpmc_bench.cpp \
  -o build/mpmc_bench
printf 'done!\n'
