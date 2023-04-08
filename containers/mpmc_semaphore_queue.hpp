// SPDX-License-Identifier: AGPL-3.0-only

/* mpmc_semaphore_queue: Multi-producer multi-consumer queue that blocks consumers.
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

#ifndef STORM_MPMC_SEMAPHORE_QUEUE_H
#define STORM_MPMC_SEMAPHORE_QUEUE_H 1

#include <utility>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <semaphore>
#include <optional>
#include <chrono>

namespace storm {

	/* mpmc_semaphore_queue: a multi-producer multi-consumer queue that
	 *             uses semaphores for counting and blocks consumers
	 *             when empty.
	 *
	 * T        : the element type, must be movable.
	 * Container: the underlying container type used in a std::queue
	 *
	 * This is almost the same interface as std::queue, but one thing to note
	 * is pop() returns by value, so you pop() instead of copying back() then
	 * pop(). This is because back()+pop() would be inherently racy.
	 *
	 * Another difference is that there is no pop() by that name, since it
	 * would be misleading. Instead, there are try_pop(), pop_wait(),
	 * pop_wait_for(), and pop_wait_until().
	 *
	 * Consumers block, while producers never block, barring resource
	 * exhaustion. The one small asterisk on that is that they do acquire and
	 * release a std::shared_mutex, but nobody holds it for longer than it
	 * takes to do stuff like emplace(args...) or pop().
	 *
	 * XXX: This queue might break if you put more than 2^31-1 elements in at
	 * once. This is from a limitation of libstdc++. Other C++ STLs might have
	 * other limitations. TODO: Test with other STLs.
	 */
	template<
		typename T,
		typename Container = typename std::queue<T>::container_type>
	class mpmc_semaphore_queue {
	public:
		// Constructor and destructor are (almost) default, and not
		// very interesting.
		mpmc_semaphore_queue() : available(0) {}
		~mpmc_semaphore_queue() = default;

		/* Since we have a mutex and other stuff, we're neither copyable nor
		 * movable, so delete these.
		 *
		 * TODO: maybe add a level of indirection to those so we can at least
		 * be movable?
		 */
		mpmc_semaphore_queue(const mpmc_semaphore_queue&) = delete;
		mpmc_semaphore_queue(mpmc_semaphore_queue&&) = delete;
		mpmc_semaphore_queue& operator=(const mpmc_semaphore_queue&) = delete;
		mpmc_semaphore_queue& operator=(mpmc_semaphore_queue&&) = delete;

		// push: put an element into the queue
		void push(const T &t){
			{
				std::lock_guard<std::shared_mutex> lk(mtx);
				q.push(t);
				// Release the lock before notifying, since that's more efficient
				// on most systems.
			}
			available.release();
		}
		// And the "move into" version of above.
		void push(T &&t){
			{
				std::lock_guard<std::shared_mutex> lk(mtx);
				q.push(std::move(t));
				// Again, release the lock then notify.
			}
			available.release();
		}

		// emplace: construct an element in-place in the queue.
		template<typename... Args>
		void emplace(Args&&... args){
			{
				std::lock_guard<std::shared_mutex> lk(mtx);
				q.emplace(std::forward<Args>(args)...);
				// Again, release the lock then notify.
			}
			available.release();
		}

		// try_pop: try to pop an element if there is one. Does not block.
		std::optional<T> try_pop(){
			// Try to grab from the semaphore.
			if(available.try_acquire())
				return std::optional<T>();

			// We got permission to take one, but we need the lock.
			std::lock_guard<std::shared_mutex> lk(mtx);

			std::optional<T> t(std::move(q.front()));
			q.pop();

			return t;
		}

		// pop_wait: wait until there is an element, then pop.
		T pop_wait(){
			// Grab one count from the semaphore.
			available.acquire();

			// Now we can take one, but we need the lock.
			std::lock_guard<std::shared_mutex> lk(mtx);

			T t(std::move(q.front()));
			q.pop();

			return t;
		}

		/* pop_wait_for: wait for up to the given time for there to be an
		 *               element, then pop, or fail on timeout.
		 *
		 * rel_time: how long to wait for before timing out.
		 *
		 * NOTE: Timeouts are subject to the usual caveats regarding scheduler
		 * delays &c.
		 */
		template<typename Rep, typename Period>
		std::optional<T> pop_wait_for(const std::chrono::duration<Rep, Period> &rel_time){
			// Try to get permission to take an element.
			if(!available.try_acquire_for(rel_time))
				return std::optional<T>();

			// Now we can take one, but we need the lock.
			std::lock_guard<std::shared_mutex> lk(mtx);

			std::optional<T> t(std::move(q.front()));
			q.pop();

			return t;
		}

		/* pop_wait_until: wait for up to the given time for there to be an
		 *                 element, then pop, or fail on timeout.
		 *
		 * timeout_time: what time to wait until before timing out.
		 *
		 * NOTE: Timeouts are subject to the usual caveats regarding scheduler
		 * delays &c.
		 */
		template<typename Clock, typename Duration>
		std::optional<T> pop_wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time){
			// Try to get permission to take an element.
			if(!available.try_acquire_until(timeout_time))
				return std::optional<T>();

			// Now we can take one, but we need the lock.
			std::lock_guard<std::shared_mutex> lk(mtx);

			std::optional<T> t(std::move(q.front()));
			q.pop();

			return t;
		}

		/* empty: return true if the container is empty, false if it's not.
		 *
		 * This is here because std::queue has it. Don't use it, because it's
		 * inherently racy.
		 */
		[[nodiscard]] bool empty() const {
			std::shared_lock<std::shared_mutex> lk(mtx);
			return q.empty();
		}

		/* size: return the number of items in the container.
		 *
		 * This is here because std::queue has it. Don't use it, because it's
		 * inherently racy.
		 */
		[[nodiscard]] typename std::queue<T, Container>::size_type size() const {
			std::shared_lock<std::shared_mutex> lk(mtx);
			return q.size();
		}

	private:

		// Get the max size we can be.
		static constexpr typename Container::difference_type max_size =
			std::numeric_limits<typename Container::difference_type>::max();

		// How many elements are available.
		//std::counting_semaphore<max_size> available;
		// XXX: libstdc++ only guarantees up to int
		std::counting_semaphore<std::numeric_limits<int>::max()> available;

		// The mutex that protects access to the queue.
		// It's mutable because we have const member functions.
		mutable std::shared_mutex mtx;

		// The queue we're using to store stuff.
		std::queue<T, Container> q;
	};

}

#endif // STORM_MPMC_SEMAPHORE_QUEUE_H
