#include "third.h"

#define do_err(x) perror(x), exit(EXIT_FAILURE)
#define CONFIG_PATH  "proj.conf"

char SERVER_IP[16];
unsigned short SERVER_PORT;
int LOG_SIZE;
int sock;
volatile int g_switch;
volatile int is_alive;
struct packet_t sendbuff, recvbuff;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

ssize_t readn(int fd, void *buf, size_t count)
{
    size_t nleft = count;
    ssize_t nread;
    char *bufp = (char *) buf;

    while (nleft > 0) {
        nread = read(fd, bufp, nleft);
        if (nread < 0) {
            if (errno == EINTR) { continue; }
            return -1;
        } else if (nread == 0) { return count - nleft; }

        bufp += nread;
        nleft -= nread;
    }

    return count;
}

unsigned short checksum(void *p, unsigned count)
{
    unsigned char *arr = p;
    int sum = 0;
    while (count > 1) {
        sum += *(unsigned short *) arr;
        arr += 2;
        count -= 2;
    }   

    if (count > 0)
        sum += *(unsigned short *) arr;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

void handle_err(int err, const char *str) {
    if (err) {
        perror(str);
        exit(1);
    }
}

void read_config(void) {
    FILE *fp = fopen(CONFIG_PATH, "r");
    handle_err(fp == NULL, "can't open config file.");

    char buf[1024];
    int n;

    while (fgets(buf, sizeof(buf), fp)) {
        n = 0;
        while (isspace(buf[n])) ++n;
        if (buf[n] == '\0')
            continue;

        char *p = strchr(buf, '=');
        *p = '\0';
        char *key = buf;
        char *value = p + 1;

        if (!strcmp("SERVER_IP", key)) {
            sscanf(value, "%s", SERVER_IP);
        //  printf("ip:%s\n", SERVER_IP);
        } else if (!strcmp("SERVER_PORT", key)) {
            sscanf(value, "%hu", &SERVER_PORT);
        //  printf("port:%hu\n", SERVER_PORT);
        } else if (!strcmp("LOG_SIZE", key)) {
            sscanf(value, "%d", &LOG_SIZE);
        //  printf("LOG_SIZE:%d\n", LOG_SIZE); 
        } 
    }
    fclose(fp);
}

void *heart_thread(void *arg) {
    struct packet_t pa;
    pa.id = HEARTBEAT;
    pa.first_num = 0;
    pa.second_num = 0;
    pa.checksum = 0;
    pa.checksum = checksum(&pa, sizeof(pa));

    while (1) {
        sleep(10);
        if (is_alive) {
            pthread_mutex_lock(&mutex);
            is_alive = 0;
            pthread_mutex_unlock(&mutex);
            write(sock, &pa, sizeof(pa));
        } else {
            printf("Oh my!!! Heart is on strike and socket is disconnected...\n");
            close(sock);
            break;
        }
    }
   pthread_exit(NULL);
}

void *recv_thread(void *arg)
{
    struct packet_t pa;
    int nread;
    while (1) {
        nread = readn(sock, &pa, sizeof(pa));
        // printf("packet.id:%d\npacket.first_num:%d\npacket.second_num:%d\n", pa.id, pa.first_num, pa.second_num); 
        // printf("checksum:%hu\n", checksum(&pa, sizeof(pa)));
        if (nread > 0) {
            if (checksum(&pa, sizeof(pa)) != 0) {
                printf("packet is unknown.\n");
            }

            if (pa.id == HEARTBEAT) {
                pthread_mutex_lock(&mutex);
                is_alive = 1;
                pthread_mutex_unlock(&mutex);
            } else if (pa.id == RET_SUM) {
                printf("sum is %d.\n", pa.first_num);
            } else if (pa.id == OVERFLOW) {
                printf("sum is overflow, ignored.\n");
            }
        }
        else {
            break;
        }
    }
    //return NULL;
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
    read_config();
    pthread_t heart_tid, recv_tid;
    pthread_create(&heart_tid, NULL, heart_thread, NULL);
    pthread_create(&recv_tid, NULL, recv_thread, NULL);

    int ret;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { do_err("socket"); }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    ret = connect(sock, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (ret < 0) { do_err("connect"); }

    is_alive = 1;

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

    while (g_switch == 0) {
        if (scanf("%d%d", &sendbuff.first_num, &sendbuff.second_num) == 2) {
            sendbuff.id = DO_SUM;
            sendbuff.checksum = 0;
            sendbuff.checksum = checksum((unsigned char *)&sendbuff, sizeof(sendbuff));
            ret = write(sock, &sendbuff, sizeof(sendbuff));
            if (ret < 0) { do_err("client write"); }
        } else {
            //   printf("wrong input\n");
            //   break;
            do_err("wrong input");
        }
    }
    // sleep(2);
    close(sock);
    pthread_mutex_destroy(&mutex);
    pthread_join(heart_tid, NULL);
    pthread_join(recv_tid, NULL);
    return 0;
}
