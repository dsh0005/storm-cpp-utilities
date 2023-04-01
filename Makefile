FORTIFY_FLAGS=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
PIE_FLAGS=-pie -fno-plt
RELRO_FLAGS=-Wl,-z,relro,-z,now
WARN_FLAGS=-Wall -Wextra -pedantic

DEBUG_EXTRA_FLAGS=-Og -ggdb3
BENCH_EXTRA_FLAGS=-O2 -march=native
LINK_EXTRA_FLAGS=

INCLUDES=-I./containers

CXX=g++
CXXFLAGS=-std=c++20

OBJDIR=build
TESTSDIR=test

TESTFLAGS=$(CXXFLAGS) $(LINK_EXTRA_FLAGS) $(WARN_FLAGS) $(INCLUDES) $(FORTIFY_FLAGS) $(PIE_FLAGS) $(RELRO_FLAGS) $(DEBUG_EXTRA_FLAGS)
BENCHFLAGS=$(CXXFLAGS) $(LINK_EXTRA_FLAGS) $(WARN_FLAGS) $(INCLUDES) $(FORTIFY_FLAGS) $(PIE_FLAGS) $(RELRO_FLAGS) $(BENCH_EXTRA_FLAGS)

all: tests benchmarks

.SUFFIXES:

$(OBJDIR)/mpmc_vanilla_test: $(TESTSDIR)/mpmc_queue_tests.cpp containers/mpmc_queue.hpp
	$(CXX) $(TESTFLAGS) $< -o $@

$(OBJDIR)/mpmc_asan_test: $(TESTSDIR)/mpmc_queue_tests.cpp containers/mpmc_queue.hpp
	$(CXX) $(TESTFLAGS) -fsanitize=address -fno-omit-frame-pointer $< -o $@

$(OBJDIR)/mpmc_tsan_test: $(TESTSDIR)/mpmc_queue_tests.cpp containers/mpmc_queue.hpp
	$(CXX) $(TESTFLAGS) -fsanitize=thread -fno-omit-frame-pointer $< -o $@

$(OBJDIR)/mpmc_ubsan_test: $(TESTSDIR)/mpmc_queue_tests.cpp containers/mpmc_queue.hpp
	$(CXX) $(TESTFLAGS) -fsanitize=undefined -fno-omit-frame-pointer $< -o $@

$(OBJDIR)/mpmc_bench: $(TESTSDIR)/mpmc_bench.cpp containers/mpmc_queue.hpp
	$(CXX) $(BENCHFLAGS) $< -o $@

tests: $(OBJDIR)/mpmc_vanilla_test $(OBJDIR)/mpmc_asan_test $(OBJDIR)/mpmc_tsan_test $(OBJDIR)/mpmc_ubsan_test

benchmarks: $(OBJDIR)/mpmc_bench

clean:
	-rm $(OBJDIR)/*

.DEFAULT:
.DEFAULT: all

.PHONY: clean all tests benchmarks
