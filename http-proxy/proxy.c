// https://github.com/vmsandeeprao/HTTP-proxy-server/blob/master/proxy.c

// #include "proxy_parse.h"

#include <asm-generic/errno-base.h>
// #include <cstddef>
#include <signal.h>
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

#define BUFFER_SIZE (1 << 14)

// const int BUFFER_SIZE = 1 << 14;

const int maximum_process_count = 1 << 7;

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

/* 透过域名打开一个通向它的socket连接 */
int open_proxyfd(char *hostname, int port)
{
	int fd;
	struct addrinfo *addrlist, *p;
	char port_str[BUFFER_SIZE];

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("\033[031m域名转换模块：创建socket对象失败\033[0m");
		return -1;
	}

	sprintf(port_str, "%d", port);

	if (getaddrinfo(hostname, port_str, NULL, &addrlist) != 0)
	{
		perror("\033[031m域名转换模块：获取域名信息失败\033[0m");
		return -1;
	}

	for (p = addrlist; p; p = p->ai_next)
	{
		if (p->ai_family == AF_INET)
		{
			if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
				break;
		}
	}
	freeaddrinfo(addrlist);
	if (!p)
	{
		close(fd);
		perror("\033[031m域名转换模块：地址表中没有一个可以连接的地址\033[0m");
		return -1;
	}
	else
		return fd;
}

void parse_uri(const char raw_uri[], char protocol[], char host[], char path[], int *port)
{
	char ato[BUFFER_SIZE];

	*port = 80;	 // 默认设80
	path = "\0"; // 默认设空
	protocol = "\0";

	puts("BP1");
	puts(raw_uri);
	int match_cnt = sscanf(raw_uri, "%[htps]://%s", protocol, ato);
	printf("matchcnt:%d\n", match_cnt);

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
	puts("AA@");
	match_cnt = sscanf(ato, "%[^/:?]:%d%s", host, port, path);
	puts("FDAS");
	// 没有端口号
	if (match_cnt == 1)
	{
		sscanf(ato, "%*[^/:?]%s", path);
	}
}

void wrap_error(int fd, int code, const char *msg)
{
	printf("包装error：%d %s\n", code, msg);
	char headers[BUFFER_SIZE] = "\0";
	char body[BUFFER_SIZE] = "\0";

	sprintf(body, "<!DOCTYPE html><title>PROXY ERROR</title>"
				  "<body>"
				  "<h>%d %s</h>"
				  "</body>",
			code, msg);

	sprintf(headers, "HTTP/1.0 %d %s\r\n"
					 "Content-type: text/html\r\n"
					 "Content-length: %d\r\n\r\n",
			code, msg, strlen(body));
	rio_write_n(fd, headers, strlen(headers));
	rio_write_n(fd, body, strlen(body));
}

#include <sys/select.h>

void tunnel_transfer(int fromfd, int tofd)
{
	char buffer[BUFFER_SIZE];

	fd_set rset, eset;
	struct timeval TV;
	TV.tv_sec = 60;

	FD_ZERO(&rset);
	// FD_ZERO(&wset);
	FD_ZERO(&eset);
	FD_SET(fromfd, &rset);
	FD_SET(fromfd, &eset);
	FD_SET(tofd, &rset);
	FD_SET(tofd, &eset);

	int readyfd;

	while ((readyfd = select(2 + 1, &rset, NULL, &eset, &TV)) != 0)
	{
		if (readyfd < 0)
		{
			perror("\033[31mCONNECT隧道模块：select等待错误\033[0m");
			break;
		}
		else
		{
			int otherfd = (readyfd == fromfd ? tofd : fromfd);
			rio_read_n(readyfd, buffer, BUFFER_SIZE - 1);
			printf("\033[35m%d => %d\n", readyfd, otherfd);
			puts(buffer);
			printf("\033[0m\n");
			rio_write_n(otherfd, buffer, BUFFER_SIZE - 1);
		}
	}
	perror("\033[31mCONNECT隧道模块：select超时\033[0m");
}

