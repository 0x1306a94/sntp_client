//
//  system_clock.cpp
//  sntp_client
//
//  Created by king on 2024/11/17.
//

#include "../system_clock.hpp"

// #include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <time.h>

namespace time_sync {

class AppleSystemClock : public SystemClock {
  private:
	//    mach_timebase_info_data_t timebase_info_;

  public:
	AppleSystemClock() {
		//        mach_timebase_info(&timebase_info_);
	}

	~AppleSystemClock() = default;

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
		struct timeval boottime;
		size_t size = sizeof(boottime);
		int mib[2]  = {CTL_KERN, KERN_BOOTTIME};

		if (sysctl(mib, 2, &boottime, &size, nullptr, 0) != -1) {
			struct timeval now;
			if (gettimeofday(&now, nullptr) != -1) {
				// 计算当前时间和启动时间的差值
				uint64_t seconds     = now.tv_sec - boottime.tv_sec;
				int64_t microseconds = now.tv_usec - boottime.tv_usec;
				return seconds * 1000 + microseconds / 1000;
			}
		}
		return 0;

		// mach_absolute_time 在系统休眠时会被暂停，不适用
		//        uint64_t absolute = mach_absolute_time();
		//        uint64_t nanos = absolute * timebase_info_.numer / timebase_info_.denom;
		//        return nanos / 1000000;  // 转换为毫秒
	}
};

std::unique_ptr<SystemClock> createSystemClock() {
	return std::make_unique<AppleSystemClock>();
}
};  // namespace time_sync
