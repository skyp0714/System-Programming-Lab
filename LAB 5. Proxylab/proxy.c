#include <stdio.h>
#include <string.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* Port number */
#define MAX_PORT_NUM 64999
#define MIN_PORT_NUM 4501

/* Structure */
typedef struct{
    char method[10];
    char url[200];
    char protocol[20];
    char host[200];
    char port[10];
    char content[1000];
    char version[10];
} Request;
/*  Cache for holding responses
  * doubly linked list
  * sorted in time order
  * eviction in LRU policy
*/
typedef struct CacheItem{
    char host[200];
    char content[1000];
    size_t size;
    char* data;
    struct CacheItem* next;
    struct CacheItem* prev;
    clock_t time;
}CacheItem;

typedef struct{
    CacheItem *head;
    CacheItem *tail;
    size_t size;
} CacheList;

CacheList cache;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *version = "HTTP/1.0\r\n";

/* Function Declarations */
void *proxy_thread(void *vargp);
void read_request(int fd, Request *req, char *pr);
void parse_line(Request *req, char *buf);
void set_host_port(char* hp, char *h, char *p);
void print_request(Request *req);
void send_response(int server_fd, int client_fd, Request *req);
char* xstrncpy(char * dst, const char *src, size_t n);

void init_cache();
CacheItem *cache_check(Request *req);
void send_cached_response(int fd, CacheItem *item);
void cache_update(CacheItem *item);
void insert_item(Request *req, char *data, int size);
void evict_LRU();
void delete_item(CacheItem *item);
void print_cache();


