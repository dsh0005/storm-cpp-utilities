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

#include <cstddef>

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

// test with a single producer and consumer on different threads
static void test_with_spsc(){
	// Here's the queue we'll be testing.
	auto q = std::make_shared<mpmc_queue<float>>();
	// This is to _try_ to aggravate data races by reducing the amount of
	// tasks after startup/sync but before potentially racing.
	// 1 producer + 1 consumer = 3
	std::latch start(2);

	static constexpr int num_items = 1'000'000;

	auto prod_fut = std::async(std::launch::async, normal_producer, q, num_items, std::ref(start));
	auto cons_fut = std::async(std::launch::async, normal_consumer, q, num_items, std::ref(start));

	prod_fut.wait();
	cons_fut.wait();
}

int main(int /* argc */, char ** /* argv */){
	instantiate_some_queues();
	cout << "instantiating queues finished.\n";

	cout << "Running basic tests of push() and size().\n";
	test_push_and_size();

	cout << "Running basic single-producer single-consumer tests.\n";
	test_with_spsc();
}
