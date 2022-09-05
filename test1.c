#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ev.h>

/* message length limitation */
#define MAX_MESSAGE_LEN (256)

#define err_message(msg) \
    do {perror(msg); exit(EXIT_FAILURE);} while(0)


static void read_cb(struct ev_loop *loop, ev_io *watcher, int revents)
{
    ssize_t ret;
    char buf[MAX_MESSAGE_LEN] = {0};

    ret = recv(watcher->fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);    
    if (ret > 0) {
        write(watcher->fd, buf, ret);

    } else if ((ret < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;

    } else {
        fprintf(stdout, "client closed (fd=%d)\n", watcher->fd);
        ev_io_stop(loop, watcher);
        close(watcher->fd);
        free(watcher);
    }
}

static void accept_cb(struct ev_loop *loop, ev_io *watcher, int revents)
{
    int connfd;
    ev_io *client;

    connfd = accept(watcher->fd, NULL, NULL);
    if (connfd > 0) {
            client = (ev_io *)calloc(1, sizeof(*client));
            ev_io_init(client, read_cb, connfd, EV_READ);
            ev_io_start(loop, client);
        
    } else if ((connfd < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;

    } else {
        close(watcher->fd);
        ev_break(loop, EVBREAK_ALL);
        /* this will lead main to exit, no need to free watchers of clients */
    }
}

static void start_server(char const *addr, unsigned short port)
{
    int fd;
    struct ev_loop *loop = ev_loop_new(EVFLAG_AUTO | EVFLAG_NOENV);

    ev_io watcher;
    struct sockaddr_in server;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) err_message("socket err\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, addr, &server.sin_addr);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) err_message("bind err\n");
    if (listen(fd, 10) < 0) err_message("listen err\n");

    ev_io_init(&watcher, accept_cb, fd, EV_READ);
    ev_io_start(loop, &watcher);


    for (;;) {
        ev_run(loop, 0);
    }

    ev_loop_destroy(loop);
}

int main(int argc, char **argv)
{
    short port;
    if (argc != 2) exit(-1);
    printf("Слушаем на %s\r\n", argv[1]);
    port = atoi(argv[1]);
    
    start_server("127.0.0.1", port);

    return 0;
}