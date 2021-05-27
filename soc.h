#ifndef SOC_H
#define SOC_H


#include "getopt.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>

typedef int SOCKET_TYPE;

// 主要配置区
#define BUFFER_SIZE 1024
#define PORT 1145

#endif