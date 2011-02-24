
#ifndef EXEC_H_
#define EXEC_H_

#define EXEC_SUCCESS 0
#define EXEC_EENV -1    /* failed to set env variable */
#define EXEC_EEXEC -2   /* failed to exec target */
#define EXEC_ELIB -3    /* failed to find lib to preload */

int exec_target(char *target_path, char *params, char *lib_path);

#endif /* EXEC_H_ */
