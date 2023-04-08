// SPDX-License-Identifier: AGPL-3.0-only

/* mpmc_bench: Simple microbenchmarks for the mpmc_queue.
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
#include <iomanip>
#include <thread>
#include <future>
#include <latch>
#include <vector>
#include <array>
#include <chrono>
#include <map>
#include <compare>
#include <functional>

#include <cstddef>

#include "mpmc_queue.hpp"
#include "mpmc_test_helpers.hpp"
using namespace storm;
using namespace storm::test;

using std::cout;

// How many producers and consumers are in a test.
struct test_size {
	int producers;
	int consumers;
};

// and we want these to be orderable like tuples.
constexpr std::strong_ordering operator<=>(
		const test_size &lhs,
		const test_size &rhs){
	return std::make_tuple(lhs.producers, lhs.consumers) <=>
	       std::make_tuple(rhs.producers, rhs.consumers);
}

using test_results_map = std::map<test_size, concurrency_test_time>;

static void print_results(const test_results_map &map){
	using std::setw;
	using std::right;
	// Might as well do it by value, since it's like 16 bytes.
	// But GCC warns on that, and I don't know an idiom to surpress it.
	for(const auto & [concurrency, times] : map){
		cout << right << setw(3) << concurrency.producers << " Producer ";
		cout << right << setw(2) << concurrency.consumers << " Consumer, ";
		cout << "wall: " << right << setw(14) << times.wall_time;
		cout << " cpu: " << right << setw(11) << times.cpu_time << '\n';
	}
}

int main(int /* argc */, char ** /* argv */){
	using std::chrono::milliseconds;

	// Here's where we store our results.
	test_results_map test_results;

	static constexpr int num_items = 1'000'000;

	static constexpr std::array test_sizes(std::to_array<test_size>({
		{1, 1},
		{1, 2},
		{2, 1},
		{2, 2},
	}));

	cout << "Running basic normal benchmarks.\n";
	for(const auto t : test_sizes){
		cout << t.producers << 'p' << t.consumers << "c: " << std::flush;
		test_results.insert_or_assign(
			t,
			test_with_concurrency(
				t.producers, t.consumers, 1.0f, num_items, milliseconds(0),
				normal_producer<float>, normal_consumer<float>));
		cout << "done\n";
	}

	print_results(test_results);

	test_results_map slow_results;

	static constexpr int slow_items = 10'000;
	static constexpr std::array slow_test_sizes(std::to_array<test_size>({
		{10, 1},
		{100, 1},
		{100, 5},
	}));

	cout << "Running slow-producer benchmarks.\n";
	for(const auto t : slow_test_sizes){
		cout << t.producers << 'p' << t.consumers << "c: " << std::flush;
		slow_results.insert_or_assign(
			t,
			test_with_concurrency(
				t.producers, t.consumers, 1.0f, slow_items, milliseconds(10),
				slow_producer<float>, normal_consumer<float>));
		cout << "done\n";
	}

	print_results(slow_results);

	test_results_map stub_results;

	cout << "Now testing with stub producers and consumers.\n";
	for(const auto t : test_sizes){
		cout << t.producers << 'p' << t.consumers << "c: " << std::flush;
		stub_results.insert_or_assign(
			t,
			test_with_concurrency(
				t.producers, t.consumers, 1.0f, num_items, milliseconds(0),
				stub_producer<float>, stub_consumer<float>));
		cout << "done\n";
	}

	print_results(stub_results);
}
