// 编译命令: gcc -pthread

#include <asm-generic/socket.h>
typedef long long LL;

#define sign(_x) (_x < 0)
#define range_4(__iter__, __from__, __to__, __step__) for (LL __iter__ = __from__; __iter__ != __to__ && sign(__to__ - __from__) == sign(__step__); __iter__ += __step__)
#define range_3(__iter__, __from__, __to__) range_4(__iter__, __from__, __to__, 1)
#define range_2(__iter__, __to__) range_4(__iter__, 0, __to__, 1)
#define range_1(__iter__, __to__) range_4(__iter__, 0, 1, 1)
#define get_range(_1, _2, _3, _4, _Func, ...) _Func
#define range(...) get_range(__VA_ARGS__, range_4, range_3, range_2, range_1, ...)(__VA_ARGS__)

#define min(_lhs, _rhs) (_lhs < _rhs ? _lhs : _rhs)
#define endl '\n'

#include <asm-generic/errno-base.h>
#include <pthread.h>
#include <stddef.h>
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

u_short bind_port = 11452;

//package vector begin　给用C++就没这些破事了害

typedef char vector_element;
typedef struct
{
	vector_element *begin, *end;
	size_t real_size;
} vector_t;

size_t vector_size(vector_t *V) { return V->end - V->begin; }

void vector_migrate(vector_t *V)
{
	V->real_size <<= 1;
	vector_element *tmp = (vector_element *)malloc(sizeof(vector_element) * V->real_size);
	size_t siz = vector_size(V);
	memcpy(tmp, V->begin, sizeof(vector_element) * siz);
	free(V->begin);
	V->begin = tmp;
	V->end = V->begin + siz;
}

void vector_resize(vector_t *V, size_t newsize)
{
	V->real_size = newsize;
	vector_element *tmp = (vector_element *)malloc(sizeof(vector_element) * V->real_size);
	size_t siz = min(newsize, vector_size(V));
	memcpy(tmp, V->begin, sizeof(vector_element) * siz);
	free(V->begin);
	V->begin = tmp;
	V->end = V->begin + siz;
}

void vector_concat_n(vector_t *V, vector_element *Es, size_t n)
{
	if (vector_size(V) + n > V->real_size)
		vector_resize(V, vector_size(V) + n << 1);
	while (n--)
	{
		*V->end = *Es;
		Es++;
		V->end++;
	}
}

void vector_emplace_back(vector_t *V, vector_element *E)
{
	size_t siz = vector_size(V);
	if (siz + 1 >= V->real_size)
		vector_migrate(V);
	*V->end = *E;
	V->end++;
}

void vector_concat(vector_t *V, vector_element *Es)
{
	while (*Es)
	{
		vector_emplace_back(V, Es);
		Es++;
	}
}
void vector_init(vector_t *V, size_t siz)
{
	V->real_size = siz;
	V->begin = (vector_element *)malloc(sizeof(vector_element) * V->real_size);
	V->end = V->begin;
}

void vector_clear(vector_t *V) { V->end = V->begin; }

void vector_destroy(vector_t *V) { free(V->begin); }

void vector_connect(vector_t *original, vector_t *another)
{
	if (original->real_size <
		vector_size(original) + vector_size(another))
	{
		vector_resize(original, vector_size(original) + vector_size(another));
	}
	for (vector_element *p = another->begin; p != another->end; p++)
	{
		*original->end = *p;
		original->end++;
	}
}

//package vector end

