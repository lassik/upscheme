#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

#ifndef _WIN32
void closesocket(int fd);
#endif

int open_tcp_port(short portno);
int open_any_tcp_port(short *portno);
int open_any_udp_port(short *portno);
int connect_to_host(char *hostname, short portno);
int connect_to_addr(struct sockaddr_in *host_addr);
int sendall(int sockfd, char *buffer, int bufLen, int flags);
int readall(int sockfd, char *buffer, int bufLen, int flags);
int addr_eq(struct sockaddr_in *a, struct sockaddr_in *b);
int socket_ready(int sock);
