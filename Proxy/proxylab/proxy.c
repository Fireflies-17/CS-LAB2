#include <stdio.h>
#include "csapp.h"

/* 缓存大小限制 */
#define MAX_CACHE_SIZE 1049000      // 最大缓存1MB
#define MAX_OBJECT_SIZE 102400      // 单个对象最大100KB

/* 固定的User-Agent头部 */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* 函数声明 */
void doit(int fd);
void *thread(void *vargp);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void forward_request(int serverfd, char *method, char *path, char *hostname, rio_t *client_rio);
void forward_response(int clientfd, int serverfd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/*
 * main - 代理服务器主函数
 * 创建监听套接字，接受客户端连接，为每个连接创建独立线程
 */
int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* 检查命令行参数 */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* 忽略SIGPIPE信号，防止写入已关闭的套接字时程序崩溃 */
    Signal(SIGPIPE, SIG_IGN);
    
    /* 在指定端口创建监听套接字 */
    listenfd = Open_listenfd(argv[1]);
    
    /* 主循环：持续接受客户端连接 */
    while (1) {
        clientlen = sizeof(clientaddr);
        
        /* 在堆上分配连接描述符，避免线程间的竞争条件 */
        connfdp = Malloc(sizeof(int));
        
        /* 接受客户端连接 */
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        /* 获取并打印客户端信息 */
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        /* 创建新线程处理该连接，实现并发 */
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    
    return 0;
}

/*
 * thread - 线程例程
 * 每个线程独立处理一个客户端连接
 * vargp指向连接描述符的指针
 */
void *thread(void *vargp)
{
    /* 从参数中提取连接描述符 */
    int connfd = *((int *)vargp);
    
    /* 分离线程，使其结束时自动释放资源 */
    Pthread_detach(Pthread_self());
    
    /* 释放堆上分配的连接描述符内存 */
    Free(vargp);
    
    /* 处理客户端请求 */
    doit(connfd);
    
    /* 关闭客户端连接 */
    Close(connfd);
    
    return NULL;
}

/*
 * doit - 处理一个HTTP请求/响应事务
 * 读取客户端请求，连接目标服务器，转发请求和响应
 */
void doit(int clientfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t client_rio;
    int serverfd;

    /* 初始化RIO读缓冲区 */
    Rio_readinitb(&client_rio, clientfd);
    
    /* 读取请求行 */
    if (!Rio_readlineb(&client_rio, buf, MAXLINE)) {
        return;  // 读取失败，直接返回
    }
    
    printf("Request: %s", buf);
    
    /* 解析请求行，提取方法、URI和版本 */
    if (sscanf(buf, "%s %s %s", method, uri, version) != 3) {
        clienterror(clientfd, method, "400", "Bad Request", 
                    "Proxy could not parse the request");
        return;
    }
    
    /* 只支持GET方法 */
    if (strcasecmp(method, "GET")) {
        clienterror(clientfd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }
    
    /* 解析URI，提取主机名、端口号和路径 */
    parse_uri(uri, hostname, port, path);
    
    /* 连接到目标服务器 */
    serverfd = open_clientfd(hostname, port);
    if (serverfd < 0) {
        clienterror(clientfd, hostname, "500", "Internal Server Error",
                    "Proxy failed to connect to the server");
        return;
    }
    
    /* 向服务器转发请求 */
    forward_request(serverfd, method, path, hostname, &client_rio);
    
    /* 将服务器响应转发给客户端 */
    forward_response(clientfd, serverfd);
    
    /* 关闭与服务器的连接 */
    Close(serverfd);
}

/*
 * parse_uri - 解析URI，提取主机名、端口和路径
 */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;
    
    /* 查找"://"，定位主机名的起始位置 */
    hostbegin = strstr(uri, "//");
    if (hostbegin == NULL) {
        hostbegin = uri;  // 没有"://"，直接从uri开始
    } else {
        hostbegin += 2;   // 跳过"//"
    }
    
    /* 查找'/'，定位路径的起始位置 */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        strcpy(path, "/");  // 没有路径，使用默认根路径
        pathbegin = hostbegin + strlen(hostbegin);
    } else {
        strcpy(path, pathbegin);  // 复制路径部分
    }
    
    /* 查找':'，检查是否指定了端口号 */
    hostend = strchr(hostbegin, ':');
    if (hostend != NULL && hostend < pathbegin) {
        /* 端口号已指定 */
        len = hostend - hostbegin;
        strncpy(hostname, hostbegin, len);
        hostname[len] = '\0';
        
        /* 提取端口号 */
        len = pathbegin - hostend - 1;
        strncpy(port, hostend + 1, len);
        port[len] = '\0';
    } else {
        /* 端口号未指定，使用默认HTTP端口80 */
        len = pathbegin - hostbegin;
        strncpy(hostname, hostbegin, len);
        hostname[len] = '\0';
        strcpy(port, "80");
    }
}

