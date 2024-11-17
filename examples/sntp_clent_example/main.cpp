
//
//  main.cpp
//  sntp_client_example
//
//  Created by king on 2024/11/17.
//

#include <sntp_client/sntp_client.hpp>

#include <iomanip>
#include <iostream>
#include <thread>

int main(int argc, char *const argv[]) {

    using namespace time_sync;

    // 创建NTP同步对象
    auto sntp = std::make_unique<SntpClient>();

    // 配置NTP服务器
    // Windows自带
    sntp->setServer("time.windows.com");
    // 苹果
    sntp->setServer("time.apple.com");
    // 阿里云
    sntp->setServer("ntp.aliyun.com");
    // 腾讯
    sntp->setServer("ntp.tencent.com");
    sntp->setVerbose(true);
    sntp->setTimeout(1);

    // 同步循环
    while (true) {
        if (sntp->needResync()) {
            std::cerr << "正在同步..." << std::endl;
            if (sntp->sync()) {
                std::cerr << "同步成功" << std::endl;
            } else {
                std::cerr << "同步失败" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        std::cerr << "当前服务器时间: " << sntp->getFormattedServerTime()
                  << " (上次同步: " << std::fixed << std::setprecision(1)
                  << sntp->getTimeSinceLastSync() << "秒前)"
                  << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return EXIT_SUCCESS;
}
