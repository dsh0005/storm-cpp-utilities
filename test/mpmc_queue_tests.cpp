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
#include "mpmc_test_helpers.hpp"
using namespace storm;
using namespace storm::test;

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

int main(int /* argc */, char ** /* argv */){
	using std::chrono::milliseconds;

	static constexpr int num_items = 1'000'000;

	instantiate_some_queues();
	cout << "instantiating queues finished.\n";

	cout << "Running basic tests of push() and size().\n";
	test_push_and_size();

	cout << "Running basic single-producer single-consumer tests.\n";
	test_with_concurrency<mpmc_queue<float>, float>(1, 1, 1.0f, num_items, milliseconds(0), normal_producer<mpmc_queue<float>, float>, normal_consumer<mpmc_queue<float>, float>);

	cout << "Running MPMC tests with small amounts of concurrency.\n";
	cout << "1p2c: " << std::flush;
	test_with_concurrency<mpmc_queue<float>, float>(1, 2, 1.0f, num_items, milliseconds(0), normal_producer<mpmc_queue<float>, float>, normal_consumer<mpmc_queue<float>, float>);
	cout << "done\n";
	cout << "2p1c: " << std::flush;
	test_with_concurrency<mpmc_queue<float>, float>(2, 1, 1.0f, num_items, milliseconds(0), normal_producer<mpmc_queue<float>, float>, normal_consumer<mpmc_queue<float>, float>);
	cout << "done\n";
	cout << "2p2c: " << std::flush;
	test_with_concurrency<mpmc_queue<float>, float>(2, 2, 1.0f, num_items, milliseconds(0), normal_producer<mpmc_queue<float>, float>, normal_consumer<mpmc_queue<float>, float>);
	cout << "done\n";
}
