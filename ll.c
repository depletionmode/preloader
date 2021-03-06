
#include <malloc.h>
#include <string.h>

#include "ll.h"

/* this is completely and totally NOT threadsafe! */

#define N_FREE(X) \
  do {            \
    if( X ) {     \
      free( X );  \
      X = NULL;   \
    }             \
  } while( 0 )

struct lnode *_get_last_node(LL *ll)
{
  struct lnode *n = ll->f;

  if( n ) while( n->n ) n = n->n;

  return n;
}

LL *ll_calloc()
{
  LL *ll = calloc(1, sizeof( struct lnode ) );
  return ll;
}

void ll_free(LL *ll)
{
  struct lnode *n = NULL;
//TODO
  N_FREE(n);
}

void ll_add(LL *ll, void *p)
{
  struct lnode *l = _get_last_node( ll );
  struct lnode *n = calloc( 1, sizeof( struct lnode ) );

  n->p = p;
  if( l ) l->n = n;
  else ll->f = n;
}

void *ll_iterate(LL *ll, LLIT *i)
{
  if( !i->ll ) {
    i->ll = ll;
    i->at = ll->f;
  }

  if( i->at ) {
    void *p = i->at->p;
    i->at = i->at->n;
    return p;
  }

  return NULL;
}

int ll_size(LL *ll)
{
  int c = 0;

  LLIT i;
  memset( &i, 0, sizeof( LLIT ) );
  while( ll_iterate( ll, &i ) ) c++;

  return c;
}

void *ll_access(LL *ll, int member)
{
  int c = 0;
  void *p = NULL;

  LLIT i;
  memset( &i, 0, sizeof( LLIT ) );
  while( ( p = ll_iterate( ll, &i ) ) ) if( c++ == member ) break;

  return p;
}
