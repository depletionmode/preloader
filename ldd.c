
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldd.h"

char *_get_name(char *buf, int *len)
{
  static char tmp[50];
  memset( tmp, 0, sizeof( tmp ) );

  memcpy( tmp, buf + 1, strcspn( buf, " " ) - 1 );

  *len = strlen( tmp );
  return tmp;
}

char *_get_path(char *buf, int *len)
{
  static char tmp[50];
  memset( tmp, 0, sizeof( tmp ) );

  memcpy( tmp,
          buf + strcspn( buf, " " ) + 4,
          strcspn( buf, "(" ) - strcspn( buf, " " ) - 5 );

  *len = strlen( tmp );
  return tmp;
}

LIBS *get_libs(char *path)
{
  LIBS *f_l = NULL, *l = NULL;

  char cmd[200];
  sprintf( cmd, "ldd %s", path);
  FILE *pf = popen( cmd, "r" );
  if( !pf )
    return NULL;

  char buf[1000];
  while( fgets( buf, sizeof( buf ), pf ) ) {
    /* ignore linux dynamic linkers (these will always be linked in */
    if( strstr( buf, "linux-vdso" ) ||
        strstr( buf, "linux-gate" ) ||
        strstr( buf, "ld-linux" ) )
      continue;

    LIBS *n_l = calloc( 1, sizeof( LIBS ) );

    int len;

    char *ptr = _get_name( buf, &len );
    n_l->name = malloc( len + 1 );
    strcpy( n_l->name, ptr );

    ptr = _get_path( buf, &len );
    n_l->path = malloc( len + 1 );
    strcpy( n_l->path, ptr);

    if( !f_l ) f_l = n_l;
    else l->nxt = n_l;

    l = n_l;
  }

  pclose( pf );

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
