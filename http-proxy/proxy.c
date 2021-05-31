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

#include <netdb.h>
// #include <sys/sem.h>

#include <semaphore.h>
#include <fcntl.h>

// #define BUFFER_SIZE 1<<15

const int BUFFER_SIZE = 1 << 16;

const int maximum_process_count = 1 << 2;

int available;

void handle_inbound(int socket_fd)
{
	char buf[BUFFER_SIZE];
	int ctr = read(socket_fd, buf, BUFFER_SIZE);
	buf[ctr] = '\0';
	puts(buf);
}

int main(int argc, char *argv[])
{
	int sockfd, newfd;
	struct sockaddr_in from, to;
	int sin_size;

	sem_t *available_conn = sem_open("/available_semaphore", O_CREAT, 0666, maximum_process_count);

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
		perror("\033[31m端口绑定失败\033[0m");
		exit(2);
	}

	listen(sockfd, 1919810);
	puts("监听11451中");

	int T = 1919810;

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
			pid_t p = fork();
			if (p == 0)
			{
				if (sem_trywait(available_conn) != 0)
				{
					perror("\033[31m并发数已达上限\033[0m");
					break;
				}
				else
				{
					int pctr;
					sem_getvalue(available_conn, &pctr);
					fprintf(stderr, "\033[32m 已经使用%d个进程 \033[0m \n", maximum_process_count-pctr);
					handle_inbound(newfd);
					sem_post(available_conn);
				}
				break;
			}
		}
	}

	return 0;
}
