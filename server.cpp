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

    while (true) {
        int client;
        if ((client = ::accept(fd, nullptr, nullptr)) < 0) {
            ::printf("failed to accept, %s\n", ::strerror(errno));
        }

        pid_t pid;
        if ((pid = ::fork()) < 0) {
            ::printf("failed to create subprocess, %s\n", ::strerror(errno));
            return -1;
        }
        else if (pid == 0) {
            ::close(fd);
            if ((pid = ::fork()) > 0) {
                ::close(client);
                return -1;
            }
            else if (pid < 0) {
                ::printf("failed to create subprocess, %s\n", ::strerror(errno));
                ::close(client);
                return -1;
            }

            sockaddr peer;
            socklen_t len = sizeof(peer);
            ::getpeername(fd, &peer, &len);

            sockaddr_in *ppeer = reinterpret_cast<sockaddr_in *>(&peer);
            ::inet_ntop(AF_INET, &ppeer->sin_addr.s_addr, addr_str, sizeof(ppeer->sin_addr.s_addr));
            ::printf("connect to %s:%d\n", addr_str, ppeer->sin_port);

            while (true) {
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
                if (request == "exit\r\n")
                    break;
            }

            ::close(client);
            return 0;
        }
        else {
            ::close(client);
        }
    }

    ::close(fd);

    return 0;
}
