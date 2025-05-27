#include "Timer.h"

Timer::Timer()
{

}

Timer::~Timer()
{

}

void Timer::cancelTimer() {
	// cancels the timer by setting end to an uninitialized time_point
	mu.lock();
	end = std::chrono::steady_clock::time_point{};
	mu.unlock();
	printf("Timer cancelled\n");
}

void Timer::startTimer(int seconds, std::function<void()> onComplete) {
	// spawns a new thread to keep track of time
	// needs synch protection
	std::thread([this, seconds, onComplete]() {
		printf("Starting timer for %d seconds\n", seconds);
		auto end_time = std::chrono::steady_clock::now()
					  + std::chrono::seconds(seconds);
		mu.lock();
		end = end_time;
		mu.unlock();

		while (std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		printf("A timer of %d seconds has ended\n", seconds);
		if (onComplete) onComplete();
		}).detach();
}

long long Timer::getRemainingMs() {
	mu.lock();
	if (end == std::chrono::steady_clock::time_point{}) {
		// uninitialized timer
		return 0;
	}

	auto now = std::chrono::steady_clock::now();
	if (now >= end) {
		// finished timer
		return 0;
	}

	return std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
	mu.unlock();
}