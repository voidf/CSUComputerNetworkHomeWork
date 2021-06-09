// 编译命令: gcc -pthread

const char *mount_point = "/mnt/c/Users/ATRI/Desktop/voidf.github.io-master/voidf.github.io-master"; // 需要挂载的网页目录

#include <asm-generic/socket.h>

#define min(_lhs, _rhs) (_lhs < _rhs ? _lhs : _rhs)

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

const int maximum_process_count = 1 << 7;

u_short bind_port = 11452;

//以下是vector包的C语言实现

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

//以上是vector包

/* 读入行 */
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

/* 拆解uri */
int parse_uri(const char raw_uri[], char protocol[], char host[], char path[], int *port)
{
	char ato[BUFFER_SIZE];
	*path = '\0';
	*protocol = '\0';
	*port = 80; // 默认设80
	int match_cnt = sscanf(raw_uri, "%[^:/]://%s", protocol, ato);
	if (match_cnt == 0)
		sscanf(raw_uri, "%s", ato);
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
	match_cnt = sscanf(ato, "%[^/:?]:%d%s", host, port, path);
	// 没有端口号
	printf("matchcnt:%d\n", match_cnt);
	if (match_cnt == 1)
		sscanf(ato, "%*[^/:?]%s", path);
	else if (match_cnt == 0)
	{
		host[0] = '\0';
		return -1;
	}
	return 0;
}

void wrap_error(int fd, int code, const char *msg)
{
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

struct ft_t
{
	int fromfd, tofd, enable_print;
};

void *endless_piping(void *arg)
{
	struct ft_t *ARG = (struct ft_t *)arg;
	char buffer[BUFFER_SIZE];
	int readcnt;

	while ((readcnt = read(ARG->fromfd, buffer, BUFFER_SIZE)) > 0)
	{
		if (ARG->enable_print)
		{
			printf("从%d处的读入数%d\n", ARG->fromfd, readcnt);
			puts(buffer);
		}
		write(ARG->tofd, buffer, readcnt);
	}
}

/* 开两个线程相互直接传送数据，实现双工管道功能 */
void tunnel_transfer(int fromfd, int tofd, int enable_print)
{
	char buffer[BUFFER_SIZE];

	struct timeval TV;
	TV.tv_sec = 10;
	// setsockopt(fromfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&TV, sizeof TV);
	// setsockopt(tofd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&TV, sizeof TV);

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

/* 从fd中一直尝试读取直到构造一个完整的HTTP包，返回包头是否允许keep-alive */
int construct_package(vector_t *V, int fd)
{
	char buf[BUFFER_SIZE];
	int reuse = 0;
	long long content_length = -1;
	int chunked_flag = 0;

	while (1)
	{
		int rctr = readline(fd, buf);
		if (strcmp(buf, "\r\n") == 0 || rctr == 0)
			break;
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
		if (strcasecmp(key, "Proxy-Connection") == 0)
		{
			if (strcasecmp(value, "keep-alive\r\n") == 0)
				reuse = 1;
		}
		else if (strcasecmp(key, "Connection") == 0)
		{
			if (strcasecmp(value, "keep-alive\r\n") == 0)
				reuse = 1;
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
	}
	vector_concat(V, "\r\n");
	int rp;
	while (content_length > 0)
	{
		printf("Content-Length ===> %lld\n", content_length);
		rp = read(fd, buf, min(BUFFER_SIZE - 1, content_length));
		if (rp == 0)
		{
			perror("\033[031m读错误：过早的EOF\033[0m");
			break;
		}
		content_length -= rp;
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
			while (len)
			{
				rdc = read(fd, buf, len);
				len -= rdc;
				vector_concat_n(V, buf, rdc);
			}
			rdc = readline(fd, buf);
			vector_concat_n(V, buf, rdc);
		}
	}
	return reuse;
}

#include <sys/stat.h>
int file_size(char *filename)
{
	struct stat statbuf;
	stat(filename, &statbuf);
	int size = statbuf.st_size;
	return size;
}

int hex2dec(char c)
{
	if ('0' <= c && c <= '9')
		return c - '0';
	else if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	else if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	else
		return -1;
}

char dec2hex(short int c)
{
	if (0 <= c && c <= 9)
		return c + '0';
	else if (10 <= c && c <= 15)
		return c + 'A' - 10;
	else
		return -1;
}

void urldecode(char url[])
{
	int i = 0;
	int len = strlen(url);
	int res_len = 0;
	char res[BUFFER_SIZE];
	for (i = 0; i < len; ++i)
	{
		char c = url[i];
		if (c != '%')
			res[res_len++] = c;
		else
		{
			char c1 = url[++i];
			char c0 = url[++i];
			int num = 0;
			num = hex2dec(c1) * 16 + hex2dec(c0);
			res[res_len++] = num;
		}
	}
	res[res_len] = '\0';
	strcpy(url, res);
}
/* A3的主逻辑 */
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
	urldecode(file_path);
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
				  "server: GOMIWEBSERVER\r\n");

	char extension[BUFFER_SIZE];
	char content_type[BUFFER_SIZE];
	strcpy(extension, strrchr(file_path, '.'));
	if (strcasecmp(extension, "css") == 0)
		sprintf(content_type, "text/css; charset=utf-8");
	else if (strcasecmp(extension, "mp3") == 0)
		sprintf(content_type, "audio/mpeg");
	else if (strcasecmp(extension, "html") == 0)
		sprintf(content_type, "text/html; charset=utf-8");
	else if (strcasecmp(extension, "js") == 0)
		sprintf(content_type, "application/javascript");
	else if (strcasecmp(extension, "ico") == 0)
		sprintf(content_type, "image/x-icon");
	else if (strcasecmp(extension, "png") == 0)
		sprintf(content_type, "image/png");
	else if (strcasecmp(extension, "json") == 0)
		sprintf(content_type, "application/json");
	else
		sprintf(content_type, "text/plain; charset=utf-8");

	sprintf(tmp, "content-type: %s\r\n", content_type);
	sprintf(tmp, "content-length: %d\r\n\r\n", siz);
	vector_concat(&V, tmp);
	read(file, V.begin + vector_size(&V), siz);
	V.end += siz;
	write(fd, V.begin, vector_size(&V));

	vector_destroy(&V);
}

