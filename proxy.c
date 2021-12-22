#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);

int main(int argc,char **argv)
{
    int listenfd,connfd;
    socklen_t  clientlen;
    char hostname[MAXLINE],port[MAXLINE];

    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]); // 프록시에 듣기소켓을 만드는 역할
    while(1){
        clientlen = sizeof(clientaddr); // 클라이언트에서 보내는 주소의 length 설정
        connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen); // 

        /*print accepted message*/
        Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0); // 프린트문 출력하기 위해 정보 가져오는 구문
        printf("Accepted connection from (%s %s).\n",hostname,port);

        /*sequential handle the client transaction*/
        doit(connfd);

        Close(connfd);
    }
    return 0;
}


/*handle the client HTTP transaction*/
void doit(int connfd) // connfd를 파라미터로 받아서 실행
{
    int end_serverfd;/*the end server file descriptor*/

    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char endserver_http_header [MAXLINE];
    /*store the request line arguments*/
    char hostname[MAXLINE],path[MAXLINE];
    int port;

    rio_t rio,server_rio;/*rio is client's rio,server_rio is endserver's rio*/
    // rio 는 클라이언트와 프록시 사이의 버퍼, server_rio는 프록시와 서버사이의 버퍼
    Rio_readinitb(&rio,connfd); // rio와 connfd와 연결
    Rio_readlineb(&rio,buf,MAXLINE); 
    sscanf(buf,"%s %s %s",method,uri,version); /*read the client request line*/

    if(strcasecmp(method,"GET")){
        printf("Proxy does not implement the method");
        return;
    }
    /*parse the uri to get hostname,file path ,port*/
    parse_uri(uri,hostname,path,&port); // hostname, port값을 모르는채로 호출 -> parse_uri 함수 호출하고 값 부여
    // parse_uri 함수 반환이 끝나면 hostname, path, &port의 값은 다 선언되있음.

    // endserver(tiny.c)의 http 헤더 생성 구문
    build_http_header(endserver_http_header,hostname,path,port,&rio);


    /*connect to the end server*/
    // 프록시 입장에서 tiny.c에 클라이언트 역할을 하기 위한 fd 생성
    end_serverfd = connect_endServer(hostname,port,endserver_http_header);

    if(end_serverfd<0){
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&server_rio,end_serverfd);
    /*write the http header to endserver*/
    // 프록시에서 서버 연결하는 파트
    
    // endserver_http_header를 end_srverfd에 저장 
    Rio_writen(end_serverfd,endserver_http_header,strlen(endserver_http_header));

    /*receive message from end server and send to the client*/
    size_t n;
    while((n=Rio_readlineb(&server_rio,buf,MAXLINE))!=0)
    {
        printf("proxy received %d bytes,then send\n",n);
        Rio_writen(connfd,buf,n); // 서버에서 받아온 값 다받오기
    }
    Close(end_serverfd);  // 종료
}

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio)
{
    char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];
    /*request line*/
    sprintf(request_hdr,requestlint_hdr_format,path);   // request_hdr는 빈 그릇, 가운대는 매크로로 이미 선언, path는 가져왔음
    /*get other request header for client rio and change it */
    while(Rio_readlineb(client_rio,buf,MAXLINE)>0)
    {
        if(strcmp(buf,endof_hdr)==0) break;/*EOF*/
        // endof_hdr = /r/o -> 엔터2개 만나면 종료.(종료조건)

        if(!strncasecmp(buf,host_key,strlen(host_key)))/*Host:*/
        {
            strcpy(host_hdr,buf);
            continue;
        }

        if(!strncasecmp(buf,connection_key,strlen(connection_key))
                &&!strncasecmp(buf,proxy_connection_key,strlen(proxy_connection_key))
                &&!strncasecmp(buf,user_agent_key,strlen(user_agent_key)))
        {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr)==0)
    {
        sprintf(host_hdr,host_hdr_format,hostname);
    }
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return ;
}

/*Connect to the end server*/
inline int connect_endServer(char *hostname,int port,char *http_header){
    char portStr[100];
    sprintf(portStr,"%d",port);
    return Open_clientfd(hostname,portStr);
}

/*parse the uri to get hostname,file path ,port*/
void parse_uri(char *uri,char *hostname,char *path,int *port)
{
    *port = 80; // 포트 80으로 디폴트 부여
    char* pos = strstr(uri,"//"); // 포인터 생성(ex : http://localhost:5000 에서 // 문구의 포인터 찾는 구문)

    pos = pos!=NULL? pos+2:uri; // pos가 널이아니면 pos 포인터값 + 2 pos = l

    char*pos2 = strstr(pos,":");  // pos2가 ':'인 경우
    if(pos2!=NULL) // 포트를 입력했음을 의미
    {
        *pos2 = '\0'; // localhost\0 5000/home.html -> 문자열 읽을 때 끊어주는 역할(\0 만나면 끊기때문)
        sscanf(pos,"%s",hostname);  // pos = localhost
        sscanf(pos2+1,"%d%s",port,path);  // port = 5000, path = home.html
    }
    else    // 포트값이 선언이 안됐기 때문에 위에서 선언한 디폴트 포트값 사용
    {
        pos2 = strstr(pos,"/");
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}
