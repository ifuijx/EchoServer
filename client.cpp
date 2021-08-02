#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <string>

constexpr in_port_t port = 57311;

int main() {
    int fd;

    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ::printf("Failed to create socket, %s\n", ::strerror(errno));
        return -1;
    }

    addrinfo hint;
    ::memset(&hint, 0, sizeof(hint));

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    addrinfo *addrs;
    ::getaddrinfo("localhost", nullptr, &hint, &addrs);

    bool connected = false;
    for (auto addri = addrs; addri; addri = addri->ai_next) {
        sockaddr addr;
        ::memcpy(&addr, addri->ai_addr, sizeof(addri->ai_addr));
        sockaddr_in *iaddr = reinterpret_cast<sockaddr_in *>(&addr);
        iaddr->sin_port = port;
        if (::connect(fd, &addr, addri->ai_addrlen)) {
            ::printf("Failed to connect, %s\n", ::strerror(errno));
            continue;
        }
        else {
            connected = true;
            break;
        }
    }
    ::freeaddrinfo(addrs);

    if (connected) {
        ::printf("Connected successfully\n");
        std::string line;
        while (std::getline(std::cin, line)) {
            line += "\r\n";
            ::write(fd, line.c_str(), line.size());

            char buf[1024];
            std::string response;
            while (true) {
                auto res = ::read(fd, buf, 1023);
                if (res < 0) {
                    ::printf("Read error, %s\n", ::strerror(errno));
                    break;
                }
                buf[res] = 0;

                response += buf;
                if (response.size() >= 2 && !response.compare(response.size() - 2, 2, "\r\n")) {
                    break;
                }
            }
            if (response == "exit\r\n")
                break;
            ::printf("%s", response.c_str());
        }
    }

    ::close(fd);

    return EXIT_SUCCESS;
}
