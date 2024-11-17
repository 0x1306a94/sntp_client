//
//  sntp_client.hpp
//  sntp_client
//
//  Created by king on 2024/11/17.
//

#ifndef sntp_client_hpp
#define sntp_client_hpp

#include <memory>
#include <optional>
#include <string>

namespace time_sync {

class SntpClient {
  public:
    /// 创建SntpClient
    explicit SntpClient();

    ~SntpClient();

    /// 配置NTP服务
    /// - Parameter server: 例如 time.apple.com time.windows.com ntp.aliyun.com ntp.tencent.com
    void setServer(const std::string &server);

    /// 设置超时时间
    /// - Parameter seconds: 超时时间，默认1s
    void setTimeout(int seconds);

    /// 启用详细信息输出
    /// - Parameter verbose:
    void setVerbose(bool verbose);
    
    /// 执行同步
    bool sync();

    /// 获取同步后当前服务器时间（秒）
    double getServerTime() const;
    /// 获取格式化的服务器时间
    std::string getFormattedServerTime() const;

    /// 是否已同步
    bool isSynced() const;

    /// 获取上次同步后的时间（秒）
    double getTimeSinceLastSync() const;

    /// 是否需要重新同步
    /// - Parameter max_interval: 最大间隔（秒）默认 3600s
    bool needResync(double max_interval = 3600) const;

  private:
    class Implement;
    std::unique_ptr<Implement> impl_;
};
};  // namespace time_sync

#endif /* sntp_client_hpp */
