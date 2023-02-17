# CSAPP Proxy Lab

本实验需要实现一个web代理服务器，实现逐步从迭代到并发，到最终的具有缓存功能的并发代理服务器。

Web 代理是充当 Web 浏览器和终端服务器之间的中间人的程序。浏览器不是直接联系终端服务器获取网页，而是联系代理，代理将请求转发到终端服务器。当终端服务器回复代理时，代理将回复发送给浏览器。

本实验共三个部分，具体要求如下：

-   在本实验的第一部分，您将设置代理以接受传入连接、读取和解析请求、将请求转发到 Web 服务器、读取服务器的响应并将这些响应转发到相应的客户端。第一部分将涉及学习基本的 HTTP 操作以及如何使用套接字编写通过网络连接进行通信的程序。
-   在第二部分中，您将升级代理以处理多个并发连接。这将向您介绍如何处理并发，这是一个重要的系统概念。
-   在第三部分也是最后一部分，您将使用最近访问的 Web 内容的简单主内存缓存将缓存添加到您的代理。

##  **Part I**

实现迭代Web代理，首先是实现一个处理`HTTP/1.0 GET`请求的基本迭代代理。开始时，我们的代理应侦听端⼝上的传⼊连接，端⼝号将在命令行中指定。建⽴连接后，您的代理应从客⼾端读取整个请求并解析请求。它应该判断客户端是否发送了⼀个有效的 HTTP 请求；如果是这样，它就可以建⽴自⼰与适当的 Web 服务器的连接，然后请求客⼾端指定的对象。最后，您的代理应读取服务器的响应并将其转发给客⼾端。

我们先将`tiny.c`中的基本框架复制过来，移除不需要的函数，保留`doit,parse_uri,clienterror`即可，其他还用不到，接下来我们需要修改的是`doit`和`parse_uri`，`doit`应该做的事如下：

1.  读取客户端的请求行，判断其是否是GET请求，若不是，调用`clienterror`向客户端打印错误信息。
2.  `parse_uri`调用解析uri，提取出主机名，端口，路径信息。
3.  代理作为客户端，连接目标服务器。
4.  调用`build_request`函数构造新的请求报文`new_request`。
5.  将请求报文`build_request`发送给目标服务器。
6.  接受目标服务器的数据，并将其直接发送给源客户端。

代码如下：

```c
#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";

void doit(int fd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_request(rio_t *real_client, char *new_request, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}

void doit(int fd) {
    int real_server_fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
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
    while ((char_nums = Rio_readlineb(&rio_server, buf, MAXLINE))) Rio_writen(fd, buf, char_nums);
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

```

若程序出现错误，printf大法依然是定位错误的好方法。此外可以通过使用`curl`来模拟操作。需要注意的是需要先运行proxy和tiny再运行curl，tiny就相当于一个目标服务器，curl则相当于一个客户端。

![](Proxy%20lab.assets/pro1.png)

##  **Part II**	

接下来我们需要改变上面的程序，使其可以处理多个并发请求，这里使用多线程来实现并发服务器。具体如下：

-   `Accept`之后通过创建新的线程来完成`doit`函数。
-   注意：由于并发导致的竞争，所以需要注意connfd传入的形式，这里选择将每个已连接描述符分配到它自己的动态分配的内存块。

代码如下，只需要在`Part I `基础上略作修改即可。

```c
#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";

void *doit(void *vargp);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_request(rio_t *real_client, char *new_request, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int main(int argc, char **argv) {
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
        connfd = Malloc(sizeof(int));       // 给已连接的描述符分配其自己的内存块，消除竞争
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
    char hostname[MAXLINE], path[MAXLINE];
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
    while ((char_nums = Rio_readlineb(&rio_server, buf, MAXLINE))) Rio_writen(fd, buf, char_nums);

    Close(fd);
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

```

##  **Part III**

第三部分需要添加缓存web对象的功能。根据实验文档的要求我们需要实现对缓存实现读写者问题，且缓存的容量有限，当容量不足是，要按照类似`LRU`算法进行驱逐。我们先定义缓存的结构，这里使用的是双向链表，选择这个数据结构的原因在于`LRU`算法的需求，链尾即使最近最少使用的web对象。关于缓存的定义以及相关操作如下：

```c
struct cache {
    char *url;
    char *content;  // web object，这里只是简单的将目标服务器发来的数据进行保存
    struct cache *prev;
    struct cache *next;
};

struct cache *head = NULL;
struct cache *tail = NULL;


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

```

此外还需要实现读者写者问题，为此定义了如下几个相关变量：

```c
int readcnt = 0;     // 目前读者数量
sem_t mutex_read_cnt, mutex_content;
void init() {
    Sem_init(&mutex_content, 0, 1);
    Sem_init(&mutex_read_cnt, 0, 1);
}
```

同时，定义了`reader`和`writer`函数作为读者和写者。

-   `int reader(int fd, char *url);`其内调用`get_cacheData`检查是否缓存命中，若是，则将所缓存的数据通过fd发送给客户端，否则返回0表示缓存未命中。
-   `void writer(char **url*, char **content*);`缓存未命中后，与之前一样进行代理服务，从目标服务器接收数据后发送到客户端，如果web object的大小符号要求的话，再调用`writer`将接收的数据进行缓存。

总代码如下：

```c
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

```

## 总结

测试结果如下，顺利拿下满分。

![](Proxy%20lab.assets/pro2.png)

本实验和上一个`malloclab`实验就不是一个级别的，可以说此实验是很简单的，也就比`datalab`略难一下。由于本身也有一些web服务器的学习经验，所以做起来还是比较轻松的，但此实验无疑新手练习多线程和并发的好实验。

至此CSAPP的最后一个lab也完成了，一共8个，除开`malloclab`得了98分外，其他7个实验均拿下满分。