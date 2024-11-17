//
//  system_clock.hpp
//  sntp_client
//
//  Created by king on 2024/11/17.
//

#ifndef system_clock_hpp
#define system_clock_hpp

#include <memory>

namespace time_sync {

class SystemClock {
  public:
    virtual ~SystemClock() = default;

    // 获取当前 Unix 时间戳（毫秒）
    // 返回从 1970-01-01 00:00:00 UTC 到现在的毫秒数
    virtual uint64_t currentTimeMillis() = 0;
    
    // 获取自启动以来的时间（毫秒）
    virtual uint64_t elapsedRealtime() = 0;
};

std::unique_ptr<SystemClock> createSystemClock();

};  // namespace time_sync

#endif /* system_clock_hpp */