void handle_inbound(int client_fd, int *serverfd)
{
	char buf[BUFFER_SIZE];
	char method[BUFFER_SIZE];
	char uri[BUFFER_SIZE];
	char version[BUFFER_SIZE];
	ssize_t len = 0;
	int resplen = 0;
	readline(client_fd, buf);
	puts(buf);
	sscanf(buf, "%s %s %s", method, uri, version);
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
	if (*serverfd < 0)
		return;

	if (strcasecmp(method, "CONNECT") == 0)
	{
		do
			readline(client_fd, buf);
		while (strcmp(buf, "\r\n"));
		sprintf(buf, "%s 200 OK\r\n\r\n", version);
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
			keep_alive = construct_package(&VECTOR, client_fd);
			write(*serverfd, VECTOR.begin, vector_size(&VECTOR));
			vector_clear(&VECTOR);
			construct_package(&VECTOR, *serverfd);
			write(client_fd, VECTOR.begin, vector_size(&VECTOR));
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
		bind_port = atoi(argv[1]);
	printf("\033[32m垃圾代理：版本1.0.0.20210603\033[0m\n\n");
	int sockfd, newfd;
	struct sockaddr_in from, to;
	int sin_size;
	sem_t *available_conn = sem_open("/available_semaphore", O_CREAT, 0666, maximum_process_count);
	sem_init(available_conn, 1, maximum_process_count);
	int tmp;
	sem_getvalue(available_conn, &tmp);
	printf("允许同时处理的进程数：%d\n", tmp);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		perror("\033[31m创建socket失败\033[0m");
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
	listen(sockfd, 0);
	printf("监听%d中\n", bind_port);
	while (1)
	{
		sin_size = sizeof(struct sockaddr_in);
		socklen_t siz;
		newfd = accept(sockfd, (struct sockaddr *)&to, &siz);
		printf("新连接传入：%d\n", newfd);
		int serverfd;

		while (waitpid(-1, 0, WNOHANG) > 0)
			printf("\033[34m已回收子进程\n\033[0m");

		if (newfd == -1)
			perror("接收错误");
		else
		{
			pid_t p = fork();
			if (p == 0)
			{
				if (sem_trywait(available_conn) != 0)
				{
					perror("\033[31m并发数已达上限\033[0m");
					printf("\033[31m并发数已达上限\033[0m\n");
					wrap_error(newfd, 114514, "并发数已达上限");
					close(newfd);
				}
				else
				{
					int pctr;
					sem_getvalue(available_conn, &pctr);
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
