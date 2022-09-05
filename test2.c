#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <ev.h>

#define thread_interaction_on_port	1025

/* message length limitation */
#define MAX_MESSAGE_LEN (256)

#define err_message(msg) \
    do {perror(msg); exit(EXIT_FAILURE);} while(0)


char *strrev(char *str)
{
    if (!str || ! *str)
        return str;

    int i = strlen(str) - 1, j = 0;

    char ch;
    while (i > j)
    {
        ch = str[i];
        str[i] = str[j];
        str[j] = ch;
        i--;
        j++;
    }
    return str;
}

/* Основной поток 
   - манипуляция  со строкой
*/
static void mread_cb(struct ev_loop *loop, ev_io *watcher, int revents)
{
    ssize_t ret;
    char buf[MAX_MESSAGE_LEN] = {0};

    ret = recv(watcher->fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);    
    if (ret > 0) {
	/* Переворот строки */
	strrev(buf);
        
	write(watcher->fd, buf, ret);

    } else if ((ret < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;

    } else {
        ev_io_stop(loop, watcher);
        close(watcher->fd);
        free(watcher);
    }
}

static void maccept_cb(struct ev_loop *loop, ev_io *watcher, int revents)
{
    int connfd;
    ev_io *client;

    connfd = accept(watcher->fd, NULL, NULL);
    if (connfd > 0) {
            client = (ev_io *)calloc(1, sizeof(*client));
            ev_io_init(client, mread_cb, connfd, EV_READ);
            ev_io_start(loop, client);
        
    } else if ((connfd < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;

    } else {
        close(watcher->fd);
        ev_break(loop, EVBREAK_ALL);
        /* this will lead main to exit, no need to free watchers of clients */
    }
}

static void mstart_server(char const *addr, unsigned short port)
{
    int fd;
    struct ev_loop *loop = ev_loop_new(EVFLAG_AUTO | EVFLAG_NOENV);

    ev_io watcher;
    struct sockaddr_in server;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) err_message("child socket err\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, addr, &server.sin_addr);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) err_message("bind err\n");
    if (listen(fd, 10) < 0) err_message("child listen err\n");

    ev_io_init(&watcher, maccept_cb, fd, EV_READ);
    ev_io_start(loop, &watcher);


    for (;;) {
        ev_run(loop, 0);
    }

    ev_loop_destroy(loop);
}

/* Поток */
static void read_cb(struct ev_loop *loop, ev_io *watcher, int revents)
{
    ssize_t ret;
    char buf[MAX_MESSAGE_LEN] = {0};

    ret = recv(watcher->fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);    
    if (ret > 0) {
	/*  Отправка данных на модификацию в другой поток */
	struct sockaddr_in cl;
	
	/* Блокирующий режим работы сокета */
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) err_message("socket err\n");
	cl.sin_family = AF_INET;
	cl.sin_port = htons(thread_interaction_on_port);
	inet_pton(AF_INET, "127.0.0.1", &cl.sin_addr);
	int flg;
	fcntl(fd, F_GETFL, &flg);
	flg &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flg);
	if (connect(fd, (struct sockaddr *)&cl, sizeof(struct sockaddr_in))==0)
	{
    	    write(fd, buf, ret);
	    ret = recv(fd, buf, sizeof(buf) , 0);    
	    if (ret > 0) 
	    {
        	write(watcher->fd, buf, ret);
	    }
	}
	close(fd);
    }
    else if ((ret < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;

    } else {
        fprintf(stdout, "client disconnected (fd=%d)\n", watcher->fd);
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

void* potok(void *v)
{
    printf("Межпоточное взаимодействие на порту %i\r\n", thread_interaction_on_port);        
    mstart_server("127.0.0.1", thread_interaction_on_port);
	return 0;
}

int main(int argc, char **argv)
{
    short port;
    if (argc != 2) 
    {
	exit(-1); 
    }
    port = atoi(argv[1]);
    
    pthread_t tid;
    pthread_attr_t attr; 
    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, potok, 0);

    printf("Слушаем на %i\r\n", port);    
    start_server("127.0.0.1", port);

    pthread_join(tid,NULL);
    return 0;
}