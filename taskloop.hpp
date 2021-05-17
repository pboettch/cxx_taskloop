// Header-only, modern C++ taskloo-class
//
// Copyright (C) 2017-2021 Patrick Boettcher <p@yai.se>
//
// SPDX-License-Identifier: LGPL-3.0
//
// Version: 1.0.0
//
// Project website: https://github.com/pboettch/cxx_taskloop

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <thread>

// A class taking a function as argument. When started it calls this
// function regurarly using the intervals returned by this function
class TaskLoop
{
	std::thread thread_;

	std::mutex cv_mutex_;
	std::condition_variable cv_;

	std::atomic<bool> stop_;                              //< if stop_ is true, the run-method will end
	std::function<std::chrono::milliseconds(void)> task_; //< the task (function) to be executed

	bool steady_ = false; //< if true the interval between two calls will be constant, independently how long the call to the task-function will take

	// this method is run in a thread and it calls the task-function in a loop
	void run()
	{
		using namespace std::chrono;

		// absolut starting time
		auto next = steady_clock::now();
		for (;;) {
			nanoseconds delay = task_(); // call of the task-function

			std::unique_lock<std::mutex> lk(cv_mutex_);

			if (stop_) // if a stop was requested
				break;

			// if the delay returned by the task is 0, the task wants us to end the loop
			if (delay == nanoseconds(0))
				break;

			// If steady_ has been requested - a fixed interval is used, ignoring the time it took for the task to run.
			// in other words, we take into account the execution-time of the task to determine the next call-time
			auto now = steady_clock::now();

			if (steady_) {
				// next time_point for its execution
				next += delay;

				// if this execution was longer than the returned interval
				// the task is called immediatly again and the cadence is 'now'
				// and we wait the full-delay.
				if (next < now)
					next = now + delay;
			} else // non-steady -> minimum delay between two calls
				next = now + delay;

			// waiting for the timeout or if someone wakes us up (dtor mainly)
			cv_.wait_until(lk, next);

			if (stop_)
				break;
		}
	}

public:
	// ctor: the given task to be run in a loop, and whether the intervals have to be considered steady
	// The loop is run immediatly
	TaskLoop(std::function<std::chrono::milliseconds(void)> task, bool steady_timing = false)
	    : task_(task), steady_(steady_timing)
	{
		{
			std::lock_guard<std::mutex> lk(cv_mutex_);
			stop_ = false;
		}
		thread_ = std::thread(&TaskLoop::run, this);
	}

	// The only way to destroy a task is to destroy the object
	~TaskLoop()
	{
		{
			std::lock_guard<std::mutex> lk(cv_mutex_);
			stop_ = true;
		}

		cv_.notify_all();

		thread_.join();
	}

	// Wakeup the task and reset the loop from the outside
	void reset()
	{
		cv_.notify_all();
	}
};
