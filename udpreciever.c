/* @author $username$ <$usermail$>
 * @date $date$
 *
 * Simplest UDP echo server*/

#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include "nx_socket.h"
#include <netinet/udp.h>	
#include <netinet/ip.h>	
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int on = 1, off = 0;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) exit(-1);
	struct sockaddr_in self;
	memset(&self, 0, sizeof(self));
	self.sin_family = AF_INET;
	self.sin_addr.s_addr = inet_addr("192.168.88.101");
	self.sin_port = htons(1026);
	if (bind(sock, (struct sockaddr*)&self, sizeof(self)) < 0) exit(-1);
	char buf[0x10000];
	memset(buf, 0, sizeof(buf));
	while(1)
	{
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		socklen_t addrln = sizeof(addr);
		ssize_t rs = recvfrom(sock, buf, sizeof(buf), 0,
				(struct sockaddr*)&addr, &addrln);
		if(rs == -1)
			printf("Error calling recvfrom");
		else printf("%s", buf);
		fflush(stdout);
	}
	return 0;
}