/* SB Rio，狗都不用 */
ssize_t readline(int fd, char *buf)
{
	char *ptr = buf;
	int read_ctr = 0;
	do
	{
		int RCTR = read(fd, ptr, 1);
		if (RCTR == 0) // EOF
			break;
		else if (RCTR == -1) // ERROR
		{
			perror("\033[031m读异常:\033[0m");
			break;
		}
		read_ctr += RCTR;
	} while (*ptr++ != '\n');
	*ptr = '\0';
	return read_ctr;
}

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
		fprintf(stderr, "\033[031m其中域名是%s\n\033[0m", hostname);
		return -1;
	}

	for (p = addrlist; p; p = p->ai_next)
	{
		if (p->ai_family == AF_INET)
		{
			// range(i, 14)
			// {
			// 	printf("尝试连接于%d\n", (p->ai_addr->sa_data[i]));
			// }
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

int parse_uri(const char raw_uri[], char protocol[], char host[], char path[], int *port)
{
	char ato[BUFFER_SIZE];
	// puts("A1");
	*path = '\0';

	*protocol = '\0';

	*port = 80; // 默认设80
	// puts("A2");

	int match_cnt = sscanf(raw_uri, "%[^:/]://%s", protocol, ato);
	// puts("A3");

	// 没有http(s)前缀
	// int match_cnt = 1;
	// fprintf(stderr, "\033[035mmatch_cnt是%d\n\033[0m", match_cnt);
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
		else
		{
			*protocol = '\0';
			sscanf(raw_uri, "%s", ato);
		}
	}
	// fprintf(stderr, "\033[035mato是%s\n\033[0m", ato);
	match_cnt = sscanf(ato, "%[^/:?]:%d%s", host, port, path);
	// fprintf(stderr, "\033[035m域名是%s\n\033[0m", host);
	// 没有端口号
	printf("matchcnt:%d\n", match_cnt);
	if (match_cnt == 1)
	{
		sscanf(ato, "%*[^/:?]%s", path);
	}
	else if (match_cnt == 0)
	{
		host[0] = '\0';
		return -1;
	}
	return 0;
	// puts("A4");
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
	write(fd, headers, strlen(headers));
	write(fd, body, strlen(body));
}

#include <sys/select.h>

struct ft_t
{
	int fromfd, tofd, enable_print;
};

void *endless_piping(void *arg)
{
	struct ft_t *ARG = (struct ft_t *)arg;
	// printf("进入线程: 入:%d 出:%d\n", ARG->fromfd, ARG->tofd);
	char buffer[BUFFER_SIZE];
	int readcnt;

	while ((readcnt = read(ARG->fromfd, buffer, BUFFER_SIZE)) > 0)
	{
		if (ARG->enable_print)
		{
			printf("从%d处的读入数%d\n", ARG->fromfd, readcnt);
			puts(buffer);
		}
		// range(i, readcnt)
		// {
		// 	printf("%X\t", buffer[i]);
		// }
		// puts("");
		write(ARG->tofd, buffer, readcnt);
	}
}

void tunnel_transfer(int fromfd, int tofd, int enable_print)
{
	char buffer[BUFFER_SIZE];

	struct timeval TV;
	TV.tv_sec = 10;
	// setsockopt(fromfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&TV, sizeof TV);
	// setsockopt(tofd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&TV, sizeof TV);

	puts("进入隧道模式");
	pthread_t rw[2];
	struct ft_t f1, f2;
	f2.tofd = f1.fromfd = fromfd;
	f2.fromfd = f1.tofd = tofd;
	f1.enable_print = f2.enable_print = enable_print;

	pthread_create(&rw[0], NULL, &endless_piping, (void *)&f1);
	pthread_create(&rw[1], NULL, &endless_piping, (void *)&f2);

	pthread_join(rw[0], NULL);
	pthread_join(rw[1], NULL);

	perror("\033[31mCONNECT隧道模块：超时关闭\033[0m");
}

int SetNonBlock(int iSock)
{
	int iFlags;

	iFlags = fcntl(iSock, F_GETFL, 0);
	iFlags |= O_NONBLOCK;
	iFlags |= O_NDELAY;
	int ret = fcntl(iSock, F_SETFL, iFlags);
	return ret;
}

int construct_package(vector_t *V, int fd)
{
	char buf[BUFFER_SIZE];
	int reuse = 0;
	long long content_length = -1;
	int chunked_flag = 0;

	while (1)
	{
		// int rctr = rio_buffered_readline(R, buf, BUFFER_SIZE);
		int rctr = readline(fd, buf);
		// printf("读入%d个字节:\n%s", rctr, buf);
		if (strcmp(buf, "\r\n") == 0 || rctr == 0)
			break;
		// puts(buf);
		char key[BUFFER_SIZE], value[BUFFER_SIZE];

		char *strstrres = strstr(buf, ": ");

		if (strstrres == NULL)
		{
			vector_concat(V, buf);
			continue;
		}

		int sep = strstrres - buf;
		strncpy(key, buf, sep);
		key[sep] = '\0';
		strcpy(value, buf + sep + 2);

		// printf("KEYLEN %d\n", strlen(key));
		// printf("KEY:%s\n", key);

		if (strcasecmp(key, "Proxy-Connection") == 0)
		{
			if (strcasecmp(value, "keep-alive\r\n") == 0)
				reuse = 1;
			// sprintf(buf, "Connection: close\r\n");
			// vector_concat(&VECTOR, buf);
			// wctr = write(*serverfd, buf, strlen(buf));
		}
		else if (strcasecmp(key, "Connection") == 0)
		{
			if (strcasecmp(value, "keep-alive\r\n") == 0)
				reuse = 1;
			// sprintf(buf, "Connection: close\r\n");
			// wctr = write(*serverfd, buf, rctr);
		}
		else if (strcasecmp(key, "content-length") == 0)
		{
			content_length = atoll(value);
			printf("Got content_length=%lld\n", content_length);
		}
		else if (strcasecmp(key, "Transfer-Encoding") == 0)
		{
			if (strstr(value, "chunked") != NULL)
				chunked_flag = 1;
		}
		vector_concat(V, buf);
		// printf("写入%d个字节:\n%s", wctr, buf);
	}
	vector_concat(V, "\r\n");
	int rp;
	// puts("BP2");
	while (content_length > 0)
	{
		printf("Content-Length ===> %lld\n", content_length);
		// rp = rio_buffered_readline(R, buf, min(BUFFER_SIZE - 1, content_length));
		rp = read(fd, buf, min(BUFFER_SIZE - 1, content_length));
		if (rp == 0)
		{
			perror("\033[031m读错误：过早的EOF\033[0m");
			break;
		}
		content_length -= rp;
		printf("I read %d \n", rp);
		vector_concat_n(V, buf, rp);
	}

	if (chunked_flag)
	{
		long long len;
		while (1)
		{
			int rdc = readline(fd, buf);
			vector_concat_n(V, buf, rdc);
			buf[rdc] = '\0';
			sscanf(buf, "%llx", &len);
			// len = atoll(buf);
			printf("\t<<rdc>>:%d <<len>>:%lld\n", rdc, len);
			puts(buf);

			if (len == 0)
			{
				rdc = readline(fd, buf);
				vector_concat_n(V, buf, rdc);
				break;
			}
			while (len > BUFFER_SIZE)
			{
				int rdc = read(fd, buf, BUFFER_SIZE);
				len -= rdc;
				vector_concat_n(V, buf, rdc);
			}
			rdc = read(fd, buf, len);
			// printf("\t<<RDC>>:%d\n", rdc);
			len -= rdc;
			vector_concat_n(V, buf, rdc);
			rdc = read(fd, buf, 2);
			vector_concat_n(V, buf, rdc);
		}
	}
	return reuse;
}

const char *mount_point = "/mnt/f/Users/Subaru/Documents/PYSPIDER/build";

#include <sys/stat.h>
int file_size(char *filename)
{
	struct stat statbuf;
	stat(filename, &statbuf);
	int size = statbuf.st_size;
	return size;
}

void filesystem_proxy(int fd, char uri[])
{
	if (strstr(uri, "..") != NULL)
	{
		wrap_error(fd, 403, "FORBIDDEN:老师傅别打了求求");
		return;
	}
	if (strcmp(uri, "/") == 0)
		sprintf(uri, "/index.html");

	char file_path[BUFFER_SIZE];
	char tmp[BUFFER_SIZE];

	sprintf(file_path, "%s%s", mount_point, uri);
	int siz = file_size(file_path);
	int file = open(file_path, O_RDONLY);
	if (file < 0)
	{
		if (errno == 2)
		{
			wrap_error(fd, 404, "Not Found");
			return;
		}
		wrap_error(fd, 500, strerror(errno));
		perror("\033[031m打开文件错误：\033[0m");
		puts(file_path);
		return;
	}
	vector_t V;
	vector_init(&V, siz + BUFFER_SIZE);
	vector_concat(&V,
				  "HTTP/1.0 200 OK\r\n"
				  "server: GOMIWEBSERVER\r\n"
				  "content-type: charset=utf-8\r\n");
	sprintf(tmp, "content-length: %d\r\n\r\n", siz);
	vector_concat(&V, tmp);
	read(file, V.begin + vector_size(&V), siz);
	V.end += siz;
	write(fd, V.begin, vector_size(&V));

	printf("vectorsize:%d, realsize:%d\n", vector_size(&V), V.real_size);
	V.end = '\0';
	puts(V.begin);

	vector_destroy(&V);
}

void handle_inbound(int client_fd, int *serverfd)
{
	char buf[BUFFER_SIZE];
	char method[BUFFER_SIZE];
	char uri[BUFFER_SIZE];
	char version[BUFFER_SIZE];

	// rio R;
	ssize_t len = 0;
	int resplen = 0;

	// rio_init(&R, client_fd);
	// rio_buffered_readline(&R, buf, BUFFER_SIZE);
	readline(client_fd, buf);

	puts(buf);

	sscanf(buf, "%s %s %s", method, uri, version);

	// printf("method:%s\n", method);
	// printf("uri:%s\n", uri);
	// printf("version:%s\n", version);

	char host[BUFFER_SIZE];
	char protocol[BUFFER_SIZE];
	char path[BUFFER_SIZE];
	int port;
	if (parse_uri(uri, protocol, host, path, &port) == -1)
	{
		filesystem_proxy(client_fd, uri);
		return;
	}

	printf("\033[034mHOST:%s\n", host);
	printf("PATH:%s\n", path);
	printf("PROTOCOL:%s\n", protocol);
	printf("PORT:%d\n\033[0m", port);

	*serverfd = open_proxyfd(host, port);
	printf("serverfd:%d\n", *serverfd);
	if (*serverfd < 0)
		return;

	if (strcasecmp(method, "CONNECT") == 0)
	{
		// read(client_fd, buf, BUFFER_SIZE);
		// puts(buf);
		do
		{
			readline(client_fd, buf);
		} while (strcmp(buf, "\r\n"));
		sprintf(buf, "%s 200 OK\r\n\r\n", version);
		puts(buf);

		send(client_fd, buf, strlen(buf), 0);
		tunnel_transfer(client_fd, *serverfd, 0);
	}
	else if (1)
	{
		vector_t VECTOR;
		vector_init(&VECTOR, BUFFER_SIZE);
		struct timeval TV;
		TV.tv_sec = 4;
		// setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&TV, sizeof TV);

		printf("\033[036m发现%s请求\n", method);

		sprintf(buf, "%s %s %s\r\n", method, path, version);

		vector_concat(&VECTOR, buf);

		int keep_alive = 1;
		while (keep_alive)
		{
			// rio SVR;
			// rio_init(&SVR, *serverfd);
			// keep_alive = 0;
			// puts("BP1");

			keep_alive = construct_package(&VECTOR, client_fd);

			printf("vectorsize:%d, realsize:%d\n", vector_size(&VECTOR), VECTOR.real_size);
			write(*serverfd, VECTOR.begin, vector_size(&VECTOR));
			// *VECTOR.end = 0;
			// puts(VECTOR.begin);

			vector_clear(&VECTOR);
			// rio_init(&R, client_fd);

			construct_package(&VECTOR, *serverfd);

			printf("vectorsize:%d, realsize:%d\n", vector_size(&VECTOR), VECTOR.real_size);
			write(client_fd, VECTOR.begin, vector_size(&VECTOR));
			// *VECTOR.end = 0;
			// puts(VECTOR.begin);

			vector_clear(&VECTOR);
		}
		puts("\033[0m");

		vector_destroy(&VECTOR);
	}
	else
	{
		// puts("Not Implemented Error");
		wrap_error(client_fd, 501, "Not Implemented Error");
	}
}

int main(int argc, char *argv[])
{
	if (argc > 1)
	{
		bind_port = atoi(argv[1]);
	}
	// printf("参数:%d\n", argc);
	printf("\033[32m垃圾代理：版本1.0.0.20210603\033[0m\n\n");
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

	// int T = 1919810;

	while (1)
	{
		// int pp;
		// {
		// 	printf("\033[34m已回收子进程%d\n\033[0m", pp);
		// }
		sin_size = sizeof(struct sockaddr_in);
		socklen_t siz;
		newfd = accept(sockfd, (struct sockaddr *)&to, &siz);
		printf("新连接传入：%d\n", newfd);
		int serverfd;

		while (waitpid(-1, 0, WNOHANG) > 0)
		{
			printf("\033[34m已回收子进程\n\033[0m");
		}

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
					fprintf(stderr, "\033[32m已经使用%d个进程\033[0m \n", maximum_process_count - pctr);
					handle_inbound(newfd, &serverfd);
					sem_post(available_conn);
					sem_getvalue(available_conn, &pctr);
					printf("\033[32m释放资源...现有%d\033[0m\n", pctr);
					close(newfd);
					close(serverfd);
				}
				break;
			}
		}
	}

	return 0;
}
