
#ifndef LDD_H_
#define LDD_H_

typedef struct libs LIBS;
struct libs {
  char *name;
  char *path;
  LIBS *nxt;
};

LIBS *get_libs(char *path);
void free_libs(LIBS *ds);

#endif /* LDD_H_ */
