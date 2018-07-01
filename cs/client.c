#include "task_one.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9619
#define do_err(x) perror(x), exit(EXIT_FAILURE)

int main(int argc, char *argv[]) 
{
    int ret;
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) { do_err("socket"); }
 
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    ret = connect(sock, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (ret < 0) { do_err("connect"); }

    struct sockaddr_in localaddr;
    socklen_t addrlen = sizeof(localaddr);
    ret = getsockname(sock, (struct sockaddr *) &localaddr, &addrlen);
    if (ret < 0) { do_err("getsockname"); }
    printf("client: %s:%hu\n", inet_ntoa(localaddr.sin_addr), ntohs(localaddr.sin_port));

    struct sockaddr_in peeraddr;
    addrlen = sizeof(peeraddr);
    ret = getpeername(sock, (struct sockaddr *) &peeraddr, &addrlen);
    if (ret < 0) { do_err("getsockname"); }
    printf("server: %s:%hu\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));

    char sendbuff[1024] = {0};
    char recvbuff[1024] = {0};
    while (fgets(sendbuff, sizeof(sendbuff), stdin)) {
        sendbuff[strlen(sendbuff) - 1] = '\0';
        write(sock, sendbuff, strlen(sendbuff));

        ret = read(sock, recvbuff, sizeof(recvbuff));
        if (ret == -1) { do_err("read"); }
        else if (ret == 0) {
            printf("server closed.\n");
            break;
        } 

        printf("result: %s\n", recvbuff);
        memset(recvbuff, 0, sizeof(recvbuff));
        memset(sendbuff, 0, sizeof(sendbuff));
    }
    close(sock);

    return 0;
}
