/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

// 함수 원형 선언
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method); 
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

//
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  // 서버를 가동하고 돌리는데 있어서 인자(arg)를 2개 넣는다.(ex tiny 8000)
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 에러 출력 문구
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);  // 듣기 소켓 (포트번호(argv[1]))
  while (1) { 
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // connectedfd 생성 문구 -> doit의 인자로 사용하기 위함
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);   // accept 구문 출력을 위한 정보 가져오기 위한 함수
    printf("Accepted connection from (%s, %s)\n", hostname, port);  
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
  
}



void doit(int fd)
{
  int is_static;
  struct stat sbuf;

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // rio와 fd 연결시키는 구문(init)
  Rio_readinitb(&rio, fd);  // 위에서 connfd 받은것을 fd로 입력
  // request line, header 읽는 구문
  Rio_readlineb(&rio, buf, MAXLINE); // buf에 옮겨 적는다.

  printf("Request headers:\n");
  printf("%s", buf);

  // buf에서 공백문자로 구분된 문자열 3개 읽어 각자 method, uri, version에 저장
  sscanf(buf, "%s %s %s", method, uri, version);

  // GET 요청인지 아닌지 확인하는 구문
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) 
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny doesn't implement this method");
    return;
  }

  // 요청 헤더는 무시
  read_requesthdrs(&rio);

  // uri를 잘라 uri, filename, cgiargs로 나눈다.
  is_static = parse_uri(uri, filename, cgiargs);
  // stat(파일 명 or 파일 상대/절대 경로, 파일의 상태 및 정보를 저장할 buf 구조체)
  if (stat(filename, &sbuf) < 0) // &sbuf -> ???
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (!strcasecmp(method, "HEAD"))
  {
    response_header(fd, filename, sbuf.st_mode);
  }

  // 정적 컨텐츠인 경우
  if (is_static)
  { /*Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method); // 정적 컨텐츠를 클라이언트에게 제공
  }
  else
  {
    /* Serve dynamic content*/
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method); // 동적 컨텐츠를 클라이언트에게 제공
  }
}

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE];
  /* HTTP response headers 출력 */
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /* HTTP response body 출력 */
  sprintf(buf, "<html><title>Tiny Error</title>");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<body bgcolor="
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

// readlineb에서 요청라인을 한번 읽는데 -> 이땐 출력을안함. 
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  {                      //static content
    strcpy(cgiargs, ""); //uri에 cgi-bin과 일치하는 문자열이 없다면 cgiargs에는 빈 문자열을 저장

    strcpy(filename, "."); //아래 줄과 더불어 상대 리눅스 경로이름으로 변환(./index.html과 같은 )
    strcat(filename, uri);

    if (uri[strlen(uri) - 1] == '/') //uri가 / 문자로 끝난다면 기본 파일 이름 추가 -> /로 안끝나는 경우가 있나?
      strcat(filename, "home.html");
    return 1;
  }
  else
  { //dynamic content
    ptr = index(uri, '?');
    if (ptr)
    { //cgi 인자 추출
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
    {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_static_origin(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //client에게 response header 보내기
  get_filetype(filename, filetype); 

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // fd = connfd에 해당.
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") == 0)
    return

  //O_RDONLY -> 파일을 읽기 전용으로 열기 <-> O_WRONLY, 둘 합치면 O_RDWR
  srcfd = Open(filename, O_RDONLY, 0);
  //mmap는 요청한 파일을 가상메모리 영역으로 매핑함
  //Mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
  //fd로 지정된 파일에서 offset을 시작으로 length바이트 만큼 start주소로 대응시키도록 한다.
  //start주소는 단지 그 주소를 사용했으면 좋겠다는 정도로 보통 0을 지정한다.
  //mmap는 지정된 영역이 대응된 실제 시작위치를 반환한다.
  //prot 인자는 원하는 메모리 보호모드(:12)를 설정한다
  //-> PROT_EXEC - 실행가능, PROT_READ - 읽기 가능, NONE - 접근 불가, WRITE - 쓰기 가능
  //flags는 대응된 객체의 타입, 대응 옵션, 대응된 페이지 복사본에 대한 수정이 그 프로세스에서만 보일 것인지, 다른 참조하는 프로세스와 공유할 것인지 설정
  //MAP_FIXED - 지정한 주소만 사용, 사용 못한다면 실패 / MAP_SHARED - 대응된 객체를 다른 모든 프로세스와 공유 / MAP_PRIVATE - 다른 프로세스와 대응 영역 공유하지 않음

  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //srcfd의 첫 번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑함
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); //파일을 클라이언트에게 전송 -> 주소 srcp에서 시작하는 filesize 바이트를 클라이언트의 연결 식별자로 복사함.
  Munmap(srcp, filesize);         //매핑된 가상메모리 주소를 반환
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //client에게 response header 보내기
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") == 0)
    return

  //O_RDONLY -> 파일을 읽기 전용으로 열기 <-> O_WRONLY, 둘 합치면 O_RDWR
  srcfd = Open(filename, O_RDONLY, 0);
  //mmap는 요청한 파일을 가상메모리 영역으로 매핑함
  //Mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
  //fd로 지정된 파일에서 offset을 시작으로 length바이트 만큼 start주소로 대응시키도록 한다.
  //start주소는 단지 그 주소를 사용했으면 좋겠다는 정도로 보통 0을 지정한다.
  //mmap는 지정된 영역이 대응된 실제 시작위치를 반환한다.
  //prot 인자는 원하는 메모리 보호모드(:12)를 설정한다
  //-> PROT_EXEC - 실행가능, PROT_READ - 읽기 가능, NONE - 접근 불가, WRITE - 쓰기 가능
  //flags는 대응된 객체의 타입, 대응 옵션, 대응된 페이지 복사본에 대한 수정이 그 프로세스에서만 보일 것인지, 다른 참조하는 프로세스와 공유할 것인지 설정
  //MAP_FIXED - 지정한 주소만 사용, 사용 못한다면 실패 / MAP_SHARED - 대응된 객체를 다른 모든 프로세스와 공유 / MAP_PRIVATE - 다른 프로세스와 대응 영역 공유하지 않음

  // 11.9 숙제 파트 부분.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //srcfd의 첫 번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑함
  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); //파일을 클라이언트에게 전송 -> 주소 srcp에서 시작하는 filesize 바이트를 클라이언트의 연결 식별자로 복사함.
  free(srcp);
  srcp = 0;
  //Munmap(srcp, filesize);         //매핑된 가상메모리 주소를 반환
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};
  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  if (Fork() == 0)
  { /* 여기는 자식 프로세스 로직 */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // 환경변수
    setenv("REQUEST_METHOD", method, 1);
    /* Redirect stdout to client */
    Dup2(fd, STDOUT_FILENO);  // Fd의 내용을 표준입출력에 넣는다는 뜻
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* 부모가 자식을 기다려야 함. */
}

void response_header(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //client에게 response header 보내기
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
}