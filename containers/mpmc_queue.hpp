// SPDX-License-Identifier: AGPL-3.0-only

/* mpmc_queue: Multi-producer multi-consumer queue that blocks consumers.
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

#ifndef STORM_MPMC_QUEUE_H
#define STORM_MPMC_QUEUE_H 1

#include <utility>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace storm {

	/* mpmc_queue: a multi-producer multi-consumer queue that blocks consumers
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
	 */
	template<
		typename T,
		typename Container = std::queue<T>::container_type>
	class mpmc_queue {
	public:
		// Constructor and destructor are default, and not interesting.
		mpmc_queue() = default;
		~mpmc_queue() = default;

		/* Since we have a mutex and other stuff, we're neither copyable nor
		 * movable, so delete these.
		 *
		 * TODO: maybe add a level of indirection to those so we can at least
		 * be movable?
		 */
		mpmc_queue(const mpmc_queue&) = delete;
		mpmc_queue(mpmc_queue&&) = delete;
		mpmc_queue& operator=(const mpmc_queue&) = delete;
		mpmc_queue& operator=(mpmc_queue&&) = delete;

		// push: put an element into the queue
		void push(const T &t){
			{
				std::lock_guard<std::shared_mutex> lk(mtx);
				q.push(t);
				// Release the lock before notifying, since that's more efficient
				// on most systems.
			}
			cv.notify_one();
		}
		// And the "move into" version of above.
		void push(T &&T){
			{
				std::lock_guard<std::shared_mutex> lk(mtx);
				q.push(std::move(t));
				// Again, release the lock then notify.
			}
			cv.notify_one();
		}

		// emplace: construct an element in-place in the queue.
		template<typename... Args>
		decltype(auto) emplace(Args&&... args){
			std::lock_guard<std::shared_mutex> lk(mtx);

			auto r = q.emplace(std::forward<Args>(args)...);
			cv.notify_one();

			return r;
		}

		// try_pop: try to pop an element if there is one. Does not block.
		std::optional<T> try_pop(){
			std::lock_guard<std::shared_mutex> lk(mtx);

			if(q.empty())
				return std::optional<T>();

			std::optional<T> t(std::move(q.front()));
			q.pop();

			return t;
		}

		// pop_wait: wait until there is an element, then pop.
		T pop_wait(){
			std::unique_lock<std::shared_mutex> lk(mtx);

			cv.wait(lk, [this](){ return !q.empty(); });

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
			std::unique_lock<std::shared_mutex> lk(mtx);

			if(!cv.wait_for(lk, rel_time, [this](){ return !q.empty(); }))
				return std::optional<T>();

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
			std::unique_lock<std::shared_mutex> lk(mtx);

			if(!cv.wait_until(lk, timeout_time, [this](){ return !q.empty(); }))
				return std::optional<T>();

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
		[[nodiscard]] std::queue<T, Container>::size_type size() const {
			std::shared_lock<std::shared_mutex> lk(mtx);
			return q.size();
		}

		/* swap: swap the queues, atomically, while being careful of waiters.
		 *
		 * So this is an interesting one, and I'm not sure it'd ever really be
		 * useful, but it's here to fill out our members to try and match
		 * std::queue.
		 *
		 * It's as noexcept as the underlying queue, which is as noexcept as the
		 * underlying container (std::deque by default), which depends on the
		 * allocator if you gave it a non-default one.
		 *
		 * Keep in mind that since we still need to lock, if this is noexcept
		 * and the locking throws, we std::terminate(). AFAIK that only happens
		 * in situations that are already UB, or in some versions of Windows
		 * if it's lazily allocating and fails.
		 */
		void swap(mpmc_queue &other) noexcept(noexcept(q.swap(other.q))) {
			// Grab the lock in an extra scope so we release it before
			// notifying waiters.
			{
				std::scoped_lock lk(mtx, other.mtx);

				// If we ever parameterize out the queue type, remember that the STL
				// idiom is "using std::swap; swap(a, b);" to allow overloads.
				q.swap(other.q);
			}

			/* TODO: optimize for the case where one or both queues doesn't need
			 * to wake any waiters because it's empty. Also the case where only
			 * one waiter can wake because q.size() == 1. Except I don't know if
			 * that's safe to do while it's not locked.
			 *
			 * Until then, just wake everybody to be safe.
			 *
			 * Also if the waiters ever differ we'll need to notify_all() anyways.
			 */
			cv.notify_all();
			other.cv.notify_all();
		}

	private:

		// The mutex that protects all of this.
		// It's mutable because we have const member functions.
		mutable std::shared_mutex mtx;
		// The condition variable we wait on.
		// It's not mutable because size() and empty() don't need to wait.
		std::condition_variable_any cv;

		// The queue we're using to store stuff.
		std::queue<T, Container> q;
	};

	// swap as an overload of std::swap.
	template<typename T, typename C>
	void swap(mpmc_queue<T, C> &lhs, mpmc_queue<T, C> &rhs)
			noexcept(noexcept(lhs.swap(rhs))) {
		lhs.swap(rhs);
	}

}

#endif // STORM_MPMC_QUEUE_H
