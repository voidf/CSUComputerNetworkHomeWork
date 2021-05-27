#include "soc.h"

// int bind_to_port(SOCKET_TYPE socket_type, int port)
// {
//     struct sockaddr_storage socket_address;

//     int addr_length = 0;

//     memset(&socket_address, 0, sizeof(socket_address));

// }

#define set0(__tobeset0__) bzero(__tobeset0__, sizeof(__tobeset0__))

int32_t main(int argc, char *argv[])
{
    int socket_handler, msg_len;
    struct sockaddr_in inbound_addr, remote_addr;
    char buffer[BUFFER_SIZE];

    int result, len;

    // af:      地址族
    // Address Family
    // pf:      协议族，与af等价
    // Protocol Family
    // AF_INET  =>  ipv4
    // AF_INET6 =>  ipv6

    // SOCK_STREAM 面向连接用
    // SOCK_DGRAM  无连接用

    // IPPROTO_TCP TCP协议
    // IPPROTO_UDP UDP协议
    // 填0则自动

    socket_handler = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_handler == -1)
    {
        perror("Error at establish socket.");
        return 114514;
    }

    memset(&inbound_addr, 0, sizeof(inbound_addr));

    inbound_addr.sin_family = AF_INET;
    inbound_addr.sin_port = htons(PORT);

    inbound_addr.sin_addr = INADDR_ANY;
    set0(inbound_addr.sin_zero);

    result = bind(socket_handler, (struct sockaddr *)&inbound_addr, sizeof(inbound_addr));

    

    return 0;
}