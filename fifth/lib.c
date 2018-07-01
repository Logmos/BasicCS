#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

extern char g_path[];

int check_complete(const char *request)
{
    int len = strlen(request);
    const char *method = strstr(request, "GET");
    //printf("debug: check_complete is completed\n");
    if (method && len > 74) {
        printf("debug: request is valid\n");
        return 1; 
    }
    else { 
        printf("debug: request is invalid\n");
        return 0;}
}

void get_path(const char *request)
{
    char *path_head = strchr(request, '/');
    char *version = strstr(request, "HTTP");
    strncpy(g_path, path_head, version - path_head);
    g_path[version - path_head - 1] = '\0';
    //printf("debug: g_path is %s\n", g_path);
    return;
}

void handle_request(char *path, int connfd)  
{
    struct stat file_info;
    int ret = stat(path, &file_info);
    if (ret == 0) {
        unsigned file_size = file_info.st_size; 
        printf("debug: file_size if %d\n", file_size);
        if (file_size <= 4096) {
            int local_fd = open(path, O_RDONLY);
            int optval = 1;
            setsockopt(connfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)); 
            ret = write(connfd, "Valid\n", 20);
            if (ret == -1) {
                perror("server write");
            } else if (ret == 0) {
                printf("client closed\n");
            }
            ret = sendfile(connfd, local_fd, 0, file_size);
            close(local_fd);
            if (ret > 0) {
                printf("debug: sendfile succeeded\n");
            } else if (ret < 0) {
                printf("sendfile returns %d\n", ret);
                perror("server sendfile");
            } else if (ret == 0) {
                perror("client closed in handle_request()");
            }
            optval = 0;
            setsockopt(connfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));
        } else {
            perror("file package exceeds 4k, ignored");
        }
    } else {
        perror("wanted file stat");
    }
    return;
}
