
#include "timercpp.h"

ActionTimer::ActionTimer() {};
ActionTimer::~ActionTimer() {};

void ActionTimer::stopInterval() {
	this->clear = true;
}