#include "license_pbs.h" /* See here for the software license */
#include <stdlib.h>
#include <stdio.h>

char *dis_umax = NULL;
unsigned dis_umaxd = 0;

void disiui_(void)
  {
  fprintf(stderr, "The call to disiui_ needs to be mocked!!\n");
  exit(1);
  }

int tcp_gets(int fd, char *str, size_t ct)
  {
  fprintf(stderr, "The call to tcp_gets needs to be mocked!!\n");
  exit(1);
  }

int tcp_getc(int fd)
  {
  fprintf(stderr, "The call to tcp_getc needs to be mocked!!\n");
  exit(1);
  }

