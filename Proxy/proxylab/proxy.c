#include <stdio.h>
#include "csapp.h"

/* 缓存大小限制 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* 固定的User-Agent头部 */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* 函数声明 */
void doit(int fd);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void forward_request(int serverfd, char *method, char *path, char *hostname, rio_t *client_rio);
void forward_response(int clientfd, int serverfd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号
    listenfd = Open_listenfd(argv[1]);
    
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
        doit(connfd);
        Close(connfd);
    }
    
    return 0;
}

/* 处理一个HTTP请求/响应事务 */
void doit(int clientfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t client_rio;
    int serverfd;

    Rio_readinitb(&client_rio, clientfd);
    if (!Rio_readlineb(&client_rio, buf, MAXLINE)) {
        return;
    }
    
    printf("Request: %s", buf);
    
    if (sscanf(buf, "%s %s %s", method, uri, version) != 3) {
        clienterror(clientfd, method, "400", "Bad Request", 
                    "Proxy could not parse the request");
        return;
    }
    
    if (strcasecmp(method, "GET")) {
        clienterror(clientfd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }
    
    parse_uri(uri, hostname, port, path);
    
    serverfd = open_clientfd(hostname, port);
    if (serverfd < 0) {
        clienterror(clientfd, hostname, "500", "Internal Server Error",
                    "Proxy failed to connect to the server");
        return;
    }
    
    forward_request(serverfd, method, path, hostname, &client_rio);
    forward_response(clientfd, serverfd);
    Close(serverfd);
}

/* 解析URI，提取主机名、端口和路径 */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;
    
    hostbegin = strstr(uri, "//");
    if (hostbegin == NULL) {
        hostbegin = uri;
    } else {
        hostbegin += 2;
    }
    
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        strcpy(path, "/");
        pathbegin = hostbegin + strlen(hostbegin);
    } else {
        strcpy(path, pathbegin);
    }
    
    hostend = strchr(hostbegin, ':');
    if (hostend != NULL && hostend < pathbegin) {
        len = hostend - hostbegin;
        strncpy(hostname, hostbegin, len);
        hostname[len] = '\0';
        
        len = pathbegin - hostend - 1;
        strncpy(port, hostend + 1, len);
        port[len] = '\0';
    } else {
        len = pathbegin - hostbegin;
        strncpy(hostname, hostbegin, len);
        hostname[len] = '\0';
        strcpy(port, "80");
    }
}

/* 转发HTTP请求到服务器 */
void forward_request(int serverfd, char *method, char *path, char *hostname, rio_t *client_rio)
{
    char buf[MAXLINE];
    int has_host = 0, has_user_agent = 0, has_connection = 0, has_proxy_connection = 0;
    
    sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
    Rio_writen(serverfd, buf, strlen(buf));
    
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }
        
        if (strncasecmp(buf, "Host:", 5) == 0) {
            has_host = 1;
            Rio_writen(serverfd, buf, strlen(buf));
        } else if (strncasecmp(buf, "User-Agent:", 11) == 0) {
            has_user_agent = 1;
            Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
        } else if (strncasecmp(buf, "Connection:", 11) == 0) {
            has_connection = 1;
        } else if (strncasecmp(buf, "Proxy-Connection:", 17) == 0) {
            has_proxy_connection = 1;
        } else {
            Rio_writen(serverfd, buf, strlen(buf));
        }
    }
    
    // 添加必需的头部
    if (!has_host) {
        sprintf(buf, "Host: %s\r\n", hostname);
        Rio_writen(serverfd, buf, strlen(buf));
    }
    
    if (!has_user_agent) {
        Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
    }
    
    if (!has_connection) {
        sprintf(buf, "Connection: close\r\n");
        Rio_writen(serverfd, buf, strlen(buf));
    }
    
    if (!has_proxy_connection) {
        sprintf(buf, "Proxy-Connection: close\r\n");
        Rio_writen(serverfd, buf, strlen(buf));
    }
    
    sprintf(buf, "\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
}

/* 转发服务器响应到客户端 */
void forward_response(int clientfd, int serverfd)
{
    char buf[MAXLINE];
    ssize_t n;
    rio_t server_rio;
    
    Rio_readinitb(&server_rio, serverfd);
    
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        if (rio_writen(clientfd, buf, n) != n) {
            fprintf(stderr, "Error writing to client\n");
            return;
        }
    }
    
    if (n < 0) {
        fprintf(stderr, "Error reading from server\n");
    }
}

/* 向客户端返回错误信息 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
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
    
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
