// SPDX-License-Identifier: AGPL-3.0-only

/* mpmc_test_helpers: Test/benchmark helpers for mpmc_queue.
 * Copyright 2023, Douglas Storm Hill
 *
 * Provided under AGPLv3 (only), see LICENSE file
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef STORM_MPMC_TEST_HELPERS_H
#define STORM_MPMC_TEST_HELPERS_H 1

#include <memory>
#include <functional>
#include <thread>
#include <future>
#include <latch>
#include <vector>
#include <chrono>

#include <ctime>

#include "mpmc_queue.hpp"

// Compiler barrier macro to make sure it does the work we ask for.
// At least for GCC, having no outputs makes it implicitly __volatile__.
// Also add comments so it's easier to see in e.g. Godbolt.
#define barrier() do { __asm__("# barrier()":::"memory"); } while(0)
#define consume_value_reg(x) do { __asm__("# consuming: %0":: "r" (x)); } while(0)

namespace storm {
	namespace test {

// Typedef for the test functions that are on the producer side.
template<typename T>
using producer_test_function =
	std::function<void(
		std::shared_ptr<mpmc_queue<T>>, // The queue to put into
		int,                            // how many items to put in
		T,                              // what value to put in
		std::latch&,                    // setup done signal
		std::latch&,                    // start signal
		std::latch&)>;                  // stop signal

// Typedef for the test functions that are on the consumer side.
template<typename T>
using consumer_test_function =
	std::function<void(
		std::shared_ptr<mpmc_queue<T>>, // The queue to take from
		int,                            // how many items to take
		std::latch&,                    // setup done signal
		std::latch&,                    // start signal
		std::latch&)>;                  // stop signal

// put n items into q
// value: the value to put copies of into q
template<typename T>
static void normal_producer(
		const std::shared_ptr<mpmc_queue<T>> q,
		const int n,
		const T value,
		std::latch &setup_done,
		std::latch &start_signal,
		std::latch &stop_signal
		){
	setup_done.arrive_and_wait();
	start_signal.arrive_and_wait();

	for(int i = 0; i < n; i++){
		q->push(value);
	}

	stop_signal.arrive_and_wait();
}

// pop n items from q, using pop_wait
template<typename T>
static void normal_consumer(
		const std::shared_ptr<mpmc_queue<T>> q,
		const int n,
		std::latch &setup_done,
		std::latch &start_signal,
		std::latch &stop_signal
		){
	setup_done.arrive_and_wait();
	start_signal.arrive_and_wait();

	for(int i = 0; i < n; i++){
		[[maybe_unused]] const T loc = q->pop_wait();
		consume_value_reg(loc);
	}

	stop_signal.arrive_and_wait();
}

// simulate putting n items into q, but do it to a local stub
// value: the value to put copies of into q
template<typename T>
static void stub_producer(
		[[maybe_unused]] const std::shared_ptr<mpmc_queue<T>> unused_q,
		const int n,
		const T value,
		std::latch &setup_done,
		std::latch &start_signal,
		std::latch &stop_signal
		){
	// here's the local queue that we use as a surrogate
	auto q = std::make_shared<std::queue<T>>();

	setup_done.arrive_and_wait();

	start_signal.arrive_and_wait();

	for(int i = 0; i < n; i++){
		q->push(value);
	}

	barrier();

	stop_signal.arrive_and_wait();
}

// simulate popping n items from q, but do it to a local stub
template<typename T>
static void stub_consumer(
		[[maybe_unused]] const std::shared_ptr<mpmc_queue<T>> unused_q,
		const int n,
		std::latch &setup_done,
		std::latch &start_signal,
		std::latch &stop_signal
		){
	// Here's the local queue that we use as a surrogate
	auto q = std::make_shared<std::queue<T>>();

	// We need to fill it up first.
	for(int i = 0; i < n; i++){
		q->push(T());
	}

	setup_done.arrive_and_wait();

	start_signal.arrive_and_wait();

	for(int i = 0; i < n; i++){
		[[maybe_unused]] T loc = std::move(q->front());
		consume_value_reg(loc);
		q->pop();
	}

	stop_signal.arrive_and_wait();
}

// How much time was taken by a benchmark.
struct concurrency_test_time {
	std::chrono::steady_clock::duration wall_time;
	std::clock_t cpu_time;
};

// test with producer(s) and consumer(s) on different threads
template<typename T>
static concurrency_test_time test_with_concurrency(
		const int producers, const int consumers,
		const T default_value, const int num_items,
		const producer_test_function<T> producer_function,
		const consumer_test_function<T> consumer_function){
	// Here's the queue we'll be testing.
	auto q = std::make_shared<mpmc_queue<T>>();

	// This is the wall clock start time.
	std::chrono::time_point<std::chrono::steady_clock> wall_start;
	// And here's the process CPU start time.
	std::clock_t cpu_start;

	// This is to _try_ to reduce timing overhead from startup.
	// +1 for us so we can time it.
	// It also has the bonus of trying to aggravate data races.
	std::latch setup(producers+consumers+1);
	std::latch start(producers+consumers+1);
	std::latch stop(producers+consumers+1);

	// Here's where we keep the futures for the producer and consumer tasks.
	std::vector<std::future<void>> producer_futs;
	std::vector<std::future<void>> consumer_futs;

	// Here's a naive estimate of how many items per worker to run.
	const int items_per_producer = num_items / producers;
	const int items_per_consumer = num_items / consumers;

	// And here's where we keep track of the number of items left.
	int producer_items_left = num_items;
	int consumer_items_left = num_items;

	// We distribute extra onto the last worker to avoid any questions about
	// rounding in the division op.
	for(int i = 0; i < producers-1; i++){
		producer_futs.push_back(
			std::async(std::launch::async,
				producer_function, q, items_per_producer, default_value,
				std::ref(setup), std::ref(start), std::ref(stop)));

		producer_items_left -= items_per_producer;
	}
	for(int i = 0; i < consumers-1; i++){
		consumer_futs.push_back(
			std::async(std::launch::async,
				consumer_function, q, items_per_consumer,
				std::ref(setup), std::ref(start), std::ref(stop)));

		consumer_items_left -= items_per_consumer;
	}

	// Now put the remaining work on the last workers.
	producer_futs.push_back(
		std::async(std::launch::async,
			producer_function, q, producer_items_left, default_value,
			std::ref(setup), std::ref(start), std::ref(stop)));
	producer_items_left = 0;
	consumer_futs.push_back(
		std::async(std::launch::async,
			consumer_function, q, consumer_items_left,
			std::ref(setup), std::ref(start), std::ref(stop)));
	consumer_items_left = 0;

	// Make sure that everyone is set up and ready to start timing.
	setup.arrive_and_wait();

	// Now that everything's set up, start the timers and the test.
	wall_start = std::chrono::steady_clock::now();
	cpu_start = std::clock();
	start.arrive_and_wait();

	// Stop the test, stop the timers and return the results.
	stop.arrive_and_wait();
	const auto wall_stop = std::chrono::steady_clock::now();
	auto cpu_stop = std::clock();

	// We could loop over the vectors and wait, but why do that when
	// the destructors do the job for us?

	return concurrency_test_time{
		wall_stop - wall_start,
		cpu_stop - cpu_start,
	};
}

	}
}
#endif /* STORM_MPMC_TEST_HELPERS_H */