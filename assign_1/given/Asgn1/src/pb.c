/* provide buffered printing for pp without using stdio.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pb.h"

void pbreset(struct pbuff *pb) {
  /* reset the given buffer to empty */
  pb->idx=0;
}

void pbputc(int c, struct pbuff *pb) {
  /* add a character to the given buffer, flushing first if it's full */
  if ( pb->idx == PBSIZE )      /* we be full */
    pbflush(pb);
  pb->buff[pb->idx++] = c;
}

void pbflush(struct pbuff *pb) {
  /* flush the given buffer */
  if ( -1 == write(pb->fd, pb->buff, pb->idx) ) {
    perror("pbflush:write");
    exit(EXIT_FAILURE);         /* something went terribly wrong */
  }
  pbreset(pb);
}

void pbputs(const char *s, struct pbuff *pb) {
  /* write the given string to the given buffer */
  while (*s)
    pbputc(*s++,pb);
}

#ifdef  TESTMAIN
int main(int argc, char *argv[]) {
  struct pbuff pb;

  pbreset(&pb);
  pb.fd = STDERR_FILENO;

  pbputs("Hello, world!\n", &pb);
  pbputs("Hello, again!\n", &pb);

  pbflush(&pb);

  return 0;
}
#endif
