#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <sys/errno.h>
#include <string>
#include <fcntl.h>
#include <vector>
#include <poll.h>
#include <map>

const size_t max_msg_size = 4096;

enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};

enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

struct Conn {
    int fd = -1;
    int state = 0;
    size_t rbuf_size = 0;
    uint8_t rbuf[4+max_msg_size];
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4+max_msg_size];
};

static void state_req(Conn *conn); // Handle conn in request state
static bool try_read_buffer(Conn *conn); // Read buffer until you are able to
                                         // parse a full request
static bool try_one_request(Conn *conn); // Parse a full request

static void state_res(Conn *conn); // Handle conn in response state
static bool try_write_buffer(Conn *conn); // Write buffer until full response
                                          // is written

static bool do_request(
        const uint8_t *req, uint32_t reqlen,
        uint32_t *rescode, uint8_t *res, uint32_t *reslen); // Execute request logic

static bool parse_cmd(const uint8_t *req, uint32_t reqlen,
        std::vector<std::string> &cmd); // Parse command

static int do_get(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen); // Execute get logic
static int do_set(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen); // Execute set logic
static int do_del(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen); // Execute del logic

static void handle_conn(Conn *conn)
{
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0);
    }
}

static void state_req(Conn *conn)
{
    while (try_read_buffer(conn)) {}
}

static bool try_read_buffer(Conn *conn)
{
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;
    do {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0 && errno == EAGAIN) {
        return false;
    }

    if (rv < 0) {
        printf("Failed read()\n");
        conn->state = STATE_END;
        return false;
    }

    if (rv == 0) {
        if (conn->rbuf_size > 0) {
            printf("Unexpected EOF\n");
        } else {
            printf("EOF\n");
        }
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    while (try_one_request(conn)) {}
    return (conn->state == STATE_REQ);
}

static bool try_one_request(Conn *conn)
{
    if (conn->rbuf_size < 4) {
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, conn->rbuf, 4);
    if (len > max_msg_size) {
        printf("Message too long\n");
        conn->state = STATE_END;
        return false;
    }

    // Not enough data in the buffer
    if (4 + len > conn->rbuf_size) {
        return false;
    }

    uint32_t rescode = RES_OK;
    uint32_t wlen = 0;
    // rbuf = 4 len + n payload
    // wbuf = 4 len + 4 rescode + n payload
    int32_t err = do_request(
        conn->rbuf+4, len,
        &rescode, conn->wbuf+4+4, &wlen
    );
    if (err) {
        conn->state = STATE_END;
        return false;
    }
    wlen += 4;
    memcpy(conn->wbuf, &wlen, 4);
    memcpy(conn->wbuf+4, &rescode, 4);
    conn->wbuf_size = 4 + wlen;

    size_t remain = conn->rbuf_size - 4 - len;
    if (remain) {
        memmove(conn->rbuf, conn->rbuf + 4 + len, remain);
    }
    conn->rbuf_size = remain;

    conn->state = STATE_RES;
    state_res(conn);

    return (conn->state == STATE_REQ);
}

static void state_res(Conn *conn)
{
    while (try_write_buffer(conn)) {}
}

static bool try_write_buffer(Conn *conn)
{
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0 && errno == EAGAIN) {
        return false;
    }

    if (rv < 0) {
        printf("Failed write()\n");
        conn->state = STATE_END;
        return false;
    }

    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if (conn->wbuf_sent == conn->wbuf_size) {
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }

    return true;
}

static bool cmd_is(std::string s1, const char *s2)
{
    return strcmp(s1.data(), s2) == 0;
}

static bool do_request(
        const uint8_t *req, uint32_t reqlen,
        uint32_t *rescode, uint8_t *res, uint32_t *reslen)
{
    std::vector<std::string> cmd;
    if (parse_cmd(req, reqlen, cmd) != 0) {
        printf("Bad request\n");
        return -1;
    }

    // printf("ECCOME\n");
    // printf("cmd.size() %zu\n", cmd.size());
    // printf("cmd[0] %s\n", cmd[0].c_str());
    // printf("cmd.size() == 3 %d\n", cmd.size() == 3);
    // printf("cmd_is(cmd[0], set) %d\n", cmd_is(cmd[0], "set"));
    if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        *rescode = do_get(cmd, res, reslen);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        *rescode = do_set(cmd, res, reslen);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        *rescode = do_del(cmd, res, reslen);
    } else {
        *rescode = RES_ERR;
        const char *msg= "Bad request";
        strcpy((char *)res, msg);
        *reslen = strlen(msg);
        return 0;
    }

    return 0;
}

