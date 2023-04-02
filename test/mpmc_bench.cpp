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
#include <chrono>
#include <map>
#include <functional>

#include <cstddef>

#include "mpmc_queue.hpp"
#include "mpmc_test_helpers.hpp"
using namespace storm;
using namespace storm::test;

using std::cout;

using test_results_map = std::map<std::pair<int, int>, concurrency_test_time>;

static void print_results(const test_results_map &map){
	using std::setw;
	using std::right;
	// Might as well do it by value, since it's like 16 bytes.
	// But GCC warns on that, and I don't know an idiom to surpress it.
	for(const auto & [concurrency, times] : map){
		cout << concurrency.first << " Producer ";
		cout << concurrency.second << " Consumer, ";
		// FIXME: ugh, these don't align. That needs to be prettied up.
		cout << "wall: " << right << setw(14) << times.wall_time;
		cout << " cpu: " << right << setw(11) << times.cpu_time << '\n';
	}
}

int main(int /* argc */, char ** /* argv */){
	// Here's where we store our results.
	test_results_map test_results;

	static constexpr int num_items = 1'000'000;

	cout << "Running basic single-producer single-consumer tests.\n";
	test_results.insert_or_assign(
		std::make_pair(1, 1),
		test_with_concurrency(1, 1, 1.0f, num_items,
			normal_producer<float>, normal_consumer<float>));

	cout << "Running MPMC tests with small amounts of concurrency.\n";
	cout << "1p2c: " << std::flush;
	test_results.insert_or_assign(
		std::make_pair(1, 2),
		test_with_concurrency(1, 2, 1.0f, num_items,
			normal_producer<float>, normal_consumer<float>));
	cout << "done\n";
	cout << "2p1c: " << std::flush;
	test_results.insert_or_assign(
		std::make_pair(2, 1),
		test_with_concurrency(2, 1, 1.0f, num_items,
			normal_producer<float>, normal_consumer<float>));
	cout << "done\n";
	cout << "2p2c: " << std::flush;
	test_results.insert_or_assign(
		std::make_pair(2, 2),
		test_with_concurrency(2, 2, 1.0f, num_items,
			normal_producer<float>, normal_consumer<float>));
	cout << "done\n";

	print_results(test_results);

	test_results_map stub_results;
	cout << "Now testing with stub producers and consumers.\n";
	stub_results.insert_or_assign(
		std::make_pair(1, 1),
		test_with_concurrency(1, 1, 1.0f, num_items,
			stub_producer<float>, stub_consumer<float>));
	stub_results.insert_or_assign(
		std::make_pair(1, 2),
		test_with_concurrency(1, 2, 1.0f, num_items,
			stub_producer<float>, stub_consumer<float>));
	stub_results.insert_or_assign(
		std::make_pair(2, 1),
		test_with_concurrency(2, 1, 1.0f, num_items,
			stub_producer<float>, stub_consumer<float>));
	stub_results.insert_or_assign(
		std::make_pair(2, 2),
		test_with_concurrency(2, 2, 1.0f, num_items,
			stub_producer<float>, stub_consumer<float>));

	print_results(stub_results);
}
