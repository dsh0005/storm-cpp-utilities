#!/bin/sh

# build tests
g++ -Og -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -Wall -Wextra -pedantic -pie \
  -fno-plt -Wl,-z,relro,-z,now \
  -I./containers \
  test/mpmc_queue_tests.cpp \
  -o build/mpmc_queue_tests
