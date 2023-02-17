#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

char *content_tmp[MAX_OBJECT_SIZE];
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";

int total_size = 0;  // 缓存中有效载荷总大小
int readcnt = 0;     // 目前读者数量
sem_t mutex_read_cnt, mutex_content;

struct cache {
    char *url;
    char *content;  // web 对象，这里只是简单的将目标服务器发来的数据进行保存
    struct cache *prev;
    struct cache *next;
};

struct cache *head = NULL;
struct cache *tail = NULL;

// 缓存操作辅助函数
char *get_cacheData(char *url);
void move_cacheNode_to_head(struct cache *node);
void delete_tail_cacheNode();
void add_cacheNode(struct cache *node);
struct cache *create_cacheNode(char *url, char *content);

void writer(char *url, char *content);
int reader(int fd, char *url);
void init();

void *doit(void *vargp);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_request(rio_t *real_client, char *new_request, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    init();
    int listenfd, *connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    pthread_t tid;
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));  // 给已连接的描述符分配其自己的内存块，消除竞争
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        Pthread_create(&tid, NULL, doit, connfd);
    }
}

void *doit(void *vargp) {
    int fd = *((int *)vargp);
    Free(vargp);
    Pthread_detach(Pthread_self());

    int real_server_fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE], url[MAXLINE];
    rio_t rio_client, rio_server;
    int port;

    /* Read request line and headers */
    Rio_readinitb(&rio_client, fd);                         // 初始化rio内部缓冲区
    if (!Rio_readlineb(&rio_client, buf, MAXLINE)) return;  // 读到0个字符，return
    // 请求行： GET http://www.cmu.edu/hub/index.html HTTP/1.1
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }

    // 解析uri
    parse_uri(uri, hostname, path, &port);
    strncpy(url, hostname, MAXLINE);
    strncat(url, path, MAXLINE);

    if (reader(fd, url)) {
        Close(fd);
        return 1;
    }
    // 缓存未命中------

    char port_str[10];
    sprintf(port_str, "%d", port);
    // 代理作为客户端，连接目标服务器
    real_server_fd = Open_clientfd(hostname, port_str);

    Rio_readinitb(&rio_server, real_server_fd);  // 初始化rio
    char new_request[MAXLINE];
    sprintf(new_request, "GET %s HTTP/1.0\r\n", path);
    build_request(&rio_client, new_request, hostname, port_str);

    // 向目标服务器发送http报文
    Rio_writen(real_server_fd, new_request, strlen(new_request));

    int char_nums;
    // 从目标服务器读到的数据直接发送给客户端
    int len = 0;
    content_tmp[0] = '\0';
    while ((char_nums = Rio_readlineb(&rio_server, buf, MAXLINE))) {
        len += char_nums;
        if (len <= MAX_OBJECT_SIZE) strncat(content_tmp, buf, char_nums);
        Rio_writen(fd, buf, char_nums);
    }

    if (len <= MAX_OBJECT_SIZE) {
        writer(url, content_tmp);
    }
    Close(fd);  //--------------------------------------------------------
}

