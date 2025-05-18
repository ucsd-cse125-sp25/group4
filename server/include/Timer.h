#include <functional>
#include <thread>
#include <mutex>

class Timer {
	public:
		Timer(void);
		~Timer(void);

		void startTimer(int, std::function<void()>);
		long long getRemainingMs();

private:
	std::mutex mu;
	std::chrono::steady_clock::time_point end;
};
