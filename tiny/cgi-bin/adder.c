/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';

    // strcpy => 문자열 복사 함수
    strcpy(arg1, buf);
    strcpy(arg2, p + 1);

    // html형식에서도 숫자를 받아 처리 할 수 있게끔 변경하기 위한 구문
    // strchr() 함수는 스트링에서 문자의 첫번째 표식('=')를 찾는다.
    // strchr() 함수는 string의 문자로 변환되는 c의 첫 번째 표시에 대한 포인터를 리턴한다.
    if (strchr(arg1, '=')) 
    {
    p = strchr(arg1, '=');
    *p = '\0';
    strcpy(arg1, p + 1);

    p = strchr(arg2, '=');
    *p = '\0';
    strcpy(arg2, p + 1);
    }

    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
          content, n1, n2, n1 + n2);
  // sprintf(content, "arg1 : %s", arg1);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */