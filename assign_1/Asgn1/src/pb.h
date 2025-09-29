#ifndef MYPUTC
#define MYPUTC

#ifndef PBSIZE
#define PBSIZE 1024
#endif

struct pbuff {
  int fd;                       /* where to write() */
  int idx;                      /* where are we in this thing */
  char buff[PBSIZE];            /* the buffer */
};

void pbreset(struct pbuff *pb);
void pbputc(int c, struct pbuff *pb);
void pbputs(const char *s, struct pbuff *pb);
void pbflush(struct pbuff *pb);

#endif
