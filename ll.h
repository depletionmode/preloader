
#ifndef LL_H_
#define LL_H_

struct lnode {
  void *p;
  struct lnode *n;
};

typedef struct ll {
  struct lnode *f;
} LL;

typedef struct llit {
  struct lnode *at;
  LL *ll;
} LLIT;

LL *ll_calloc();
void ll_free(LL *ll);
void ll_add(LL *ll, void *p);
void *ll_iterate(LL *ll, LLIT *i);
int ll_size(LL *ll);
void *ll_access(LL *ll, int member);  /* random access - grossly inefficient! */

#endif /* LL_H_ */
