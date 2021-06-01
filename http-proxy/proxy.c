#include "proxy_parse.h"

#include <asm-generic/errno-base.h>
// #include <cstddef>
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

const int BUFFER_SIZE = 1 << 14;

const int maximum_process_count = 1 << 2;

const int bind_port = 11451;

// package rio begin

typedef struct
{
	int fd;
	int cnt;
	char *bufptr;
	char buf[BUFFER_SIZE];
} rio;

/* 初始化鲁棒io结构体 */
void rio_init(rio *R, int fd)
{
	R->fd = fd;
	R->cnt = 0;
	R->bufptr = R->buf;
}

/* 从fd中连续读入n个字节 */
ssize_t rio_read_n(int fd, void *buf, size_t n)
{
	ssize_t nread;
	size_t nleft = n;
	char *bp = (char *)buf;
	while (nleft > 0)
	{
		if ((nread = read(fd, bp, nleft)) < 0)
		{
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		}
		else if (nread == 0)
			break;
		nleft -= nread;
		bp += nread;
	}
	return n - nleft;
}

/* 向fd中连续写入n个字节 */
ssize_t rio_write_n(int fd, void *buf, size_t n)
{
	ssize_t nwrite;
	size_t nleft = n;
	char *bp = (char *)buf;
	while (nleft > 0)
	{
		if ((nwrite = write(fd, bp, nleft)) <= 0)
		{
			if (errno == EINTR)
				nwrite = 0;
			else
				return -1;
		}
		nleft -= nwrite;
		bp += nwrite;
	}
	return n;
}

/* 利用rio结构缓冲，减少调用read次数的read实现 */
ssize_t rio_read(rio *rp, void *buf, size_t n)
{
	ssize_t cnt;
	while (rp->cnt <= 0)
	{
		rp->cnt = read(rp->fd, rp->buf, sizeof(rp->buf));
		if (rp->cnt < 0)
		{
			if (errno != EINTR)
				return -1;
		}
		else if (rp->cnt == 0)
			return 0;
		else
			rp->bufptr = rp->buf;
	}
	cnt = n;
	if (rp->cnt < n)
		cnt = rp->cnt;
	memcpy(buf, rp->bufptr, cnt);
	rp->cnt -= cnt;
	rp->bufptr += cnt;
	return cnt;
}

/* 从fd中读入行 */
ssize_t rio_buffered_readline(rio *rp, void *buf, size_t n)
{
	size_t i, ret;
	char c;
	char *bp = (char *)buf;
	// 为'\0'预留位置
	for (i = 1; i < n; ++i)
	{
		if ((ret = rio_read(rp, &c, 1)) == 1)
		{
			*bp++ = c;
			if (c == '\n')
				break;
		}
		else if (ret == 0)
		{
			if (i == 1)
				return 0;
			else
				break;
		}
		else
			return -1;
	}
	*bp = '\0';
	return i;
}

ssize_t rio_buffered_read_n(rio *rp, void *buf, size_t n)
{
	ssize_t nread;
	size_t nleft = n;
	char *bp = (char *)buf;
	while (nleft > 0)
	{
		if ((nread = rio_read(rp, bp, nleft)) < 0)
		{
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		}
		else if (nread == 0)
			break;
		nleft -= nread;
		bp += nread;
	}
	return n - nleft;
}

// package rio end



void parse_uri(const char raw_uri[], char protocol[], char host[], char path[], int *port)
{
	char ato[BUFFER_SIZE];

	*port = 80;	 // 默认设80
	path = "\0"; // 默认设空
	protocol = "\0";

	int match_cnt = sscanf(raw_uri, "%[htps]://%s", protocol, ato);

	// 没有http(s)前缀
	if (match_cnt == 0)
	{
		sscanf(raw_uri, "%s", ato);
	}
	else
	{
		if (strcmp(protocol, "http") == 0)
			*port = 80;
		else if (strcmp(protocol, "https") == 0)
			*port = 443;
	}

	match_cnt = sscanf(ato, "%[^/:?]:%d%s", host, port, path);

	// 没有端口号
	if (match_cnt == 1)
	{
		sscanf(ato, "%*[^/:?]%s", path);
	}
}

void handle_inbound(int socket_fd)
{
	char buf[BUFFER_SIZE];
	char method[BUFFER_SIZE];
	char uri[BUFFER_SIZE];
	char version[BUFFER_SIZE];

	rio R;
	ssize_t len = 0;
	int resplen = 0;

	rio_init(&R, socket_fd);
	rio_buffered_readline(&R, buf, BUFFER_SIZE);

	sscanf(buf, "%s %s %s", method, uri, version);

	char host[BUFFER_SIZE];
	char protocol[BUFFER_SIZE];
	char path[BUFFER_SIZE];
	int port;
	parse_uri(uri, protocol, host, path, &port);

	struct hostent *H = gethostbyname(host);
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
	from.sin_port = htons(bind_port);
	from.sin_addr.s_addr = htonl(INADDR_ANY);

	bzero(&(from.sin_zero), 8);
	if (bind(sockfd, (struct sockaddr *)&from, sizeof(struct sockaddr)) < 0)
	{
		perror("\033[31m端口绑定失败\033[0m");
		exit(2);
	}

	listen(sockfd, 1919810);
	printf("监听%d中", bind_port);

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
					fprintf(stderr, "\033[32m 已经使用%d个进程 \033[0m \n", maximum_process_count - pctr);
					handle_inbound(newfd);
					sem_post(available_conn);
				}
				break;
			}
		}
	}

	return 0;
}
