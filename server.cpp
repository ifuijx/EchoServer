#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <string>
#include <sys/select.h>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <memory>
#include <limits>

int initserver(int type, const sockaddr *addr, socklen_t alen, int qlen) {
    int fd;
    int reuse = 1;

    if ((fd = ::socket(addr->sa_family, type, 0)) < 0)
        return -1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
        goto errout;
    if (::bind(fd, addr, alen) < 0)
        goto errout;
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET)
        if (::listen(fd, qlen) < 0)
            goto errout;
    return fd;

errout:
    int err = errno;
    ::close(fd);
    errno = err;
    return -1;
}

template <typename TEle>
class AsyncSet {
public:
    void add(TEle val) {
        std::scoped_lock<std::mutex> lock(mutex_);
        set_.insert(val);
    }

    void erase(TEle val) {
        std::scoped_lock<std::mutex> lock(mutex_);
        set_.erase(val);
    }

    std::vector<TEle> items() const {
        std::scoped_lock<std::mutex> lock(mutex_);
        std::vector<TEle> ret(set_.begin(), set_.end());
        return ret;
    }

private:
    mutable std::mutex mutex_;
    std::set<TEle> set_;
};

constexpr in_port_t port = 57311;

int main() {
    addrinfo hint;
    ::memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    addrinfo *addrs;
    ::getaddrinfo("localhost", nullptr, &hint, &addrs);

    char addr_str[INET_ADDRSTRLEN];

    addrinfo *cursor = addrs;
    sockaddr_in addr;
    while (cursor) {
        /* The domain is AF_INET */
        sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(cursor->ai_addr);

        if (!::inet_ntop(addr_in->sin_family, &addr_in->sin_addr.s_addr, addr_str, INET_ADDRSTRLEN))
            ::printf("conversion failed\n");
        else {
            ::printf("%s\n", addr_str);

            ::memcpy(&addr, addr_in, sizeof(sockaddr_in));
        }

        cursor = cursor->ai_next;
    }
    ::freeaddrinfo(addrs);

    addr.sin_port = port;

    int fd = initserver(SOCK_STREAM, reinterpret_cast<sockaddr *>(&addr), sizeof(addr), 1);
    if (fd < 0) {
        ::printf("failed to create socket\n");
        return -1;
    }

    AsyncSet<int> async_fds;
    std::thread echo([&async_fds] {
        while (true) {
            auto fds = async_fds.items();
            fd_set fd_set;
            FD_ZERO(&fd_set);
            int max_fd = std::numeric_limits<int>::min();
            for (auto fd : fds) {
                max_fd = std::max(max_fd, fd);
                FD_SET(fd, &fd_set);
            }
            max_fd = std::max(max_fd, 0);

            timeval timeout {
                .tv_sec = 0,
                .tv_usec = 1000 * 50,
            };
            auto res = ::select(max_fd + 1, &fd_set, nullptr, nullptr, &timeout);

            if (res < 0) {
                ::printf("Failed to select, %s\n", ::strerror(errno));
                return;
            }
            else {
                for (int fd = 0, cnt = 0; fd <= max_fd && cnt < res; ++fd) {
                    if (FD_ISSET(fd, &fd_set)) {
                        ++cnt;

                        int client = fd;
                        std::string request;
                        char buf[1025];
                        while (true) {
                            ssize_t res = ::read(client, buf, 1024);
                            if (res < 0)
                                break;
                            buf[res] = '\0';
                            request += buf;
                            if (request.size() >= 2 && !request.compare(request.size() - 2, 2, "\r\n"))
                                break;
                        }

                        ::write(client, request.c_str(), request.size());
                        if (request == "exit\r\n") {
                            ::close(client);
                            async_fds.erase(client);
                        }
                    }
                }
            }
        }
    });
    echo.detach();

    while (true) {
        int client;
        if ((client = ::accept(fd, nullptr, nullptr)) < 0) {
            ::printf("failed to accept, %s\n", ::strerror(errno));
        }

        sockaddr peer;
        socklen_t len = sizeof(peer);
        ::getpeername(client, &peer, &len);

        sockaddr_in *ppeer = reinterpret_cast<sockaddr_in *>(&peer);
        ::inet_ntop(AF_INET, &ppeer->sin_addr.s_addr, addr_str, sizeof(ppeer->sin_addr.s_addr));
        ::printf("connect to %s:%d\n", addr_str, ppeer->sin_port);

        async_fds.add(client);
    }

    ::close(fd);

    return 0;
}
