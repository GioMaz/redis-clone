#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>

const size_t max_msg_size = 4096;

static int32_t write_all(int fd, char *wbuf, size_t n)
{
    while (n > 0) {
        ssize_t wv = write(fd, wbuf, n);
        if (wv <= 0) {
            return -1;
        }
        assert((size_t)wv <= n);
        n -= (size_t)wv;
        wbuf += wv;
    }
    return 0;
}

int main(void)
{
    // 1) socket()
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("Failed socket()\n");
        exit(1);
    }

    // 2) connect()
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rv < 0) {
        printf("Failed connect()\n");
        exit(1);
    }

    // 3) write()
    {
        char msg[] = "buongiorno!";
        uint32_t len = (uint32_t)strlen(msg);
        if (len > max_msg_size)  {
            return -1;
        }
        char wbuf[4+max_msg_size];
        memcpy(wbuf, &len, 4);
        memcpy(wbuf+4, msg, len);
        int32_t err = write_all(fd, wbuf, 4+len);
        if (err) {
            printf("Failed write_all()\n");
        }
    }
    {
        char msg[] = "buongiorno!";
        uint32_t len = (uint32_t)strlen(msg);
        if (len > max_msg_size)  {
            return -1;
        }
        char wbuf[4+max_msg_size];
        memcpy(wbuf, &len, 4);
        memcpy(wbuf+4, msg, len);
        int32_t err = write_all(fd, wbuf, 4+len);
        if (err) {
            printf("Failed write_all()\n");
        }
    }


    close(fd);

    return 0;
}