void handle_inbound(int client_fd, int *serverfd)
{
	char buf[BUFFER_SIZE];
	char method[BUFFER_SIZE];
	char uri[BUFFER_SIZE];
	char version[BUFFER_SIZE];

	rio R;
	ssize_t len = 0;
	int resplen = 0;

	rio_init(&R, client_fd);
	rio_buffered_readline(&R, buf, BUFFER_SIZE);

	puts(buf);

	sscanf(buf, "%s %s %s", method, uri, version);

	printf("method:%s\n", method);
	printf("uri:%s\n", uri);
	printf("version:%s\n", version);

	char host[BUFFER_SIZE];
	char protocol[BUFFER_SIZE];
	char path[BUFFER_SIZE];
	int port;
	parse_uri(uri, protocol, host, path, &port);

	printf("HOST:%s\n", host);
	printf("PATH:%s\n", path);
	printf("PROTOCOL:%s\n", protocol);
	printf("PORT:%d\n", port);

	*serverfd = open_proxyfd(host, port);
	printf("serverfd:%d\n", *serverfd);
	if (strcasecmp(method, "CONNECT") == 0)
	{
		// read(client_fd, buf, BUFFER_SIZE);
		// puts(buf);
		sprintf(buf, "%s 200 Connection Established\r\n\r\n", version);
		puts(buf);

		rio_write_n(client_fd, buf, strlen(buf));
		tunnel_transfer(client_fd, *serverfd);
	}
	else
	{
		wrap_error(client_fd, 501, "Not Implemented Error");
	}
}

int main(int argc, char *argv[])
{
	printf("\033[32m垃圾代理：版本0.1.0\033[0m\n\n");
	int sockfd, newfd;
	struct sockaddr_in from, to;
	int sin_size;
	// puts("P1");
	sem_t *available_conn = sem_open("/available_semaphore", O_CREAT, 0666, maximum_process_count);
	sem_init(available_conn, 1, maximum_process_count);

	int tmp;
	sem_getvalue(available_conn, &tmp);

	printf("允许同时处理的进程数：%d\n", tmp);

	// puts("P2");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		perror("\033[31m创建socket失败\033[0m");
		exit(1);
	}

	// puts("P3");
	from.sin_family = AF_INET;
	from.sin_port = htons(bind_port);
	from.sin_addr.s_addr = htonl(INADDR_ANY);

	bzero(&(from.sin_zero), 8);
	// puts("P4");
	if (bind(sockfd, (struct sockaddr *)&from, sizeof(struct sockaddr)) < 0)
	{
		perror("\033[31m端口绑定失败\033[0m");
		exit(2);
	}
	// puts("P5");
	listen(sockfd, 0);
	// puts("P6");
	printf("监听%d中\n", bind_port);
	// puts("P7");

	int T = 1919810;

	while (T--)
	{
		sin_size = sizeof(struct sockaddr_in);
		socklen_t siz;
		newfd = accept(sockfd, (struct sockaddr *)&to, &siz);
		printf("新连接传入：%d\n", newfd);
		int serverfd;

		if (newfd == -1)
		{
			perror("接收错误");
		}
		else
		{
			pid_t p = fork();
			if (p == 0)
			{
				// printf("进程号：%d\n", p);
				if (sem_trywait(available_conn) != 0)
				{
					// puts("A1");
					perror("\033[31m并发数已达上限\033[0m");
					printf("\033[31m并发数已达上限\033[0m\n");
					wrap_error(newfd, 114514, "并发数已达上限");
					close(newfd);
				}
				else
				{
					// puts("A2");
					int pctr;
					sem_getvalue(available_conn, &pctr);
					// printf("\033[32m 已经使用%d个进程 \033[0m \n", maximum_process_count - pctr);
					fprintf(stderr, "\033[32m 已经使用%d个进程 \033[0m \n", maximum_process_count - pctr);
					handle_inbound(newfd, &serverfd);
					sem_post(available_conn);

					close(newfd);
					close(serverfd);
				}
				break;
			}
		}
	}

	return 0;
}
