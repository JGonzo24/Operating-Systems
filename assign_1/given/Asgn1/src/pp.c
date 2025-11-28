#include<stdarg.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<ctype.h>
#include "pb.h"

static const char *dtos(long num, int us, int radix) {
  /* convert to a string in the given radix, signed or unsigned
   * depending on us */
  #define SIZE (sizeof(long)*8 + 2) /* Big enough for binary */
  const char digit[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static char b[SIZE], *s;
  int neg;
  unsigned long pos;             /* be positive */

  if ( radix < 2 || radix > 36 )
    return "bad radix";

  if ( num == 0 )
    return "0";

  /* this arithmetic only works on non-negative numbers */
  neg = (num < 0) && !us;
  if ( neg )
    pos = -num;
  else
    pos = num;

  s = b+SIZE-1;
  *s = '\0';
  while (pos) {
    *--s=digit[pos%radix];
    pos /= radix;
  }
  if ( neg )
    *--s='-';

  return s;
}

static void padputs(const char *s, int wid, int zpad, struct pbuff *pb){
  /* print the given string right justified in a field wid wide */
  int i;
  for( i = wid - strlen(s); i>0; i--)
    pbputc(zpad?'0':' ',pb);
  pbputs(s,pb);
}

void pp(FILE *where, char *fmt,...) {
  /* will fail for max negative number.  Oh, well */
  va_list ap;
  long l;
  char c;
  int pad=0,zpad=0;
  char *s;
  struct pbuff pb;

  fflush(NULL);                 /* clear all stdio buffers */
  pbreset(&pb);                 /* set up our local buffer */
  pb.fd = fileno(where);

  va_start(ap,fmt);
  for(s=fmt;*s;s++) {
    pad=zpad=0;
    if ( *s == '%' ) {
      l=0;
      if ( (c = *++s) == 'l' ) {  /* check for long */
        l = 1;
        c = *++s;
      }
      if ( isdigit(*s) ) {  /* check for field width */
        if ( *s == '0' )
          zpad=1;
        pad = strtol(s,&s,10);/* translate number and advance to just past */
        c = *s;               /* if it's the nul, it'll be caught below */
      }
      switch (c) {
      case '%':
        pbputc(*s,&pb);
        break;
      case 'c':
        pbputc(va_arg(ap,int), &pb);
        break;
      case 'd':
        if ( l )
          padputs(dtos(va_arg(ap,long), 0, 10), pad, zpad, &pb);
        else
          padputs(dtos(va_arg(ap,int), 0, 10),  pad, zpad, &pb);
        break;
      case 'p':
        pbputs("0x",&pb);
        l = 1;  /* pointers are longs */
      case 'x': /* fall through */
        if ( l )
          padputs(dtos(va_arg(ap,long), 1, 16), pad, zpad, &pb);
        else
          padputs(dtos(va_arg(ap,int), 1, 16), pad, zpad, &pb);
        break;
      case 's':
        padputs(va_arg(ap,char*), pad, zpad,&pb);
        break;
      default:
        pbputs("<Unknown conversion:",&pb);
        pbputc(*s,&pb);
        pbputs(">",&pb);
        break;
      }
    } else {
      pbputc(*s,&pb);
    }
  }
  va_end(ap);
  pbflush(&pb);  /* Flush our buffer */

}

