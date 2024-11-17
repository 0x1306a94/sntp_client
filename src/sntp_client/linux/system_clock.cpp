//
//  system_clock.cpp
//  sntp_client
//
//  Created by king on 2024/11/17.
//

#include "../system_clock.hpp"

#include <sys/time.h>
#include <time.h>

namespace time_sync {

class LinuxSystemClock : public SystemClock {
  private:
  public:
	LinuxSystemClock() {
	}

	~LinuxSystemClock() = default;

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
		struct timespec times = {0, 0};
		clock_gettime(CLOCK_MONOTONIC, &times);
		uint64_t ts = times.tv_sec * 1000 + times.tv_nsec / 1000000;
		return ts;
	}
};

std::unique_ptr<SystemClock> createSystemClock() {
	return std::make_unique<LinuxSystemClock>();
}
};  // namespace time_sync
