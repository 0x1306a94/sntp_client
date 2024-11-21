//
//  sntp_client.cpp
//  sntp_client
//
//  Created by king on 2024/11/17.
//

#include "sntp_client.hpp"

#include "sntp_types.h"
#include "system_clock.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <ctime>
#include <iomanip>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <vector>

#include <iomanip>
#include <iostream>

namespace time_sync {
// NTP时间从1900年开始,需要和Unix时间(1970年开始)转换
constexpr uint64_t NTP_TIMESTAMP_DELTA = 2208988800ull;

constexpr int NTP_PACKET_SIZE = 48;

constexpr char STANDARD_NTP_PORT[] = "123";
constexpr int NTP_MODE_CLIENT = 3;
constexpr int NTP_MODE_SERVER = 4;
constexpr int NTP_MODE_BROADCAST = 4;
constexpr int NTP_VERSION = 3;

constexpr int NTP_LEAP_NOSYNC = 3;
constexpr int NTP_STRATUM_DEATH = 0;
constexpr int NTP_STRATUM_MAX = 15;

struct TimeResult {
    double offset;          // 时间偏移
    double delay;           // 往返延迟
    double sync_boot_time;  // 同步时的boottime（毫秒）
    double sync_time;       // 同步时的服务器时间（秒）
};

class SntpClient::Implement {
  public:
    std::unique_ptr<SystemClock> system_clock_;
    std::string server_;
    int timeout_sec_{1};
    bool verbose_{false};
    /// 同步时的boottime（毫秒）
    uint64_t base_boottime_{0};
    /// 同步时的服务器时间（秒）
    double base_server_time_{0};
    bool is_synced_{false};

    Implement()
        : system_clock_(createSystemClock()) {}

    void setServer(const std::string &server) {
        server_ = server;
    }

    void setTimeout(int seconds) {
        timeout_sec_ = seconds;
    }

    void setVerbose(bool verbose) {
        verbose_ = verbose;
    }

    void printNtpTimestamp(const char *prefix, const ntp_timestamp &ts, bool is_network_order = true) {

        uint32_t seconds = is_network_order ? ntohl(ts.seconds) : ts.seconds;
        uint32_t fraction = is_network_order ? ntohl(ts.fraction) : ts.fraction;

        double timestamp = (seconds - NTP_TIMESTAMP_DELTA) + ((double)fraction / (1LL << 32));
        time_t time = seconds - NTP_TIMESTAMP_DELTA;
        std::cerr << prefix << ": "
                  << getFormattedTime(time) << "." << std::setfill('0') << std::setw(9)
                  << (uint32_t)((double)ts.fraction * 1000000000.0 / (1LL << 32))
                  << " (" << seconds << "." << fraction << ")"
                  << std::endl;
    }

    void printSntpPacket(const char *prefix, const sntp_packet &packet, bool is_network_order = true) {
        std::cerr << "=== " << prefix << " ===" << std::endl;

        // LI VN Mode
        std::cerr << "LI: " << (int)packet.lvm.li
                  << ", VN: " << (int)packet.lvm.vn
                  << ", Mode: " << (int)packet.lvm.mode << std::endl;

        // Stratum
        std::cerr << "Stratum: " << (int)packet.stratum;
        if (packet.stratum == 0) {
            std::cerr << " (unspecified or invalid)";
        } else if (packet.stratum == 1) {
            std::cerr << " (primary reference)";
        } else if (packet.stratum <= 15) {
            std::cerr << " (secondary reference)";
        } else {
            std::cerr << " (reserved)";
        }
        std::cerr << std::endl;

        // Poll interval
        std::cerr << "Poll Interval: " << (int)packet.poll
                  << " (2^" << (int)packet.poll << " seconds)" << std::endl;

        // Precision
        std::cerr << "Precision: " << (int)packet.precision
                  << " (2^" << (int)packet.precision << " seconds)" << std::endl;

        // Root Delay
        double root_delay = 0;
        if (is_network_order) {
            root_delay = ntohs(packet.root_delay_int) +
                ntohs(packet.root_delay_fraction) / 65536.0;
        } else {
            root_delay = packet.root_delay_int +
                packet.root_delay_fraction / 65536.0;
        }
        std::cerr << "Root Delay: " << std::fixed << std::setprecision(6)
                  << root_delay << " seconds" << std::endl;

        // Root Dispersion
        double root_dispersion = 0;
        if (is_network_order) {
            root_dispersion = ntohs(packet.root_dispersion_int) +
                ntohs(packet.root_dispersion_fraction) / 65536.0;
        } else {
            root_dispersion = packet.root_dispersion_int +
                packet.root_dispersion_fraction / 65536.0;
        }
        std::cerr << "Root Dispersion: " << std::fixed << std::setprecision(6)
                  << root_dispersion << " seconds" << std::endl;

        // Reference Identifier
        char refid[5] = {0};
        memcpy(refid, packet.ref_id, 4);
        std::cerr << "Reference ID: " << refid << std::endl;

        // Timestamps
        printNtpTimestamp("Reference Timestamp", packet.ref_time, is_network_order);
        printNtpTimestamp("Origin Timestamp", packet.ori_time, is_network_order);
        printNtpTimestamp("Receive Timestamp", packet.recv_time, is_network_order);
        printNtpTimestamp("Transmit Timestamp", packet.tran_time, is_network_order);

        std::cerr << "====================\n"
                  << std::endl;
    }

