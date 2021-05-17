#include "../taskloop.hpp"

#include <iostream>
#include <vector>

using namespace std::chrono;

#define TEST(section, name) \
	static void section##_##name()

static int errors = 0;

#define EXPECT_EQ(a, b)                                                                                                   \
	if ((a) != (b)) {                                                                                                       \
		std::cerr << __PRETTY_FUNCTION__ << " " << __FILE__ << ":" << __LINE__ << " EQ failed: " << a << " != " << b << "\n"; \
		errors++;                                                                                                             \
	}

class TaskWithDelay
{
	milliseconds delay;

public:
	TaskWithDelay(const milliseconds &d, int &v)
	    : delay(d), value(v) {}

	int &value;

	milliseconds operator()()
	{
		value++;
		return delay;
	}
};

TEST(Task, one_loop)
{
	int v_sec = 0,
	    v_msec = 0;

	{
		TaskLoop tasksec(TaskWithDelay(seconds(1), v_sec));
		TaskLoop taskmsec(TaskWithDelay(milliseconds(500), v_msec));

		// waiting for 3 seconds is too long, sometimes the "seconds"-task is doing 4 iterations
		std::this_thread::sleep_for(milliseconds(2750));
	}

	EXPECT_EQ(v_sec, 3);
	EXPECT_EQ(v_msec, 6);
}

TEST(Task, reset)
{
	int v_sec = 0;

	{
		TaskLoop task(TaskWithDelay(seconds(2), v_sec));

		std::this_thread::sleep_for(milliseconds(200));
		task.reset();
		std::this_thread::sleep_for(milliseconds(2500));
		task.reset();
		std::this_thread::sleep_for(milliseconds(200));
		task.reset();
		std::this_thread::sleep_for(milliseconds(200));
	}

	EXPECT_EQ(v_sec, 5);
}

class TaskSteady
{
	std::vector<steady_clock::time_point> time_points_;

	std::shared_ptr<TaskLoop> task_;

public:
	void start(bool steady,
	           milliseconds wait,
	           milliseconds delay)
	{
		task_ = std::make_shared<TaskLoop>(
		    [this, delay, wait] {
			    auto now = steady_clock::now();
			    time_points_.push_back(now);

			    //static auto last = now;
			    //std::cerr << now.time_since_epoch().count() << " "
			    //          << last.time_since_epoch().count() << " "
			    //          << (now - last).count() << " "
			    //          << "\n";
			    //last = now;

			    std::this_thread::sleep_for(wait);
			    return delay;
		    },
		    steady);
	}

	void stop()
	{
		task_ = nullptr;
	}

	bool check(milliseconds expected)
	{
		steady_clock::duration total(0);

		for (auto i = time_points_.begin(), e = time_points_.end();; ++i) {
			if (i + 1 == e) // last
				break;

			auto dur = *(i + 1) - *i;
			total = total + dur;
		}

		total /= time_points_.size() - 1;

		std::cerr << "avg " << total.count() << " " << expected.count() << "\n";

		return expected == duration_cast<milliseconds>(total);
	}
};

TEST(Task, steady_false_50_50)
{
	TaskSteady t;

	t.start(false, milliseconds(50), milliseconds(50));
	std::this_thread::sleep_for(seconds(2));
	t.stop();

	EXPECT_EQ(t.check(milliseconds(100)), true);
}

// 50 msec run-time, 100 ms interval -> 100ms interval
TEST(Task, steady_true_50_100)
{
	TaskSteady t;

	t.start(true, milliseconds(50), milliseconds(100));
	std::this_thread::sleep_for(seconds(2));
	t.stop();

	EXPECT_EQ(t.check(milliseconds(100)), true);
}

// 100 msec run-time, 10 ms interval -> 110ms interval
TEST(Task, steady_true_100_10)
{
	TaskSteady t;

	t.start(true, milliseconds(100), milliseconds(10));
	std::this_thread::sleep_for(seconds(2));
	t.stop();

	EXPECT_EQ(t.check(milliseconds(110)), true);
}

TEST(Task, steady_true_22_150)
{
	TaskSteady t;
	t.start(true, milliseconds(150), milliseconds(22));
	std::this_thread::sleep_for(seconds(2));
	t.stop();
	EXPECT_EQ(t.check(milliseconds(150 + 22)), true);
}

int main(void)
{
	Task_steady_true_22_150();
	Task_steady_true_100_10();
	Task_steady_true_50_100();
	Task_steady_false_50_50();
	Task_reset();
	Task_one_loop();

	return errors;
}

