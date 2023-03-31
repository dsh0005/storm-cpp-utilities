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

#include "mpmc_queue.hpp"
using namespace storm;

static void instantiate_some_queues(){
	// just instantiate some queues with different types
	// This tests the templating, constructors, and destructors
	mpmc_queue<int> qi;
	mpmc_queue<float> qf;

	// how about something non-copyable?
	mpmc_queue<std::future<void>> qfut;
}

int main(int /* argc */, char ** /* argv */){
	instantiate_some_queues();
}