    bool sync() {
        auto result = syncOnce();
        if (!result.has_value()) {
            return false;
        }

        base_boottime_ = result->sync_boot_time;
        base_server_time_ = result->sync_time;
        is_synced_ = true;

        return true;
    }

    std::optional<TimeResult> syncOnce() {

        // 解析服务器地址
        struct addrinfo hints = {}, *servinfo;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(server_.c_str(), STANDARD_NTP_PORT, &hints, &servinfo) != 0) {
            if (verbose_) {
                std::cerr << "getaddrinfo fail" << std::endl;
            }
            return std::nullopt;
        }

        // 创建socket
        int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        if (sockfd < 0) {
            if (verbose_) {
                std::cerr << "socket create failed: " << strerror(errno) << std::endl;
            }
            freeaddrinfo(servinfo);
            return std::nullopt;
        }

        // Prevent SIGPIPE signals
        //防止终止进程的信号？
        int nosigpipe = 1;
        //SO_NOSIGPIPE是为了避免网络错误，而导致进程退出。用这个来避免系统发送signal
        setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));

        // 设置超时
        struct timeval tv;
        tv.tv_sec = timeout_sec_;
        tv.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            if (verbose_) {
                std::cerr << "set receive timeout failed: " << strerror(errno) << std::endl;
            }
            close(sockfd);
            freeaddrinfo(servinfo);
            return std::nullopt;
        }

        struct sntp_packet sntp_request;
        struct sntp_packet sntp_reply;

        bzero(&sntp_request, sizeof(struct sntp_packet));
        sntp_request.lvm.li = 0;
        sntp_request.lvm.vn = NTP_VERSION;
        sntp_request.lvm.mode = NTP_MODE_CLIENT;

        // 记录发送时间 (t1)
        double requestTime = system_clock_->currentTimeMillis() / 1000.0;
        uint32_t ntp_seconds = (uint32_t)(requestTime + NTP_TIMESTAMP_DELTA);
        uint32_t ntp_fraction = (uint32_t)((requestTime - (int)requestTime) * (1LL << 32));

        // 设置发送时间戳
        sntp_request.tran_time.seconds = htonl(ntp_seconds);
        sntp_request.tran_time.fraction = htonl(ntp_fraction);

        if (verbose_) {
            printSntpPacket("SNTP Request", sntp_request, true);
        }

        // 发送请求
        if (sendto(sockfd, &sntp_request, sizeof(struct sntp_packet), 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
            if (verbose_) {
                std::cerr << "send request failed: " << strerror(errno) << std::endl;
            }
            close(sockfd);
            freeaddrinfo(servinfo);
            return std::nullopt;
        }

        // 接收响应
        if (recvfrom(sockfd, &sntp_reply, sizeof(struct sntp_packet), 0, nullptr, nullptr) < 0) {
            if (verbose_) {
                std::cerr << "receive response failed: " << strerror(errno)
                          << " (timeout=" << timeout_sec_ << "s)" << std::endl;
            }
            close(sockfd);
            freeaddrinfo(servinfo);
            return std::nullopt;
        }

        close(sockfd);
        freeaddrinfo(servinfo);

        if (verbose_) {
            printSntpPacket("SNTP Response", sntp_reply, true);
        }

        if (sntp_reply.lvm.mode != NTP_MODE_SERVER) {
            if (verbose_) {
                std::cerr << "received packet is invalid" << std::endl;
            }
            return std::nullopt;
        }

        // 记录接收时间 (t4)
        double t4 = system_clock_->currentTimeMillis() / 1000.0;

        sntp_reply.ref_time.seconds = ntohl(sntp_reply.ref_time.seconds);
        sntp_reply.ref_time.fraction = ntohl(sntp_reply.ref_time.fraction);
        sntp_reply.ori_time.seconds = ntohl(sntp_reply.ori_time.seconds);
        sntp_reply.ori_time.fraction = ntohl(sntp_reply.ori_time.fraction);
        sntp_reply.recv_time.seconds = ntohl(sntp_reply.recv_time.seconds);
        sntp_reply.recv_time.fraction = ntohl(sntp_reply.recv_time.fraction);
        sntp_reply.tran_time.seconds = ntohl(sntp_reply.tran_time.seconds);
        sntp_reply.tran_time.fraction = ntohl(sntp_reply.tran_time.fraction);

        int stratum = sntp_reply.stratum & 0xff;
        if (checkValidServerReply(sntp_reply.lvm.li, sntp_reply.lvm.mode, stratum, sntp_reply.tran_time, sntp_reply.ref_time, sntp_request.tran_time, sntp_reply.ori_time)) {
            return std::nullopt;
        }

        // 转换NTP时间戳为Unix时间戳
        double t1 = (sntp_reply.ori_time.seconds - NTP_TIMESTAMP_DELTA) + (double)sntp_reply.ori_time.fraction / (1LL << 32);
        double t2 = (sntp_reply.recv_time.seconds - NTP_TIMESTAMP_DELTA) + (double)sntp_reply.recv_time.fraction / (1LL << 32);
        double t3 = (sntp_reply.tran_time.seconds - NTP_TIMESTAMP_DELTA) + (double)sntp_reply.tran_time.fraction / (1LL << 32);

        TimeResult result = {0, 0, 0};

        // 计算往返延迟和偏移
        result.delay = (t4 - t1) - (t3 - t2);
        result.offset = ((t2 - t1) + (t3 - t4)) / 2;

        result.sync_boot_time = system_clock_->elapsedRealtime();
        // 计算同步时的服务器时间
        result.sync_time = t4 + result.offset;

        if (verbose_) {
            std::cerr << "本地发送时间(t1)=" << getFormattedTime(t1)
                      << ", 服务器接收时间(t2)=" << getFormattedTime(t2)
                      << ", 服务器发送时间(t3)=" << getFormattedTime(t3)
                      << ", 本地接收时间(t4)=" << getFormattedTime(t4)
                      << std::fixed << std::setprecision(2)
                      << ", 延迟=" << result.delay * 1000.0 << "ms"
                      << ", 偏移=" << result.offset * 1000.0 << "ms"
                      << ", 同步时服务器时间=" << getFormattedTime(result.sync_time)
                      << std::endl;
        }

        return result;
    }

    bool checkValidServerReply(int leap, int mode, int stratum, const ntp_timestamp &transmitTimestamp, const ntp_timestamp &referenceTimestamp, const ntp_timestamp &requestTimestamp, const ntp_timestamp &originateTimestamp) {
        if (leap == NTP_LEAP_NOSYNC) {
            if (verbose_) {
                std::cerr << "unsynchronized server" << std::endl;
            }
            return false;
        }

        if ((mode != NTP_MODE_SERVER) && (mode != NTP_MODE_BROADCAST)) {
            if (verbose_) {
                std::cerr << "untrusted mode: " << mode << std::endl;
            }
            return false;
        }

        if ((stratum == NTP_STRATUM_DEATH) || (stratum > NTP_STRATUM_MAX)) {
            if (verbose_) {
                std::cerr << "untrusted stratum: " << stratum << std::endl;
            }
            return false;
        }

        if (requestTimestamp.seconds != originateTimestamp.seconds || requestTimestamp.fraction != originateTimestamp.fraction) {
            if (verbose_) {
                std::cerr << "originateTimestamp != randomizedRequestTimestamp" << std::endl;
            }
            return false;
        }

        if (transmitTimestamp.seconds == 0 && transmitTimestamp.fraction == 0) {
            if (verbose_) {
                std::cerr << "zero transmitTimestamp" << std::endl;
            }
            return false;
        }

        if (referenceTimestamp.seconds == 0 && referenceTimestamp.fraction == 0) {
            if (verbose_) {
                std::cerr << "zero referenceTimestamp" << std::endl;
            }
            return false;
        }

        return true;
    }

    double getServerTime() const {
        if (!is_synced_) {
            return 0;
        }
        uint64_t now = system_clock_->elapsedRealtime();
        double elapsed = (now - base_boottime_) / 1000.0;
        return base_server_time_ + elapsed;
    }

    std::string getFormattedServerTime() const {
        time_t server_time = static_cast<time_t>(getServerTime());
        struct tm *timeinfo = localtime(&server_time);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }

    std::string getFormattedTime(time_t t) const {
        time_t time = static_cast<time_t>(t);
        struct tm *timeinfo = localtime(&time);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }

    bool isSynced() const {
        return is_synced_;
    }

    double getTimeSinceLastSync() const {
        if (!is_synced_) {
            return 0;
        }
        uint64_t now = system_clock_->elapsedRealtime();
        return (now - base_boottime_) / 1000.0;
    }

    bool needResync(double max_interval) const {
        return !is_synced_ || getTimeSinceLastSync() > max_interval;
    }
};

SntpClient::SntpClient()
    : impl_(std::make_unique<Implement>()) {
}

SntpClient::~SntpClient() {
}

void SntpClient::setServer(const std::string &server) {
    impl_->setServer(server);
}

void SntpClient::setTimeout(int seconds) {
    impl_->setTimeout(seconds);
}

void SntpClient::setVerbose(bool verbose) {
    impl_->setVerbose(verbose);
}

bool SntpClient::sync() {
    return impl_->sync();
}

double SntpClient::getServerTime() const {
    return impl_->getServerTime();
}

std::string SntpClient::getFormattedServerTime() const {
    return impl_->getFormattedServerTime();
}

bool SntpClient::isSynced() const {
    return impl_->isSynced();
}

double SntpClient::getTimeSinceLastSync() const {
    return impl_->getTimeSinceLastSync();
}

bool SntpClient::needResync(double max_interval) const {
    return impl_->needResync(max_interval);
}
};  // namespace time_sync
