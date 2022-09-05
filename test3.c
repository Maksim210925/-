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
#include <pthread.h>
#include <ev.h>
#include<errno.h> 
#include <netinet/udp.h>	
#include <netinet/ip.h>	
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

/* Сокет отправки днных клиенту*/
int ssock;
/* IP интерфейса с которого отправляются модифицированные данные клиениту */
uint32_t out_ip;  
/* IP интерфейса приема данных от клиента */
uint32_t in_ip;
/* Порт на который отправляются данные клиенту */
uint16_t outport = 1026;
/* Порт сервича переворота строки  */
const unsigned short service_port = 1025; 


/* message length limitation */
#define MAX_MESSAGE_LEN (256)

#define err_message(msg) \
    do {perror(msg); exit(EXIT_FAILURE);} while(0)

/* UDP заголовок - RFC768 */
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



/*
	Generic checksum calculation function
*/
unsigned short InetCSum(unsigned short *ptr,int nbytes) 
{
	long sum;
	unsigned short oddbyte;
	short answer;

	sum=0;
	while(nbytes>1) {
		sum+=*ptr++;
		nbytes-=2;
	}
	if(nbytes==1) {
		oddbyte=0;
		*((unsigned char*)&oddbyte)=*(unsigned char*)ptr;
		sum+=oddbyte;
	}

	sum = (sum>>16)+(sum & 0xffff);
	sum = sum + (sum>>16);
	answer=(short)~sum;
	
	return(answer);
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


struct IPv4PseudoHeader
{
	uint32_t source_address;
	uint32_t dest_address;
	uint8_t placeholder;
	uint8_t protocol;
	uint16_t udp_length;
} ;

/*
 * Для L3 уровня
 *    

int RawSendTo(int raw_sockfd, const void *buf, size_t len, const struct sockaddr_in *dest_addr, socklen_t daddrlen)
{
	int rs = 0;
	struct sockaddr_in addr;
	socklen_t addrlen;
	getsockname(raw_sockfd, (struct sockaddr*)&addr, &addrlen);

	size_t bufln = len + sizeof(struct udphdr);
	char *buf_ = (char *)malloc(bufln);
	memset(buf_, 0, bufln);
	memcpy(buf_ + sizeof(struct udphdr), buf, len);
	struct udphdr *udph = (struct udphdr *)buf_;
	udph->source = ((struct sockaddr_in)addr).sin_port;
	udph->dest = dest_addr->sin_port;
	udph->check = 0;
	udph->len = htons(bufln);

	IPv4PseudoHeader psh;
	psh.source_address =((struct sockaddr_in*)&addr)->sin_addr.s_addr;
	psh.dest_address =	dest_addr->sin_addr.s_addr;
	psh.placeholder = 0;
	psh.protocol = IPPROTO_UDP;
	psh.udp_length = udph->len;
	int psize = sizeof(IPv4PseudoHeader) + bufln;
	char * pseudogram = (char *)malloc(psize);
	memcpy(pseudogram, (char *)&psh, sizeof(IPv4PseudoHeader));
	memcpy(pseudogram + sizeof(IPv4PseudoHeader), buf_, bufln);
	udph->check = InetCSum((unsigned short*)pseudogram, psize);
	rs = sendto(raw_sockfd, buf_, bufln, 0, (struct sockaddr *)dest_addr, daddrlen);
	free(pseudogram);
	free(buf_);
	return rs;
}

*/

void RawSendTo(int ssock, char *data, unsigned short ret, sockaddr_in *dst_addr, socklen_t daddrlen)
{
    struct IPv4PseudoHeader psh;
    char *pseudogram;
    char buf[MAX_MESSAGE_LEN + sizeof(struct udphdr) + sizeof(struct iphdr)] = {0};
    
    struct sockaddr_in saddr;
	socklen_t saddrlen;
	getsockname(ssock, (struct sockaddr*)&saddr, &saddrlen);


   //IP header
    struct iphdr *iph = (struct iphdr *)buf;    
    //UDP Header
    struct udphdr *udph = (struct udphdr *)(buf + sizeof(struct iphdr));
    char   *UdpPayload = (char *)udph + sizeof(struct udphdr);
 
    memset(buf, 0, sizeof(buf));

            uint16_t udplength = sizeof(udph) + ret;
            uint16_t iplength = sizeof(struct iphdr) + udplength;
            
            // Формирование IP заголовка 
        	iph->ihl = 5; //sizeof(struct iphdr) >> 2;
	        iph->version = 4;
	        iph->tos = 0;
	        iph->tot_len = iplength;
	        iph->id = 0;
	        iph->frag_off = 0;
	        iph->ttl = 255;
	        iph->protocol = IPPROTO_UDP;
	        iph->check = 0;
	        iph->saddr = saddr.sin_addr.s_addr;
	        iph->daddr = dst_addr->sin_addr.s_addr;
        
            //CRC
	        iph->check = InetCSum((unsigned short *)iph, sizeof(struct iphdr));

        	//Формирование UDP заголовка     
	        udph->dest = htons(outport);;
        	udph->source = saddr.sin_port;
	        udph->len = htons(udplength);
	        udph->check = 0;
        	memcpy(UdpPayload, data, ret);

        	// UDP checksumm 
        	unsigned short datalen = ret + (ret % 2);               // Размер данных при подсчете суммы должен быть кратным 2
	        psh.source_address = iph->saddr;
	        psh.dest_address = iph->daddr;
	        psh.placeholder = 0;
	        psh.protocol = IPPROTO_UDP;
	        psh.udp_length = htons(udplength);
	
	        int psize = sizeof(struct IPv4PseudoHeader) + sizeof(struct udphdr) + datalen;
	        pseudogram = (char *)malloc(psize);
	
	        memcpy(pseudogram , (char*) &psh , sizeof (struct IPv4PseudoHeader));
	        memcpy(pseudogram + sizeof(struct IPv4PseudoHeader) , udph , sizeof(struct udphdr) + datalen);
	        udph->check = InetCSum( (unsigned short*) pseudogram , psize);
            free(pseudogram);
            // Отправка 
            saddr.sin_family = AF_INET;
            saddr.sin_port = htons(outport);
            saddr.sin_addr.s_addr = iph->daddr;
        	sendto(ssock, buf, iplength, MSG_DONTWAIT,(struct sockaddr *)&saddr, sizeof(saddr));
}


/* Поток */
static void read_cb(struct ev_loop *loop, ev_io *watcher, int revents)
{
    //ev_io *client;
    socklen_t slen;
    size_t ret;
    char buf[MAX_MESSAGE_LEN + sizeof(struct udphdr) + sizeof(struct iphdr)] = {0};
    char buf2[MAX_MESSAGE_LEN] = {0};
    struct sockaddr_in saddr;
    struct sockaddr_in cl;

     //IP header
    //struct iphdr *iph = (struct iphdr *)buf;    
    //UDP Header
    struct udphdr *udph = (struct udphdr *)(buf + sizeof(struct iphdr));
    char   *UdpPayload = (char *)udph + sizeof(struct udphdr);
 
    /* Прием RAW UDP */ 
    ret = recvfrom(watcher->fd, buf, sizeof(buf), MSG_DONTWAIT,(struct sockaddr *)&saddr, &slen);    
    if (ret > 0) 
    {
         printf("Получен RAW UDP from %2x:%hu. Data %s\r\n", ntohl(saddr.sin_addr.s_addr), ntohs(saddr.sin_port), UdpPayload );

      	/*
		Отправка данных на модификацию в другой поток
		Сокет блокирующий
	    */
	    int fd = socket(AF_INET, SOCK_STREAM, 0);
	    if (fd < 0) err_message("socket err\n");
	    cl.sin_family = AF_INET;
	    cl.sin_port = htons(service_port);
	    inet_pton(AF_INET, "127.0.0.1", &cl.sin_addr);
	    int flg;
	    fcntl(fd, F_GETFL, &flg);
	    flg &= ~O_NONBLOCK;
	    fcntl(fd, F_SETFL, flg);
	    if (connect(fd, (struct sockaddr *)&cl, sizeof(struct sockaddr_in))==0)
	    {
    		write(fd, UdpPayload, ntohs(udph->len));
	        ret = recv(fd, buf2, sizeof(buf2) , 0);    
	        if (ret > 0) 
	        {
            	    saddr.sin_port = htons(outport);//udph->source;
            	    saddr.sin_addr.s_addr = out_ip;//iph->saddr;
            	    printf("Отправка RAW UDP to %2x:%hu. Data: %s\r\n", ntohl( saddr.sin_addr.s_addr), ntohs(saddr.sin_port), buf2 );
            	    RawSendTo(ssock, buf2, ret, &saddr, slen);
	        }
	    }
    close(fd);
    }
    else if ((ret < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;

    } else {
        ev_io_stop(loop, watcher);
        close(watcher->fd);
        free(watcher);
    }
}



static void start_server(char *ifstr, unsigned short port)
{
    struct ifreq ifr;
    int fd;
    struct ev_loop *loop = ev_loop_new(EVFLAG_AUTO | EVFLAG_NOENV);
    ev_io watcher;
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (fd < 0) err_message("socket err\n");

    memset(&ifr, 0, sizeof(struct ifreq));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), ifstr);
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0)
    {
        perror("Server-setsockopt() error for SO_BINDTODEVICE");
        printf("%s\n", strerror(errno));
        exit(-1); }
    if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
        printf("%s\n", strerror(errno));
        exit(-1); }
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    in_ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    server.sin_addr.s_addr= in_ip;
    printf("service listen iface:  %s\r\n", ifstr);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) err_message("bind err\n");
    ev_io_init(&watcher, read_cb, fd, EV_READ);
    ev_io_start(loop, &watcher);
    for (;;) {
        ev_run(loop, 0);
    }

    ev_loop_destroy(loop);
	return;
}

