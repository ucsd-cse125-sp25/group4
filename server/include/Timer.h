#include <functional>
#include <thread>
#include <mutex>

class Timer {
	public:
		Timer(void);
		~Timer(void);

		void startTimer(int, std::function<void()>);
		float getFracElapsed();

private:
	std::mutex mu;
	std::chrono::steady_clock::time_point start, end;
};
