#include "Timer.h"

Timer::Timer()
{

}

Timer::~Timer()
{

}

void Timer::startTimer(int seconds, std::function<void()> onComplete) {
	// spawns a new thread to keep track of time
	// needs synch protection
	std::thread([this, seconds, onComplete]() {
		printf("[TIMER] starting timer for %d seconds\n", seconds);
		mu.lock();
		start = std::chrono::steady_clock::now();
		end = start + std::chrono::seconds(seconds);
		mu.unlock();

		while (std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		printf("[TIMER] timer of %d seconds has ended\n", seconds);
		if (onComplete) onComplete();
		}).detach();
}

float Timer::getFracElapsed() {
	mu.lock();
	if (end == std::chrono::steady_clock::time_point{}) {
		// uninitialized timer
		mu.unlock();
		return 0;
	}

	auto now = std::chrono::steady_clock::now();
	// before start
	if (now <= start) {
		mu.unlock();
		return 0.0f;
	}
	// after end
	if (now >= end) {
		mu.unlock();
		return 1.0f;
	}

	float total = std::chrono::duration<float>(end - start).count();
	float elapsed = std::chrono::duration<float>(now - start).count();
	mu.unlock();
	return elapsed / total;
}