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
	std::thread([seconds, onComplete]() {
		printf("Starting timer for %d seconds\n", seconds);
		auto start = std::chrono::steady_clock::now();
		auto end = start + std::chrono::seconds(seconds);
		while (std::chrono::steady_clock::now() < end) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		printf("A timer of %d seconds has ended\n", seconds);
		if (onComplete) onComplete();
		}).detach();
}