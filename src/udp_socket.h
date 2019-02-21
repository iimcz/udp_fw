//
// Created by neneko on 20.2.19.
//

#ifndef UDF_FW_UDP_SOCKET_H
#define UDF_FW_UDP_SOCKET_H

#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>

struct udp_socket_t {
    udp_socket_t() : socket_{::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)}, buffer_(65536, 0), stopping_{false} {
        if (socket_ < 0) {
            report_error();
        }
    }

    ~udp_socket_t() {
        close(socket_);
    }

    void bind(uint16_t port) {
        sockaddr_in addr;
        ::memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(socket_, reinterpret_cast<sockaddr *>(&addr), sizeof(sockaddr_in)) != 0) {
            report_error();
        }
    }

    void enable_broadcast() {
        int broadcast = 1;
        if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) != 0) {
            report_error();
        }
    }

    std::vector<char> &wait_for_data() noexcept {
        while (active()) {
            pollfd fds = {socket_, POLLIN, 0};
            const auto ret = ::poll(&fds, 1, 1000);
            if (ret < 0 && errno != EINTR) {
                show_error("wait_for_data");
            }
            if (ret > 0) {
                return read_data();
            }
        }
        return read_data();
    }

    std::vector<char> &read_data() noexcept {
        buffer_.resize(buffer_.capacity());
        const size_t len = buffer_.size();
        sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        auto ret = recvfrom(socket_, reinterpret_cast<void *>(&buffer_[0]), len, MSG_DONTWAIT,
                            reinterpret_cast<sockaddr *>(&addr), &addr_len);
        if (ret < 0 && errno != EAGAIN) {
            show_error("read_data");
        }
        if (ret < 0) {
            ret = 0;
        }
        buffer_.resize(ret);
        return buffer_;
    }

    void connect(const char *address, const char *port) {
        struct addrinfo hints;
        struct addrinfo *result = nullptr;
        struct addrinfo *addr_info = nullptr;
        ::memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        auto s = getaddrinfo(address, port, &hints, &result);
        if (s != 0) {
            throw std::runtime_error(gai_strerror(s));
        }

        for (addr_info = result; addr_info != nullptr; addr_info = addr_info->ai_next) {
            if (::connect(socket_, addr_info->ai_addr, addr_info->ai_addrlen) < 0) {
                if (!addr_info->ai_next) {
                    report_error();
                }
            } else {
                return;
            }
        }
        throw std::runtime_error("Failed to connect");
    }


    void send(const std::vector<char> &data) noexcept {
        if (::send(socket_, &data[0], data.size(), 0) < static_cast<ssize_t >(data.size())) {
            show_error("send_data");
        }
    }

    void stop() noexcept {
        stopping_ = true;
    }

    bool active() const noexcept {
        return !stopping_;
    }

private:
    int socket_;
    std::vector<char> buffer_;
    bool stopping_;

    void report_error() const {
        throw std::runtime_error(::strerror(errno));
    }

    void show_error(const std::string &where) const {
        std::cerr << "Error in " << where << ": " << ::strerror(errno) << "\n";
    }
};


#endif //UDF_FW_UDP_SOCKET_H
