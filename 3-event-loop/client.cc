#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>

const size_t max_msg_size = 4096;

static int32_t send_req(int fd, const char *msg);
static int32_t write_all(int fd, const char *wbuf, size_t n);
static int32_t read_res(int fd);
static int32_t read_all(int connfd, char *rbuf, size_t n);

static int32_t send_req(int fd, const char *msg)
{
    uint32_t len = (uint32_t)strlen(msg) + 1;
    if (len > max_msg_size) {
        return -1;
    }

    char wbuf[4+max_msg_size];
    memcpy(wbuf, &len, 4);
    memcpy(wbuf+4, msg, len);
    return write_all(fd, wbuf, 4+len);
}

static int32_t write_all(int fd, const char *wbuf, size_t n)
{
    while (n > 0) {
        ssize_t rv = write(fd, wbuf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        wbuf += rv;
    }
    return 0;
}

static int32_t read_res(int fd)
{
    uint32_t len = 0;
    read_all(fd, (char *)&len, 4);

    char msg[max_msg_size] = {0};
    read_all(fd, msg, len);
    printf("RESPONSE: %s\n", msg);
    return 0;
}

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

int main(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("Failed socket()\n");
        exit(1);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rv < 0) {
        printf("Failed connect()\n");
        exit(1);
    }

    const int n = 5;
    const char *msg_list[n] = { "ciao!", "come", "stai", "amico", "mio"};
    for (size_t i = 0; i < n; i++) {
        int32_t err = send_req(fd, msg_list[i]);
        if (err) {
            printf("Failed write()\n");
            exit(1);
        }
    }

    for (size_t i = 0; i < n; i++) {
        int32_t err = read_res(fd);
        if (err) {
            printf("Failed read()\n");
            exit(1);
        }
    }

    close(fd);

    return 0;
}
