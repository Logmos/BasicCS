#include "second.h"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9619 
#define do_err(x) perror(x), exit(EXIT_FAILURE)
unsigned char checksum(unsigned char *arr, int count);

struct data {
    int fst;
    int scd;
    int checkvalue;
} recvbuff;

void add_two_int32(int sockfd)
{
    static int ret, len = sizeof(recvbuff);
    while (1) {
        memset(&recvbuff, 0, len); 
        ret = read(sockfd, &recvbuff, len);
        if (ret == -1) { do_err("server read"); }
        if (ret == 0) {
            printf("client closed\n");
            break;
        }
        char sendbuff[1024] = { 0 };
        if (checksum((unsigned char *)&recvbuff, len) == 0) {
            printf("server received :%d %d \n", recvbuff.fst, recvbuff.scd);
            sprintf(sendbuff, "%d", recvbuff.fst + recvbuff.scd);
        }
        else {
            sprintf(sendbuff, "%s", "data is damaged during transportation!!!\n");
        }

        ret = write(sockfd, sendbuff, strlen(sendbuff));
        if (ret == -1) { do_err("server write"); }
        if (ret == 0) { printf("client closed\n"); }
    }
}

int main(int argc, char *argv[])
{
    int listenfd;
    struct sockaddr_in servaddr;
    int ret;

    if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        do_err("server socket");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int optval = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret < 0) { do_err("setsockopt"); }

    ret = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (ret < 0) { do_err("bind"); }

    ret = listen(listenfd, SOMAXCONN);
    if (ret < 0) { do_err("listen"); }

    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    int connsock;

    while (1) {
        connsock = accept(listenfd, (struct sockaddr *) &peeraddr, &peerlen);
        if (connsock < 0) { do_err("accept"); }
        printf("client: %s:%hu\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
        add_two_int32(connsock);
    }
    close(listenfd);

    return 0;
}
