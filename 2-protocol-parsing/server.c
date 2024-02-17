#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <sys/errno.h>
#include <string.h>

const size_t max_msg_size = 4096;

static int32_t read_all(int connfd, char *rbuf, size_t n)
{
    while (n > 0) {
        ssize_t rv = read(connfd, rbuf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        rbuf += rv;
    }
    return 0;
}

static int32_t handle_request(int connfd)
{
    char rbuf[4+max_msg_size];
    errno = 0;
    int32_t err = read_all(connfd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            printf("EOF\n");
        } else {
            printf("Failed read()\n");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > max_msg_size) {
        printf("Message too long\n");
        return -1;
    }

    err = read_all(connfd, rbuf+4, len);
    if (err) {
        printf("Failed read()\n");
        return err;
    }

    rbuf[4+len] = 0;
    printf("message: %s\n", rbuf+4);
    return 0;
}

int main()
{
    // 1) socket()
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("Failed socket()\n");
        exit(1);
    }
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // 2) bind()
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0);
    int rv = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rv < 0) {
        printf("Failed bind()\n");
        exit(1);
    }

    // 3) listen()
    listen(fd, SOMAXCONN);

    while (1) {
        // 4) accept()
        struct sockaddr_in client_addr = {0};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            printf("Failed connect()\n");
            continue;
        }

        // 5) read()
        // 6) write()
        while (1) {
            int32_t err = handle_request(connfd);
            if (err) {
                break;
            }
        }

        close(connfd);
    }

    close(fd);

    return 0;
}
