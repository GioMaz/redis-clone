#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>

const size_t max_msg_size = 4096;

static int32_t send_req(int fd, std::vector<std::string> &cmd);
static int32_t write_all(int fd, const char *wbuf, size_t n);
static int32_t read_res(int fd);
static int32_t read_all(int connfd, char *rbuf, size_t n);

static int32_t send_req(int fd, std::vector<std::string> &cmd)
{
    uint32_t len = 4;
    for (const std::string &s : cmd) {
        len += 4 + s.size();
    }

    if (len > max_msg_size) {
        return -1;
    }

    char wbuf[4+max_msg_size];
    memcpy(wbuf, &len, 4);
    uint32_t n = cmd.size();
    memcpy(wbuf+4, &n, len);
    size_t pos = 8;
    for (std::string &s : cmd) {
        size_t sz = s.size();
        memcpy(wbuf+pos, &sz, 4);
        memcpy(wbuf+pos+4, s.data(), sz);
        pos += 4 + sz;
    }

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

    uint32_t rescode = 0;
    read_all(fd, (char *)&rescode, 4);

    char msg[max_msg_size] = {0};
    read_all(fd, msg, len-4);

    printf("response: %d %s\n", rescode, msg);
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

int main(int argc, char **argv)
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

    std::vector<std::string> cmd;
    for (size_t i = 1; i < argc; i++) {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err) {
        printf("err\n");
        return 1;
    }
    read_res(fd);

    close(fd);

    return 0;
}
