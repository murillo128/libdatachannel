/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "threadpool.hpp"

namespace rtc {

ThreadPool &ThreadPool::Instance() {
	// Init handles joining on cleanup
	static ThreadPool *instance = new ThreadPool;
	return *instance;
}

ThreadPool::~ThreadPool() { join(); }

int ThreadPool::count() const {
	std::unique_lock lock(mWorkersMutex);
	return int(mWorkers.size());
}

void ThreadPool::spawn(int count) {
	std::unique_lock lock(mWorkersMutex);
	mJoining = false;
	while (count-- > 0)
		mWorkers.emplace_back(std::bind(&ThreadPool::run, this));
}

void ThreadPool::join() {
	std::unique_lock lock(mWorkersMutex);
	mJoining = true;
	mCondition.notify_all();

	for (auto &w : mWorkers)
		w.join();

	mWorkers.clear();
}

void ThreadPool::run() {
	while (runOne()) {
	}
}

bool ThreadPool::runOne() {
	if (auto task = dequeue()) {
		task();
		return true;
	}
	return false;
}

std::function<void()> ThreadPool::dequeue() {
	std::unique_lock lock(mMutex);
	while(!mJoining) {
		if(!mTasks.empty()) {
			clock::time_point time = mTasks.begin()->first;
			if(time <= clock::now())
				break;

			mCondition.wait_until(lock, time);

		} else {
			mCondition.wait(lock);
		}
	}
	if (mTasks.empty())
		return nullptr;

	auto task = std::move(mTasks.begin()->second);
	mTasks.erase(mTasks.begin());
	return task;
}

} // namespace rtc
