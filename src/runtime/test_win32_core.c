/*
 * win32_core_test.c -- the Win32 personality contract.
 *
 * Round-trips real text through both conversions, then refuses broken input
 * WITHOUT touching the destination: a partially converted filename that looks
 * valid is worse than an error.
 */

#include "win32_core.h"
#include <stdio.h>
#include <string.h>
static int f=0,c=0;
#define T(cond,msg) do{ c++; if(!(cond)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,msg); f++;} }while(0)
int main(void){
  /* round trip ASCII */
  const char *a="hello";
  uint16_t w[64]; char b[64];
  int n=Utf8ToWideChar(a,-1,w,64);
  T(n==5,"ascii wide len");
  int m=WideCharToUtf8(w,n,b,64);
  T(m==5 && strcmp(b,"hello")==0,"ascii round trip");
  /* BMP + supplementary */
  const char *u="a\xC3\xA9\xF0\x9F\x98\x80z";   /* aé😀z */
  n=Utf8ToWideChar(u,-1,w,64);
  T(n==5,"utf8->wide unit count (a=1, é=1, emoji=2, z=1)");
  m=WideCharToUtf8(w,n,b,64);
  T(m==(int)strlen(u) && memcmp(b,u,strlen(u))==0,"utf8 round trip bytes");
  /* lone surrogate refused, buffer untouched */
  uint16_t bad[2]={0xDC00,0};
  strcpy(b,"SENTINEL");
  T(WideCharToUtf8(bad,1,b,64)==-1,"lone low surrogate refused");
  T(GetLastError()==W32_ERROR_NO_UNICODE_TRANSLATION,"error code set");
  T(strcmp(b,"SENTINEL")==0,"buffer untouched on refusal");
  /* overlong refused */
  const unsigned char ov[]={0xC0,0xAF,0};
  T(Utf8ToWideChar((const char*)ov,2,w,64)==-1,"overlong utf8 refused");
  /* truncated multi-byte refused */
  const char tr[2]={(char)0xE2,0};
  T(Utf8ToWideChar(tr,2,w,64)==-1,"truncated utf8 refused");
  /* too-small buffer refused before writing */
  strcpy(b,"SENTINEL");
  T(WideCharToUtf8(w,n,b,2)==-1,"insufficient buffer refused");
  T(GetLastError()==W32_ERROR_INSUFFICIENT_BUFFER,"buffer error code");
  T(strcmp(b,"SENTINEL")==0,"small-buffer refusal leaves output alone");
  /* NULL args */
  T(WideCharToUtf8(NULL,1,b,64)==-1,"NULL input refused");
  T(Utf8ToWideChar(NULL,1,w,64)==-1,"NULL utf8 refused");
  /* last error thread-local */
  SetLastError(W32_ERROR_ACCESS_DENIED);
  T(GetLastError()==W32_ERROR_ACCESS_DENIED,"Set/GetLastError");
  SetLastError(0);
  T(GetLastError()==0,"last error cleared");
  /* status mapping */
  T(Win32FromNTStatus(0)==0,"STATUS_SUCCESS -> ERROR_SUCCESS");
  T(Win32FromNTStatus(0xC000000D)==W32_ERROR_INVALID_PARAMETER,"invalid parameter");
  T(Win32FromNTStatus(0xC0000008)==W32_ERROR_INVALID_HANDLE,"invalid handle");
  T(Win32FromNTStatus(0xC0000034)==W32_ERROR_FILE_NOT_FOUND,"not found -> FILE_NOT_FOUND");
  T(Win32FromNTStatus(0xC0000022)==W32_ERROR_ACCESS_DENIED,"access denied");
  T(Win32FromNTStatus(0xC0000023)==W32_ERROR_INSUFFICIENT_BUFFER,"too small");
  T(Win32FromNTStatus(0xC0000002)==W32_ERROR_INVALID_FUNCTION,"not implemented");
  T(Win32FromNTStatus(0xDEADBEEF)==W32_ERROR_INTERNAL_ERROR,"unknown status -> internal error");
  printf("=== win32_core: %d checks, %d failed ===\n",c,f);
  return f?1:0;
}
