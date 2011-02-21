
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "dynsym.h"

int main(int ac, char *av[])
{
  /* test code */
  int fd = open( av[1], O_RDONLY );

  DYNSYM *ds = get_dynsyms( fd );

  close( fd );

  while( ds ) {
      printf( "%s\n", ds->name );
      ds = ds->nxt;
  }

  free_dynsyms( ds );

  /* add target to DB */
  /* get dynamic symbols */
  /* add symbols to DB ) */
  /* (auto-resolve sigs and add to DB) */
  /* pull symbols from DB and show symbol list (pull on every refresh - inefficient but probably quick) */
  /* allow for selection of symbols */

  return 0;
}