static bool parse_cmd(const uint8_t *req, uint32_t reqlen,
        std::vector<std::string> &cmd)
{
    if (reqlen < 4) {
        return false;
    }

    uint32_t n = 0;
    memcpy(&n, req, 4);

    size_t pos = 4;
    while (n--) {
        if (pos + 4 > reqlen) {
            printf("pos+4: %zu, reqlen: %d\n", pos+4, reqlen);
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, req+pos, 4);
        cmd.push_back(std::string((char *)req+pos+4, sz));
        pos += 4 + sz;
    }

    if (pos != reqlen) {
        return -1;
    }

    return 0;
}

static std::map<std::string, std::string> g_map;

static int do_get(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    // printf("GET\n");
    if (g_map.count(cmd[1]) == 0) {
        return RES_NX;
    }

    std::string &val = g_map[cmd[1]];
    assert(val.size() <= max_msg_size);
    memcpy(res, val.data(), val.size());
    *reslen = (uint32_t)val.size();
    return RES_OK;
}

static int do_set(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    // printf("SET\n");
    g_map[cmd[1]] = cmd[2];
    return RES_OK;
}

static int do_del(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    // printf("DEL\n");
    if (g_map.count(cmd[1]) == 0) {
        return RES_NX;
    }

    g_map.erase(cmd[1]);
    return RES_OK;
}

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn)
{
    if (fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }

    fd2conn[conn->fd] = conn;
}

static void fd_set_nb(int fd)
{
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        printf("Failed fcntl()\n");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        printf("Failed fcntl()\n");
    }
}

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd)
{
    struct sockaddr_in client_addr = {0};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0) {
        printf("Failed accept()\n");
        return -1;
    }

    fd_set_nb(connfd);

    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
    if (!conn) {
        close(connfd);
        printf("Failed malloc()\n");
        return -1;
    }

    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);

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
    if (rv < 0) {
        printf("Failed listen()\n");
        exit(1);
    }

    // Use non blocking mode for fd
    // In blocking mode:
    // - accept() blocks when there are no connections in the kernel queue
    // - read() blocks when there is no data in the kernel
    // - write() blocks when the kernel write buffer is full
    // In non blocking mode:
    // - accept() returns EAGAIN when there are no connections in the kernel queue
    // - read() returns EAGAIN when there is no data in the kernel
    // - write() returns EAGAIN when the kernel write buffer is full
    // - poll() blocks until the fds array has a ready fd
    fd_set_nb(fd);

    // Map from fd to Conn *
    std::vector<Conn *> fd2conn;

    // Array of fds
    std::vector<struct pollfd> pollfds;
    while (1) {
        pollfds.clear();
        struct pollfd pfd = {fd, POLLIN, 0};
        pollfds.push_back(pfd);

        // Repopulate pollfds (from fd2conn)
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }

            struct pollfd pfd = {0};
            pfd.fd = conn->fd;
            pfd.events = conn->state == STATE_REQ ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            pollfds.push_back(pfd);
        }

        // 4) poll(): Edit pollfd.revents flags
        int rv = poll(pollfds.data(), (nfds_t)pollfds.size(), 1000);
        if (rv < 0) {
            printf("Failed poll()\n");
        }

        // 5) read() / write() all the ready fds
        for (size_t i = 1; i < pollfds.size(); i++) {
            if (pollfds[i].revents) {
                Conn *conn = fd2conn[pollfds[i].fd];
                handle_conn(conn);
                if (conn->state == STATE_END) {
                    fd2conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn);
                }
            }
        }

        // 6) accept()
        if (pollfds[0].revents) {
            (void)accept_new_conn(fd2conn, fd);
        }
    }

    printf("Closing welcome connection\n");

    close(fd);

    return 0;
}