void parse_uri(char *uri, char *hostname, char *path, int *port) {
    *port = 80;  // 默认端口
    char *ptr_hostname = strstr(uri, "//");
    //  http://hostname:port/path
    if (ptr_hostname)
        ptr_hostname += 2;  // 绝对uri
    else
        ptr_hostname = uri;  // 相对uri，相对url不包含"http://"或"https://"等协议标识符

    char *ptr_port = strstr(ptr_hostname, ":");
    if (ptr_port) {
        // 字符串ptr_hostname需要以'\0'为结尾标记
        *ptr_port = '\0';
        strncpy(hostname, ptr_hostname, MAXLINE);

        sscanf(ptr_port + 1, "%d%s", port, path);
    } else {  // uri中没有端口号
        char *ptr_path = strstr(ptr_hostname, "/");
        if (ptr_path) {
            strncpy(path, ptr_path, MAXLINE);
            *ptr_path = '\0';
            strncpy(hostname, ptr_hostname, MAXLINE);
        } else {
            strncpy(hostname, ptr_hostname, MAXLINE);
            strcpy(path, "");
        }
    }
}
void build_request(rio_t *real_client, char *new_request, char *hostname, char *port) {
    char temp_buf[MAXLINE];

    // 获取client的请求报文
    while (Rio_readlineb(real_client, temp_buf, MAXLINE) > 0) {
        if (strstr(temp_buf, "\r\n")) break;  // end

        // 忽略以下几个字段的信息
        if (strstr(temp_buf, "Host:")) continue;
        if (strstr(temp_buf, "User-Agent:")) continue;
        if (strstr(temp_buf, "Connection:")) continue;
        if (strstr(temp_buf, "Proxy Connection:")) continue;

        sprintf(new_request, "%s%s", new_request, temp_buf);
        printf("%s\n", new_request);
        fflush(stdout);
    }
    sprintf(new_request, "%sHost: %s:%s\r\n", new_request, hostname, port);
    sprintf(new_request, "%s%s%s%s", new_request, user_agent_hdr, conn_hdr, proxy_hdr);
    sprintf(new_request, "%s\r\n", new_request);
}
void init() {
    Sem_init(&mutex_content, 0, 1);
    Sem_init(&mutex_read_cnt, 0, 1);
}
/* 创建缓存节点 */
struct cache *create_cacheNode(char *url, char *content) {
    struct cache *node = (struct cache *)malloc(sizeof(struct cache));

    int len = strlen(url);
    node->url = (char *)malloc(len * sizeof(char));
    strncpy(node->url, url, len);

    len = strlen(content);
    node->content = (char *)malloc(len * sizeof(char));
    strncpy(node->content, content, len);

    node->prev = NULL;
    node->next = NULL;

    return node;
}
/* 将节点添加到缓存头部 */
void add_cacheNode(struct cache *node) {
    node->next = head;
    node->prev = NULL;
    if (head != NULL) {
        head->prev = node;
    }
    head = node;
    if (tail == NULL) {
        tail = node;
    }
    total_size += strlen(node->content) * sizeof(char);
}
/* 删除缓存尾部节点 */
void delete_tail_cacheNode() {
    if (tail != NULL) {
        total_size -= strlen(tail->content) * sizeof(char);
        Free(tail->content);
        Free(tail->url);
        struct cache *tmp = tail;
        tail = tail->prev;
        Free(tmp);

        if (tail != NULL) {
            tail->next = NULL;
        } else {
            head = NULL;
        }
    }
}
/* 移动缓存节点到头部 */
void move_cacheNode_to_head(struct cache *node) {
    if (node == head) {
        return;
    } else if (node == tail) {
        tail = tail->prev;
        tail->next = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    node->prev = NULL;
    node->next = head;
    head->prev = node;
    head = node;
}
/* 获取缓存数据 */
char *get_cacheData(char *url) {
    struct cache *node = head;
    while (node != NULL) {
        if (strcmp(node->url, url) == 0) {
            move_cacheNode_to_head(node);
            return node->content;
        }
        node = node->next;
    }
    return NULL;
}

int reader(int fd, char *url) {
    int find = 0;
    P(&mutex_read_cnt);
    readcnt++;
    if (readcnt == 1)  // first in
        P(&mutex_content);
    V(&mutex_read_cnt);

    char *content = get_cacheData(url);
    if (content) {  // 命中
        Rio_writen(fd, content, strlen(content));
        find = 1;
    }

    P(&mutex_read_cnt);
    readcnt--;
    if (readcnt == 0)  // last out
        V(&mutex_content);
    V(&mutex_read_cnt);

    return find;
}
void writer(char *url, char *content) {
    P(&mutex_content);
    while (total_size + strlen(content) * sizeof(char) > MAX_CACHE_SIZE) {
        delete_tail_cacheNode();
    }
    struct cache *node = create_cacheNode(url, content_tmp);
    add_cacheNode(node);
    V(&mutex_content);
}
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf,
            "<body bgcolor="
            "ffffff"
            ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