void * potok(void *v)
{
    mstart_server("127.0.0.1", service_port);
	return 0;
}

int main(int argc, char **argv)
{
    struct ifreq ifr;
    struct sockaddr_in server;

    if (argc == 1)  {
	printf("тестовое задание 3: автор Босенко Максим Александрович 04.09.2022\r\n");
        printf("Параметры командной строки: main eth0  eth1\r\n\teth0 - имя входящего интиерфейса,\r\n\t eth1 - имя исходящего интерфейса\r\n\tПрием данных по порту  1024, отправка 1026\\r\n");
        exit(0);
    }
    if (argc != 3)  exit(-1);

    /* Сокет для отправки данных клиенту 
	    - привязка к интерфейсу
        - не блокирующий
	*/
    ssock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (ssock < 0) {
        err_message("socket err\n");
        exit(-1);
    }

    if (fcntl(ssock, F_SETFL, O_NONBLOCK)== -1) {
        printf("%s\n", strerror(errno));
        exit(-1); }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), argv[2]);
    if (setsockopt(ssock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0)
    {
        perror("Server-setsockopt() error for SO_BINDTODEVICE");
        printf("%s\n", strerror(errno));
        exit(-1); }
    if (ioctl(ssock, SIOCGIFADDR, &ifr) == -1) {
        printf("%s\n", strerror(errno));
        exit(-1); }

    server.sin_family = AF_INET;
    server.sin_port = htons(service_port);
    out_ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    server.sin_addr.s_addr= out_ip;
    printf("service out iface:  %s\r\n", argv[2]);
    fcntl(ssock, F_SETFL, O_NONBLOCK);
    if (bind(ssock, (struct sockaddr *)&server, sizeof(server)) < 0) { 
	err_message("bind err\n");
	exit(-1);
    }
    //-----------------------------------------------------------------------------------------

    pthread_t tid;
    pthread_attr_t attr; 
    pthread_attr_init(&attr);

    /* Поток модификации строки */
    pthread_create(&tid, &attr, potok, 0);

    /* Поток приема запросов */
    start_server(argv[1], service_port);

    pthread_join(tid, NULL);
    return 0;
}