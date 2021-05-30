#include "proxy_parse.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

// #define BUFFER_SIZE 1<<15

const int BUFFER_SIZE = 1 << 15;



int main(int argc, char *argv[])
{
	int sockfd, newfd;
	struct sockaddr_in from, to;
	int sin_size;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		perror("创建socket失败");
		exit(1);
	}

	from.sin_family = AF_INET;
	from.sin_port = htons(11451);
	from.sin_addr.s_addr = htonl(INADDR_ANY);

	bzero(&(from.sin_zero), 8);
	if (bind(sockfd, (struct sockaddr *)&from, sizeof(struct sockaddr)) < 0)
	{
		perror("端口绑定失败");
		exit(2);
	}

	listen(sockfd, 1919);
	puts("监听11451中");

	int T = 10;

	while (T--)
	{
		sin_size = sizeof(struct sockaddr_in);
		socklen_t siz;
		newfd = accept(sockfd, (struct sockaddr *)&to, &siz);

		if (newfd == -1)
		{
			perror("接收错误");
		}
		else
		{
			char buf[BUFFER_SIZE];
			int ctr = read(newfd, buf, BUFFER_SIZE);
			buf[ctr] = '\0';
			puts(buf);
		}
	}

	return 0;
}