/*
 * forward_request - 转发HTTP请求到服务器
 * 
 * 1. 发送请求行（转换为HTTP/1.0）
 * 2. 读取并转发客户端的请求头
 * 3. 添加/替换必需的请求头（Host, User-Agent, Connection, Proxy-Connection）
 */
void forward_request(int serverfd, char *method, char *path, char *hostname, rio_t *client_rio)
{
    char buf[MAXLINE];
    int has_host = 0, has_user_agent = 0, has_connection = 0, has_proxy_connection = 0;
    
    /* 发送请求行：将HTTP/1.1转换为HTTP/1.0 */
    sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
    Rio_writen(serverfd, buf, strlen(buf));
    
    /* 读取并处理客户端发送的请求头 */
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        /* 遇到空行，表示请求头结束 */
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }
        
        /* 检查并处理特殊的请求头 */
        if (strncasecmp(buf, "Host:", 5) == 0) {
            has_host = 1;
            Rio_writen(serverfd, buf, strlen(buf));  // 转发原有的Host头
        } else if (strncasecmp(buf, "User-Agent:", 11) == 0) {
            has_user_agent = 1;
            Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));  // 替换为固定的User-Agent
        } else if (strncasecmp(buf, "Connection:", 11) == 0) {
            has_connection = 1;  // 记录已有，稍后替换
        } else if (strncasecmp(buf, "Proxy-Connection:", 17) == 0) {
            has_proxy_connection = 1;  // 记录已有，稍后替换
        } else {
            Rio_writen(serverfd, buf, strlen(buf));  // 转发其他头部
        }
    }
    
    /* 确保必需的请求头存在 */
    
    /* Host头：指定目标服务器主机名 */
    if (!has_host) {
        sprintf(buf, "Host: %s\r\n", hostname);
        Rio_writen(serverfd, buf, strlen(buf));
    }
    
    /* User-Agent头：标识客户端类型 */
    if (!has_user_agent) {
        Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
    }
    
    /* Connection头：告知服务器请求后关闭连接 */
    if (!has_connection) {
        sprintf(buf, "Connection: close\r\n");
        Rio_writen(serverfd, buf, strlen(buf));
    }
    
    /* Proxy-Connection头：代理特有的连接控制 */
    if (!has_proxy_connection) {
        sprintf(buf, "Proxy-Connection: close\r\n");
        Rio_writen(serverfd, buf, strlen(buf));
    }
    
    /* 发送空行，标志请求头结束 */
    sprintf(buf, "\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
}

/*
 * forward_response - 转发服务器响应到客户端
 * 从服务器读取所有数据（包括头部和主体），并转发给客户端
 */
void forward_response(int clientfd, int serverfd)
{
    char buf[MAXLINE];
    ssize_t n;
    rio_t server_rio;
    
    /* 初始化RIO读缓冲区 */
    Rio_readinitb(&server_rio, serverfd);
    
    /* 持续读取服务器数据并转发给客户端 */
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        /* 使用rio_writen（非包装版本）以便检查返回值 */
        if (rio_writen(clientfd, buf, n) != n) {
            fprintf(stderr, "Error writing to client\n");
            return;  // 写入失败，可能客户端已断开
        }
    }
    
    /* 检查读取是否出错 */
    if (n < 0) {
        fprintf(stderr, "Error reading from server\n");
    }
}

/*
 * clienterror - 向客户端返回错误信息
 * 构造并发送HTTP错误响应
 *   fd - 客户端连接描述符
 *   cause - 引起错误的原因（如方法名、文件名等）
 *   errnum - 错误代码（如"404"）
 *   shortmsg - 简短错误信息（如"Not Found"）
 *   longmsg - 详细错误描述
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    /* 构造HTTP响应主体（HTML格式） */
    strcpy(body, "<html><title>Proxy Error</title>");
    strcat(body, "<body bgcolor=\"ffffff\">\r\n");
    strcat(body, errnum);
    strcat(body, ": ");
    strcat(body, shortmsg);
    strcat(body, "\r\n");
    strcat(body, "<p>");
    strcat(body, longmsg);
    strcat(body, ": ");
    strcat(body, cause);
    strcat(body, "\r\n");
    strcat(body, "<hr><em>The Proxy server</em>\r\n");
    
    /* 发送HTTP响应头 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    
    /* 发送HTTP响应主体 */
    Rio_writen(fd, body, strlen(body));
}
