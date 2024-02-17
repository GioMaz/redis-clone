#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main()
{
    // 1) socket(): create file descriptor
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    // Edit socket options
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // 2) bind(): Bind socket to address
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234); // port 1234
    addr.sin_addr.s_addr = ntohl(0); // address 0.0.0.0
    int rv = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rv < 0) {
        printf("Failed bind\n");
    }

    // 3) listen(): Listen for connections
    rv = listen(fd, SOMAXCONN);

    while (1) {
        // 4) accept(): Accept 
        struct sockaddr_in client_addr = {0};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue;
        }

        // 5) read(): Read from connfd
        char rbuf[64] = {0};
        ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
        if (n < 0) {
            return 0;
        }
        printf("message: %s\n", rbuf);

        // 6) write(): Write to connfd
        char wbuf[64] = "buongiorno anche a te!";
        write(connfd, wbuf, strlen(wbuf));

        // 7) close(): Close connfd
        close(connfd);
    }

    // 8) close(): Close fd
    close(fd);

    return 0;
}
