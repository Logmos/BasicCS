#include "fifth.h"
#include "threadpool.h"

#define MAX_EVENTS 1024
#define CONFIG_PATH "proj.conf"
#define DLL_FILE_NAME "./libfifth.so"

char SERVER_IP[16];
char recvbuff[1024];
char g_path[512];
unsigned short SERVER_PORT;
int LOG_SIZE;
int LOGFILE_CNT;

int g_switch; 
int epollfd;
struct epoll_event events[MAX_EVENTS];
FILE *log_fp;
threadpool_t *thp;

void *handle;
int (*check_complete)(const char *request);
void (*get_path)(const char *request);
void (*handle_request)(char *path, int connfd);

void handle_signal(int sig) {
    printf("received signal: %d\n", sig);
    g_switch = 1;
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
        sprintf(s, "./LOG/log_%d.txt", LOGFILE_CNT++);
        log_fp = fopen(s, "w");
        handle_err(log_fp == NULL, "create log file failed.");
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
            printf("IP:%s\n", SERVER_IP);
        } else if (!strcmp("SERVER_PORT", key)) {
            sscanf(value, "%hu", &SERVER_PORT);
            printf("PORT:%hu\n", SERVER_PORT);
        } else if (!strcmp("LOG_SIZE", key)) {
            sscanf(value, "%d", &LOG_SIZE);
            printf("LOG_SIZE:%d\n", LOG_SIZE); 
        } else {
             write_log("err: unknown config item, ignored.\n");
        }
    }

    fclose(fp);
}


void do_task(void *task) {
    int sock = *(int *)task;
    free(task);

    while (1) {
        int ret = read(sock, recvbuff, 1024);
        //printf("debug: ret is %d\n", ret);
        //printf("debug: recv is %s\n", recvbuff);
        if (ret == -1) {
            if (errno == EAGAIN) {
                struct epoll_event event;
                event.data.fd = sock;
                event.events = EPOLLIN | EPOLLET;
                epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, &event);
                close(sock);
            } else {
                perror("server read");
            }
            return;
        }
        if (ret == 0) {
            struct epoll_event event;
            event.data.fd = sock;
            event.events = EPOLLIN | EPOLLET;
            epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, &event);
            printf("client closed\n");
            return;
        } else {
            printf("Rcv--------------\n%s\nEnd-------------\n", recvbuff);
            write_log(recvbuff);
        }

        if (check_complete(recvbuff)) {
            get_path(recvbuff);
            handle_request(g_path, sock);  
            //printf("debug: handle_request is finished\n");
        } else {
            ret = write(sock, "Invalid request\n", 20);
            if (ret == -1) {
                perror("server writes invlid request");
            } else if (ret == 0) {
                printf("client closed\n");
            }
            perror("resource wanted is invaild, ignored");
        }
    }
}

void global_init() {
    int i;
    char buf[50];
    if (access("./LOG", F_OK) != 0) {
        if (mkdir("./LOG", 0755) < 0) {
            perror("mkdir LOG failed");
            exit (-1);
        }
    }
    sprintf(buf, "./LOG/log_%d.txt", LOGFILE_CNT++);
    log_fp = fopen(buf, "a");

    read_config();

    thp = threadpool_create(8, 128, 0);
    //printf("threadpool inited\n");

    handle = dlopen(DLL_FILE_NAME, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "Failed to open library %s err:%s\n", DLL_FILE_NAME, dlerror());
        exit(EXIT_FAILURE);
    }

    check_complete = dlsym(handle, "check_complete");
    get_path = dlsym(handle, "get_path");
    handle_request = dlsym(handle, "handle_request");
}

void global_destroy() {
    fclose(log_fp);
    threadpool_destroy(thp, threadpool_specific_way);
    dlclose(handle);
}

int open_listenfd() {
    int listenfd, ret;
    int optval = 1;
    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    handle_err(listenfd == -1, "server listenfd");

    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval));
    handle_err(ret == -1, "setsockopt in eliminating address already used\n");

    ret = setsockopt(listenfd, 6, TCP_CORK, (const void *)&optval, sizeof(optval));
    handle_err(ret == -1, "setsockopt in enhancing response speed");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    ret = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    handle_err(ret < 0, "server bind");

    ret = listen(listenfd, SOMAXCONN);
    handle_err(ret < 0, "server listen");

    return listenfd;
}

int main(int argc, char *argv[])
{
    signal(SIGUSR1, handle_signal);
    global_init();

    int listenfd = open_listenfd();

    struct epoll_event event;

    struct sockaddr_in peeraddr;
    socklen_t socklen;
    int sock;
    int nready, i;
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
                int *ptr = malloc(sizeof(int));
                *ptr = events[i].data.fd;
                threadpool_add(thp, do_task, (void *)(ptr), 0);
            }
        }
    }

    global_destroy();
    return 0;
}
