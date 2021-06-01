#include <stdio.h>
#include <string.h>
const char *cc = "live.bilibili.com";

char proto[114514] = "gfdnjaikfhaewlhuclk";
char host[114514] = "fgnuwaibcnluwheloiahelw";
char path[114514] = "fgmnijchanwklfhweiloalowhl";
int port = 23412;

const int BUFFER_SIZE = 1 << 14;

void parse_uri(const char raw_uri[], char protocol[], char host[], char path[], int *port)
{
	char ato[BUFFER_SIZE];

	*port = 80; // 默认设80

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

    printf("CNT:%d\n", match_cnt);
	// 没有端口号
	if (match_cnt == 1)
	{
		printf("MATCH:%d\n",sscanf(ato, "%*[^/:?]%s", path));
	}

}

signed main()
{
    parse_uri(cc, proto, host, path, &port);
    printf("PROTOCOL:");
    puts(proto);
    printf("HOST:");
    puts(host);
    printf("PATH:");
    puts(path);
    printf("PORT:%d\n", port);
    return 0;
}