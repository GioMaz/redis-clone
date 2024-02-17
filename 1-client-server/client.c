#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const size_t max_msg_size = 4096;

int main()
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
    char msg[] = "buongiorno!";
    write(fd, msg, strlen(msg));

    // 4) read()
    char rbuf[64] = {0};
    ssize_t n = read(fd, rbuf, sizeof(rbuf)-1);
    if (n < 0) {
        printf("Failed read()\n");
        exit(1);
    }
    printf("message: %s\n", rbuf);

    // 5) close()
    close(fd);

    return 0;
}