int main(int argc, char **argv)
{
    //ignore SIGPIPE
    Signal(SIGPIPE, SIG_IGN);

    char *port = argv[1];
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    int listenfd;
    pthread_t tid;

    // initialize cache
    init_cache();

    //argument check
    if(argc != 2){
        printf("ERROR(main): wrong argument!\n");
        exit(1);
    }
    //port check
    if(atoi(port) < MIN_PORT_NUM || atoi(port) > MAX_PORT_NUM){
        printf("ERROR(main): invalid port number!\n");
        exit(1);
    }
    //handling concurrent requests
    listenfd = Open_listenfd(port);
    while(1) {
        int *connfdp = Malloc(sizeof(int));
        *connfdp =Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, proxy_thread, connfdp);
    }

    return 0;
}
/*
1. read and parse request
2. forward request to server
3. forward response to client
4. add cache
*/
void *proxy_thread(void *vargp){
    int client_fd = *((int *) vargp);
    int server_fd;
    CacheItem *cache_hit;
    char proxy_request[MAXBUF];
    Request *Requestp = Malloc(sizeof(Request));
    Pthread_detach(pthread_self());
    free(vargp);
    // analysize request and build proxy request
    read_request(client_fd, Requestp, proxy_request);
    //check cache
    cache_hit = cache_check(Requestp);
    if(cache_hit){      /* cache hit */
        //send cached response and update cache
        send_cached_response(client_fd, cache_hit);
    }else{              /* cache miss */
        server_fd = Open_clientfd(Requestp->host, Requestp->port);
        // forward request to server
        Rio_writen(server_fd, proxy_request, strlen(proxy_request));
        // forward response to client and insert cache
        send_response(server_fd, client_fd, Requestp);
        Close(server_fd);
    }
    Close(client_fd);
    free(Requestp);
    //print_cache();
    return NULL;
}
// read request from client and build proxy request(to server)
void read_request(int fd, Request *req, char *pr){
    rio_t client_rio;
    char buf[MAXLINE];
    char host_port[20];
    strcpy(host_port, "");
    // read and parse request line
    Rio_readinitb(&client_rio, fd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    parse_line(req, buf);
    memset(buf, 0, sizeof(buf));
    // read and cnstruct proxy request
    strcpy(pr, req->method);
    strcat(pr, " ");
    strcat(pr, req->content);
    strcat(pr, " ");
    strcat(pr, version);
    // add headers
    // if host was declared in request line
    if(strlen(req->host)){
        strcat(pr, "Host: ");
        strcat(pr, req->host);
        strcat(pr, ":");
        strcat(pr, req->port);
        strcat(pr, "\r\n");
    }
    strcat(pr, user_agent_hdr);
    strcat(pr, connection_hdr);
    strcat(pr, proxy_connection_hdr);
    //add other headers 
    while(Rio_readlineb(&client_rio, buf, MAXLINE)>0){
        if(strstr(buf, "\r\n")){
            break;
        }else if(strstr(buf, "User-Agent:") || strstr(buf, "Connection:") || strstr(buf, "Proxy-Connection:")){
        }else if(strstr(buf, "Host")){
            // if host is given by header
            if(!strlen(req->host)){
                sscanf(buf, "Host: %s", host_port);
                set_host_port(host_port, req->host, req->port);
                // strcpy(req->protocol, protocol);
                strcpy(req->host, host_port);
                strcat(pr, "Host: ");
                strcat(pr, req->host);
                strcat(pr, ":");
                strcat(pr, req->port);
                strcat(pr, "\r\n");
            }
        }else{
            // other additional headers
            strcat(pr, buf);
        }
        memset(buf, 0, sizeof(buf));
    }
    strcat(pr, "\r\n");
    return;
}
// parse request line and set request characteristics
void parse_line(Request *req, char *buf){
    char host_port[220];
    sscanf(buf, "%s %s %s \n", req->method, req->url, req->version);
    //method check
    if(strcmp(req->method, "GET") != 0){
        printf("ERROR(parse_line): Proxy does not implement the method\n");
    }
    //parsing url
    strcpy(req->content, "/");
    if(strstr(req->url, "://")){
        sscanf(req->url, "%[^:]://%[^/]%s", req->protocol, host_port, req->content);
    }else{
        sscanf(req->url, "%[^/]%s", host_port, req->content);
    }
    set_host_port(host_port, req->host, req->port);

    //print_request(req);
    return;
}
// separate host and post from host:port
void set_host_port(char* hp, char *h, char *p){
    char *tmp = strstr(hp, ":");
    if(tmp){
        *tmp = '\0';
        strcpy(p, tmp+1);
    }else{
        //default port (80)
        strcpy(p, "80");
    }
    strcpy(h, hp);
    return;
}
// printing requests for debugging
void print_request(Request *req){
    printf("-----------printing parsed request----------\n");
    printf("method: %s\n", req->method);
    printf("uri: %s\n", req->url);
    printf("protocol: %s\n", req->protocol);
    printf("host: %s\n", req->host);
    printf("port: %s\n", req->port);
    printf("content: %s\n", req->content);
    printf("version: %s\n", req->version);
    printf("----------------done parsing!----------------\n");
}
//send response to the client
void send_response(int server_fd, int client_fd, Request *req){
    rio_t server_rio;
    char buf[MAXLINE];
    char data[MAX_OBJECT_SIZE];
    unsigned int data_size = 0;
    unsigned int size;
    // read request and send to client
    Rio_readinitb(&server_rio, server_fd);
    while((size = Rio_readnb(&server_rio, buf, MAXLINE)) > 0){
        // forward to client
        Rio_writen(client_fd, buf, size);
        // accumulate to data only if response do not exceed MAX_OBJECT_SIZE
        if(data_size + size < MAX_OBJECT_SIZE){
            if(data_size){
                strncat(data, buf, size);
            }else{
                xstrncpy(data, buf, size);
            }
        }
        data_size += size;
    }
    // cache if it's smaller than MAX_OBJECT_SIZE
    if(data_size < MAX_OBJECT_SIZE){
        insert_item(req, data, data_size);
    }
    return;
}

// Cache Functions
// initialize cache
void init_cache(){
    cache.size = 0;
    cache.head = malloc(sizeof(struct CacheItem));
    CacheItem *item = cache.head;
    strcpy(item->host, "");
    strcpy(item->content, "");
    item->size = 0;
    item->data = NULL;
    item->next = NULL;
    item->prev = NULL;
    item->time = clock();
    cache.tail = item;
    return;
}
// check cache hit for the request
CacheItem *cache_check(Request *req){
    CacheItem *cur = cache.head;
    while(cur != NULL){
        // Cache hit!
        if((strcmp(cur->host, req->host)==0) && (strcmp(cur->content, req->content)==0)){
            break;
        }
        cur = cur->next;
    }
    return cur;
}
// access cached response and update cache
void send_cached_response(int fd, CacheItem *item){
    Rio_writen(fd, item->data, item->size);
    // update access time and location for the request
    cache_update(item);
    return;
}
// change location in cachelist
void cache_update(CacheItem *item){
    item->time = clock();
    CacheItem *cur = item;
    if(cache.tail == item) return;
    while((cur->next != NULL) && ((cur->next)->time <= item->time)){
        cur = cur->next;
    }
    //insert between cur and cur_next
    item->prev->next = item->next;
    item->next->prev = item->prev;
    item->next = cur->next;
    cur->next = item;
    item->prev = cur;
    if(cur->next == NULL){
        cache.tail = item;
    }else{
        cur->next->prev = item;
    }
    return;
}
// insert new item
void insert_item(Request *req, char *data, int size){
    char *new_data = malloc(sizeof(char)*size);
    strcpy(new_data, data);
    // evict until there exist enough space
    while(cache.size + size > MAX_CACHE_SIZE){
        evict_LRU();
    }
    // generate new item
    CacheItem *item = malloc(sizeof(struct CacheItem));
    item->time = clock();
    strcpy(item->host, req->host);
    strcpy(item->content, req->content);
    item->size = size;
    item->data = new_data;
    // insert new item
    cache.tail->next = item;
    item->prev = cache.tail;
    item->next = NULL;
    // update cache info
    cache.tail = item;
    cache.size += size;
    return;
}
// evict LRU element (front element)
void evict_LRU(){
    if(cache.tail == cache.head){
        printf("EROR(evict_LRU): Cannot evict from empty cache.");
        return;
    }
    delete_item(cache.head->next);
    return;
}
// delete item from the list
void delete_item(CacheItem *item){
    item->prev->next = item->next;
    if(item == cache.tail){
        cache.tail = item->prev;
    }else{
        item->next->prev = item->prev;
    }
    cache.size -= item->size;
    free(item->data);
    free(item);
    return;
}
// safe strcpy
char* xstrncpy(char * dst, const char *src, size_t n){
    dst[0] = '\0';
    return strncat(dst, src, n);
}
// printing cache for debugging
void print_cache(){
    printf("--------------printinf cache contents--------------\n");
    CacheItem *cur = cache.head;
    while(cur != NULL){
        printf("cache content: host(%s), content(%s)\n", cur->host, cur->content);
        cur = cur->next;
    }
    return;
}