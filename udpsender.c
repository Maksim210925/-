#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/udp.h>	
#include <netinet/ip.h>	
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

char msg1[] = "123456789-Hello there!\n";
char msg2[] = "Bye bye!\n";

int main()
{
    char buf[1024];
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
    {
        perror("socket");
        exit(1);
    }

    for (;;) {
        addr.sin_family = AF_INET;
	addr.sin_port = htons(1024);
	addr.sin_addr.s_addr = inet_addr("192.168.88.100");
		printf("Send data: %s", msg1);
	sendto(sock, msg1, sizeof(msg1), 0, (struct sockaddr *)&addr, sizeof(addr));
    
	//int slen;
	//if (recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&addr, &slen) == -1)
	//{
	//    continue;
	//}
	//printf("UDP recieve: %s", buf);
    }
    close(sock);

    return 0;
}