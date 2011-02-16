
#ifndef DYNSYM_H_
#define DYNSYM_H_

typedef struct dynsym DYNSYM;
struct dynsym {
  char *name;
  DYNSYM *nxt;
};

DYNSYM *get_dynsyms(int fd);
void free_dynsyms(DYNSYM *ds);

#endif /* DYNSYM_H_ */
