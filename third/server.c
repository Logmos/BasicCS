#include "third.h"

#define MAX_EVENTS 1024
#define MAX_TASKS 4096
#define CONFIG_PATH "proj.conf"

char SERVER_IP[16];
unsigned short SERVER_PORT;
int LOG_SIZE;
int LOGFILE_CNT;

int g_switch; /* global variable dominating loop */
int epollfd;
int ntask;
struct epoll_event events[MAX_EVENTS];
struct task_t {
        int sock;
} tasks[MAX_TASKS]; /* tasks poll */
FILE *log_fp;

pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_t worker_thread_tid;

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

void handle_signal(int sig) {
    printf("received signal: %d\n", sig);
    g_switch = 1;
    ntask = -1;
}

void handle_err(int err, const char *str) {
    if (err) {
        perror(str);
        exit(1);
    }
}

void write_log(const char *str) {
    time_t timep;
    char s[30];
    time(&timep);
    strcpy(s, ctime(&timep));
    fprintf(log_fp, "\ntime:%s   things:%s\n", s, str);
    fseek(log_fp, 0, SEEK_END);
    unsigned fsize = ftell(log_fp);

    if (fsize >= LOG_SIZE) {
        fclose(log_fp);
        sprintf(s, "log_%d.txt", LOGFILE_CNT++);
        log_fp = fopen(s, "w");
        handle_err(log_fp == NULL, "create log file failed.");
    }
}

unsigned short checksum(void *p, unsigned count) {
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

int add_safe(int fst, int scd)
{
    int sum = fst + scd;
    if (fst > 0 && scd > 0 && sum < 0) { return 0;}
    else if (fst < 0 && scd < 0 && sum > 0) { return 0;}
    return 1;
}

void do_task(struct task_t *task) {
    int sock = task->sock;
    struct packet_t pa;
    char msg[50] = { 0 };

    while (1) {
        int ret = readn(sock, &pa, sizeof(pa));
        if (ret == -1) {
            if (errno == EAGAIN)
                return;

        }
        // assert(ret == sizeof(pa));
        // printf("packet.id:%d\npacket.first_num:%d\npacket.second_num:%d\n", pa.id, pa.first_num, pa.second_num); 
        // printf("checksum:%hu\n", checksum(&pa, sizeof(pa)));
        sprintf(msg, "id:%d first:%d second:%d", pa.id, pa.first_num, pa.second_num);
        write_log(msg);

        if  (ret == -1) {
            perror("server read");
            return;
        } else if (ret == 0) {
            struct epoll_event event;
            event.data.fd = sock;
            event.events = EPOLLIN | EPOLLET;
            epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, &event);
            printf("client closed\n");
            return;
        } else {
            sprintf(msg, "id:%d first:%d second:%d", pa.id, pa.first_num, pa.second_num);
            write_log(msg);
            do {
                if (pa.id == DO_SUM) {            
                    if (checksum(&pa, sizeof(pa)) != 0) {
                        pa.id = UNKNOWN;
                    } else {
                        pa.id = RET_SUM;
                        if (add_safe(pa.first_num, pa.second_num)) {
                            pa.first_num += pa.second_num;
                            pa.second_num = pa.checksum = 0;
                            pa.checksum = checksum(&pa, sizeof(pa));
                        } else {
                            pa.id = OVERFLOW;
                        }
                    }
                } else if (pa.id == HEARTBEAT) {
                    // printf("debug: HEARTBEAT\n");
                    pa.id = HEARTBEAT;
                    pa.checksum = 0;
                    pa.checksum = checksum(&pa, sizeof(pa));
                } else { // unknown
                    pa.id = UNKNOWN;
                    pa.checksum = 0;
                    pa.checksum = checksum(&pa, sizeof(pa));
                }
            } while (0);
        }

        ret = write(sock, &pa, sizeof(pa));
        // printf("debug: writted.\n");
        if (ret == -1) {
            perror("server write");
        }
        if (ret == 0) {
            printf("client closed");
            // delete_event(epollfd, fd, EPOLLIN)
        }
    }
}

void *worker_thread(void *arg) {
    while (g_switch == 0) {
        pthread_mutex_lock(&mutex);
        while (ntask == 0) {
            pthread_cond_wait(&cond, &mutex);
        }

        if (ntask == -1) {
            pthread_mutex_unlock(&mutex);
            return NULL;
        } 
        else {
            struct task_t task = tasks[--ntask];
            pthread_mutex_unlock(&mutex);
            do_task(&task);
        }
    }
    return NULL;
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
            printf("ip:%s\n", SERVER_IP);
        } else if (!strcmp("SERVER_PORT", key)) {
            sscanf(value, "%hu", &SERVER_PORT);
            printf("port:%hu\n", SERVER_PORT);
        } else if (!strcmp("LOG_SIZE", key)) {
            sscanf(value, "%d", &LOG_SIZE);
            printf("LOG_SIZE:%d\n", LOG_SIZE); 
        } else {
             write_log("err: unknown config item, ignored.\n");
        }
    }

    fclose(fp);
}

void global_init() {
    char buf[50];
    sprintf(buf, "log_%d.txt", LOGFILE_CNT++);
    log_fp = fopen(buf, "a");

    read_config();

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);

    pthread_create(&worker_thread_tid, NULL, worker_thread, NULL);
}


int main(int argc, char *argv[]) {
    signal(SIGUSR1, handle_signal);
    global_init();

    int listenfd;
    struct sockaddr_in servaddr;
    int ret;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    handle_err(listenfd == -1, "socket");

    int on = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    handle_err(ret == -1, "setsockopt");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    ret = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    handle_err(ret < 0, "bind");

    ret = listen(listenfd, SOMAXCONN);
    handle_err(ret < 0, "listen");

    struct epoll_event event;

    struct sockaddr_in peeraddr;
    socklen_t socklen;
    int sock;
    int nready;
    int i;
    char buf[1024];

    epollfd = epoll_create1(EPOLL_CLOEXEC);
    event.data.fd = listenfd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);

    while (g_switch == 0) {
        nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nready == -1) {
            if (errno == EINTR)
                continue;
            handle_err(1, "epoll_wait");
        }

        for (i = 0; i < nready; ++i) {
            if (events[i].data.fd == listenfd) {
                socklen = sizeof(peeraddr);
                sock = accept4(listenfd, (struct sockaddr *) &peeraddr, &socklen, SOCK_NONBLOCK);
                handle_err(sock == -1, "accept4");

                printf("new client %s:%hu accepted\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
                sprintf(buf, "recv from (%s:%hu)\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
                write_log(buf);

                event.data.fd = sock;
                event.events = EPOLLIN | EPOLLET;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event);
            } else {
                pthread_mutex_lock(&mutex);
                tasks[ntask++].sock = events[i].data.fd;
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
            }
        }
    }
    pthread_join(worker_thread_tid, NULL);
    fclose(log_fp);
    return 0;
}
