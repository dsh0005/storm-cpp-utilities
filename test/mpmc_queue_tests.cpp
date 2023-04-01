// SPDX-License-Identifier: AGPL-3.0-only

/* mpmc_queue_tests: Tests for the mpmc_queue.
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
#include <iostream>
#include <thread>
#include <future>
#include <latch>
#include <vector>

#include <cstddef>
#include <cassert>

#include "mpmc_queue.hpp"
using namespace storm;

using std::cout;

static void instantiate_some_queues(){
	cout << "instantiating some mpmc_queues\n";
	// just instantiate some queues with different types
	// This tests the templating, constructors, and destructors
	mpmc_queue<int> qi;
	mpmc_queue<float> qf;

	// how about something non-copyable?
	mpmc_queue<std::future<void>> qfut;

	cout << "destroying some mpmc_queues\n";
}

static void test_push_and_size(){
	mpmc_queue<int> q;

	for(int i = 0; i < 10; i++){
		q.push(i);
		q.emplace(i);
	}

	const std::size_t sz = q.size();
	if(sz != 20){
		cout << "queue size wrong! " << sz << '\n';
	}else{
		cout << "queue size looks good\n";
	}

	std::size_t sz_count = 0;
	while(!q.empty()){
		const auto i = q.try_pop();
		if(!i.has_value()){
			cout << "queue ran out early!\n";
		}
		sz_count++;
	}

	cout << "Got " << sz_count << " elements back out from queue.\n";

	if(!q.empty()){
		cout << "Queue is not empty when it should be!\n";
	}
}

// put n items into q
static void normal_producer(
		const std::shared_ptr<mpmc_queue<float>> q,
		const int n,
		std::latch &start_signal
		){
	start_signal.arrive_and_wait();

	for(int i = 0; i < n; i++){
		q->push(i);
	}
}

// pop n items from q
static void normal_consumer(
		const std::shared_ptr<mpmc_queue<float>> q,
		const int n,
		std::latch &start_signal
		){
	start_signal.arrive_and_wait();

	for(int i = 0; i < n; i++){
		q->pop_wait();
	}
}

// test with producer(s) and consumer(s) on different threads
static void test_with_concurrency(const int producers, const int consumers){
	// Here's the queue we'll be testing.
	auto q = std::make_shared<mpmc_queue<float>>();
	// This is to _try_ to aggravate data races by reducing the amount of
	// tasks after startup/sync but before potentially racing.
	std::latch start(producers+consumers);

	static constexpr int num_items = 10'000'000;

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
				normal_producer, q, items_per_producer, std::ref(start)));

		producer_items_left -= items_per_producer;

		assert(producer_items_left > 0);
	}
	for(int i = 0; i < consumers-1; i++){
		consumer_futs.push_back(
			std::async(std::launch::async,
				normal_consumer, q, items_per_consumer, std::ref(start)));

		consumer_items_left -= items_per_consumer;

		assert(consumer_items_left > 0);
	}

	// Now put the remaining work on the last workers.
	producer_futs.push_back(
		std::async(std::launch::async,
			normal_producer, q, producer_items_left, std::ref(start)));
	producer_items_left = 0;
	consumer_futs.push_back(
		std::async(std::launch::async,
			normal_consumer, q, consumer_items_left, std::ref(start)));
	consumer_items_left = 0;

	// We could loop over the vectors and wait, but why do that when
	// the destructors do the job for us?
}

int main(int /* argc */, char ** /* argv */){
	instantiate_some_queues();
	cout << "instantiating queues finished.\n";

	cout << "Running basic tests of push() and size().\n";
	test_push_and_size();

	cout << "Running basic single-producer single-consumer tests.\n";
	test_with_concurrency(1, 1);

	cout << "Running MPMC tests with small amounts of concurrency.\n";
	cout << "1p2c: " << std::flush;
	test_with_concurrency(1, 2);
	cout << "done\n";
	cout << "2p1c: " << std::flush;
	test_with_concurrency(2, 1);
	cout << "done\n";
	cout << "2p2c: " << std::flush;
	test_with_concurrency(2, 2);
	cout << "done\n";
}
