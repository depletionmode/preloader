
#ifndef DYNSYM_H_
#define DYNSYM_H_

typedef struct dynsym DYNSYM;
struct dynsym {
  char *name;
  DYNSYM *nxt;
};

#define DYNSYM_UNDEFINED_ONLY 1
#define DYNSYM_DEFINED_ONLY 2
#define DYNSYM_ALL 3

DYNSYM *get_dynsyms(int fd, int flags);
void free_dynsyms(DYNSYM *ds);

#endif /* DYNSYM_H_ */
