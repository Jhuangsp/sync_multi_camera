#ifndef ACTION_TIMER
#define ACTION_TIMER
#include <iostream>
#include <thread>
#include <chrono>

class ActionTimer {

public:
	ActionTimer();
	~ActionTimer();

	template<typename T>
	void setTimeout(T function, int delay);
	template<typename T>
	void setInterval(T function, int interval);
	void stopInterval();
private:
	bool clear = false;

};

template<typename T>
void ActionTimer::setTimeout(T function, int delay) {
	this->clear = false;
	std::thread tr_out([=]() {
		if (this->clear) return;
		std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		if (this->clear) return;
		function();
	});
	tr_out.detach();
}

template<typename T>
void ActionTimer::setInterval(T function, int interval) {
	this->clear = false;
	std::thread t_in([=]() {
		while (true) {
			if (this->clear) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(interval));
			if (this->clear) return;
			function();
		}
	});
	t_in.detach();
}
#endif