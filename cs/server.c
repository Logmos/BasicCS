#include "task_one.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9619 
#define do_err(x) perror(x), exit(EXIT_FAILURE)

void add_two_int32(int sockfd)
{
    char recvbuff[1024];
    static int ret;
    static int fst, scd;
    while (1) {
        memset(recvbuff, 0, sizeof(recvbuff)); 
        ret = read(sockfd, recvbuff, 1024);
        if (ret == -1) { do_err("server read"); }
        if (ret == 0) {
            printf("client closed\n");
            break;
        }
        printf("server received: %s\n", recvbuff);

        char sendbuff[1024] = { 0 };
        if (sscanf(recvbuff, " %d %d", &fst, &scd) == 2) {
            sprintf(sendbuff, "%d", fst + scd);
        }
        else {
            sprintf(sendbuff, "%s", "wrong input, please try another\n");
        }

        ret = write(sockfd, sendbuff, strlen(sendbuff));
        if (ret == -1) { do_err("server write"); }
        if (ret == 0) { printf("client closed\n"); }

    }
}

int main(int argc,char *argv[])
{
    int listenfd;
    struct sockaddr_in servaddr;
    int ret;

    if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        do_err("server socket");
    }

    memset(&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int on = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret < 0) { do_err("setsockopt"); }

    ret = bind(listenfd, (struct sockaddr *) &servaddr, sizeof (servaddr));
    if (ret < 0) { do_err("bind"); }

    ret = listen(listenfd, SOMAXCONN);
    if (ret < 0) { do_err("listen"); }

    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof (peeraddr);
    int sock;

    while (1) {
        sock = accept(listenfd, (struct sockaddr *) &peeraddr, &peerlen);
        if (sock < 0) { do_err("accept"); }
        printf("client: %s:%hu\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
        add_two_int32(sock);
    }
    close(listenfd);

    return 0;
}
