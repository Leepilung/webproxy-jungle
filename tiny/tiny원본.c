// tiny Web Server implement
// start command & path : in tiny folder (./tiny 8000)
// link : http://13.125.112.33:8000/

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

#include "csapp.h"

// 함수 Prototype 선언
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method); 
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// Main Function
int main(int argc, char **argv) { // argc : 옵션의 개수(arg count의 약어), argv : 옵션 문자열 배열
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  // 서버를 가동하고 돌리는데 있어서 인자(arg)를 2개 넣고 이를 검사(ex tiny 8000)
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 에러 출력 문구
    exit(1);
  }

  // Open_listenfd 호출해서 듣기 소켓. 인자(argv[1] = 포트번호)
  listenfd = Open_listenfd(argv[1]); 
  while (1) { 
    clientlen = sizeof(clientaddr); // Accept 함수에서 사용하기 위해 주소길이 계산

    // Accept 함수는 인자로 1. 듣기 식별자, 2. 소켓구조체의 주소 3. 주소(소켓구조체)의 길이를 인자로 받음
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // connectedfd 생성 문구 -> doit의 인자로 사용하기 위함
    
    // Getaddrinfo 함수는 호스트(서버) 이름: 호스트 주소, 서비스 이름: 포트 번호 의 스트링 표시를 소켓 주소 구조체로 변환 (모든 프로토콜에 대해)
    // Getnameinfo 함수는 addrinfo와 반대로 소켓 주소 구조체에서 스트링 표시로 변환한다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);   // accept 구문 출력을 위한 정보 가져오기 위한 함수
    printf("Accepted connection from (%s, %s)\n", hostname, port);  
    doit(connfd);   // line:netp:tiny:doit  doit 실행
    Close(connfd);  // line:netp:tiny:close 클라이언트쪽 소켓 닫음
  }
}

// 1개의 HTTP 트랜잭션을 처리
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;  // rio 구조체 선언

  // rio와 fd 연결시키는 구문(init)
  Rio_readinitb(&rio, fd);  // 위에서 connfd 받은것을 fd로 입력받아 rio와 연결
  // request line, header 읽는 구문
  Rio_readlineb(&rio, buf, MAXLINE); // rio에 있는 string을 buf(버퍼)에 옮겨 적는다.

  printf("Request headers:\n");
  printf("%s", buf);

  // buf에서 공백문자로 구분된 문자열 3개 읽어 각자 method, uri, version에 저장
  sscanf(buf, "%s %s %s", method, uri, version);

  // GET 요청 or HEAD 요청인지 아닌지 확인하는 구문
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny doesn't implement this method");
    return;
  }

  // 요청 헤더는 무시
  read_requesthdrs(&rio);

  // uri를 잘라 uri, filename, cgiargs로 나눈다.
  is_static = parse_uri(uri, filename, cgiargs);  // URI를 파일 이름과 비어있을 지도 모르는 CGI 인자 스트링 분석 하고, 정적이면 1 반환 // 파일 네임을 parse_uri를 통해 만듦.
  // stat(파일 명 or 파일 상대/절대 경로, 파일의 상태 및 정보를 저장할 buf 구조체)
  if (stat(filename, &sbuf) < 0) // request가 정적 or 동적 컨텐츠를 위한 것인지를 나타내는 플래그 구문. // &subf에 파일네임 넘긴다.
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 정적 컨텐츠인 경우
  if (is_static)
  { /*Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))  // 해당 파일이 보통 파일인지, 읽기 권한을 갖고 있는 파일인지 검증 -> 맞으면 1 , 틀리면 0 반환
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method); // 정적 컨텐츠를 클라이언트에게 제공
  }
  else  // 동적 컨텐츠인 경우
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

// HTTP 응답을 응답 라인에 적절한 상태 코드와 상태 메시지와 함께 클라이언트에 보내는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  /* HTTP response headers 출력 */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // buf를 fd에 (위에 클라에서 준 걸 썼으니까 다시 파일 식별자로 보내서 돌려줌)
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));

  /* HTTP response body 출력 */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
}

// readlineb에서 요청라인을 한번 읽는데 -> 이땐 출력을안함. (doit 함수에서 GET 구분할 때 사용)
void read_requesthdrs(rio_t *rp) // 헤더만 뽑아낼 때 rp 사용
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);  // 헤더를 읽고 첫 줄을 빼버린다(요청 라인 제외)
  // 그 다음 while문 들어가서 전부 출력함.
  // strcmp = string compare (문자열 비교) 함수 두 문자열을 비교.
  // 헤더의 마지막 줄은 비어있기 때문에 \r\n만 buf(버퍼)에 담겨있다면, while문 조건식에 걸려서 탈출.
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    // 멈춘 지점까지 출력하고 다시 while문 반복함
  }
  return;
}

