
#include <stdio.h>

#include "ldd.h"

LIBS *get_libs(int fd)
{
  LIBS *f_l = NULL, *l = NULL;

  return l;
}

void free_libs(LIBS *l)
{
  LIBS *nxt;

  while( l ) {
    nxt = l->nxt;

    free(l->name);
    free(l->path);
    free(l);

    l = nxt;
  }
}
