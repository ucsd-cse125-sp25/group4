#include <functional>
#include <thread>

class Timer {
	public:
		Timer(void);
		~Timer(void);

		void startTimer(int, std::function<void()>);

};