// parse_uri 
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // uri에 cgi-bin 경로가 없다는 것은 정적 컨텐츠를 요청한다는 의미이므로 1을 반환함.
  // strstr 함수 -> (대상문자열, 검색할문자열) : 문자열을 찾았으면 문자열로 시작하는 문자열의 포인터를 반환한다.
  if (!strstr(uri, "cgi-bin")) // 정적 커텐츠에 해당한다면
  {                      
    strcpy(cgiargs, "");       //uri에 cgi-bin과 일치하는 문자열이 없다면 cgiargs에는 빈 문자열을 저장
    strcpy(filename, ".");     //아래 줄과 더불어 상대 리눅스 경로이름으로 변환(./index.html과 같은 )
    strcat(filename, uri);     //filename에 uri를 이어 붙인다.
    // 최종 결과 cigargs = ""(공백 문자열), filename = "./~~ or ./home.html"

    // other Example --- uri : /godzilla.jpg -> cgiagrs : filenmae : ./godzilla.jpg

    if (uri[strlen(uri) - 1] == '/')  // uri가 '/' 문자로 끝난다면
      strcat(filename, "home.html");  // 기본 파일 이름(ex home.html)을 filename에 추가한다. -> 해당 이름의 정적컨텐츠가 출력된다. 
    return 1; // 정적 컨텐츠임을 반환.
  }

  // 동적 컨텐츠의 경우
  else 
  { 
    ptr = index(uri, '?');  // 모든 CGI 인자를 추출한다.
    // index 함수  문자열에서 특정 문자의 위치를 반환한다.
    
    if (ptr) // 만약 '?'가 존재하면 ptr은 0이아닌 숫자가 반환되므로 if문 조건 충족
    { 
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';  // ptr 초기화
    }
    else
    {
      strcpy(cgiargs, "");  // 없으면 안넣음
    }
    strcpy(filename, ".");  // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환한다.
    strcat(filename, uri);  // 이어 붙이는 함수. 파일 네임에 uri를 붙인다.

    /* Example -> uri : /cgi-bin/adder?132&132
    ->
    cgiargs : 132&132
    filename : ./cgi-bin/adder
    */

    return 0; // 동적 컨텐츠임을 반환.
  }
}

// 정적 파일의 경우
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //client에게 response header
  get_filetype(filename, filetype);                             // 파일 이름의 접미어를 확인하여 파일 타입 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                          // 클라이언트에 응답 줄과 응답 헤더 출력 -> 버퍼에 넣고 출력.
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);           // 기본 구문
  sprintf(buf, "%sConnection: close\r\n", buf);                 // 기본 구문
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);      // 기본 구문
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);    // 기본 구문
  
  // writen = client(TELNET)에서 출력.
  Rio_writen(fd, buf, strlen(buf));                             // 요청한 파일의 내용을 연결 식별자 fd로 복사해서 응답 본체를 보냄.(이후에 버퍼 이동)
  
  printf("Response headers:\n");                                
  printf("%s", buf);

  if (!strcasecmp(method, "HEAD"))
    return;

  // O_RDONLY -> 파일을 읽기 전용으로 열기 <-> O_WRONLY(쓰기 전용으로 열기), 둘 합치면 O_RDWR
  srcfd = Open(filename, O_RDONLY, 0);  // 열려고 하는 파일의 식별자 번호 리턴. filename을 오픈하고 식별자를 얻어온다.
                                        // 마지막 인자 0 -> 읽기 전용이기도하고, 새로 파일을 만드는게 아니라 NUll값처럼 없다는 의미.
  // Mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
  // mmap함수는 요청한 파일을 가상메모리 영역으로 매핑함
  
  // fd로 지정된 파일에서 offset을 시작으로 length바이트 만큼 start주소로 대응시키도록 한다.
  // start주소는 단지 그 주소를 사용했으면 좋겠다는 정도로 보통 0을 지정한다.
  // mmap는 지정된 영역이 대응된 실제 시작위치를 반환한다.
  // prot 인자는 원하는 메모리 보호모드(:12)를 설정한다
  // -> PROT_EXEC - 실행가능, PROT_READ - 읽기 가능, NONE - 접근 불가, WRITE - 쓰기 가능
  // flags는 대응된 객체의 타입, 대응 옵션, 대응된 페이지 복사본에 대한 수정이 그 프로세스에서만 보일 것인지, 다른 참조하는 프로세스와 공유할 것인지 설정
  // MAP_FIXED - 지정한 주소만 사용, 사용 못한다면 실패 / MAP_SHARED - 대응된 객체를 다른 모든 프로세스와 공유 / MAP_PRIVATE - 다른 프로세스와 대응 영역 공유하지 않음

  // 11.9 숙제 파트 부분.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //srcfd의 첫 번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑함
  srcp = malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); //파일을 클라이언트에게 전송 -> 주소 srcp에서 시작하는 filesize 바이트를 클라이언트의 연결 식별자로 복사함.
  free(srcp);
  //Munmap(srcp, filesize);         //매핑된 가상메모리 주소를 반환
}


// 파일의 타입을 이름에서 찾아서 화인하는 함수
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
  else if (strstr(filename, ".mp4"))  // 과제 11.7 tiny에 동영상 파일 처리 검증 구문
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};
  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf)); // fd에 버퍼 넣는다
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (!strcasecmp(method, "HEAD")) // 같으면 0(false) 들어가고 끝냄(HEAD가 맞으면)
    return; // void 타입이라 바로 리턴

  if (Fork() == 0)
  { /* 여기는 자식 프로세스 로직 */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // 환경변수
    /* Redirect stdout to client */
    Dup2(fd, STDOUT_FILENO);  // Fd의 내용을 표준입출력에 넣는다는 뜻
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* 부모가 자식을 기다려야 함. */
}
