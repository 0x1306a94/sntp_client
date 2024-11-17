//
//  system_clock.cpp
//  sntp_client
//
//  Created by king on 2024/11/17.
//

#include "../system_clock.hpp"

#include <sys/time.h>
#include <windows.h>

namespace time_sync {

class WindowsSystemClock : public SystemClock {
  private:
  public:
	WindowsSystemClock() {
	}

	~WindowsSystemClock() = default;

	// 获取当前时间（毫秒）
	uint64_t currentTimeMillis() override {
		struct timeval tv;
		if (gettimeofday(&tv, nullptr) != -1) {
			return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
		}
		return 0;
	}

	// 获取自启动以来的时间（毫秒）
	uint64_t elapsedRealtime() override {
		return (uint64_t)GetTickCount();
	}
};

std::unique_ptr<SystemClock> createSystemClock() {
	return std::make_unique<WindowsSystemClock>();
}
};  // namespace time_sync
