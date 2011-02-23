
#ifndef DATABASE_H_
#define DATABASE_H_

#include <sqlite3.h>

#define DATABASE_PATH ".preloader-db"

typedef struct database {
  sqlite3 *db;
  int target_id;
} DATABASE;

DATABASE *database_init();			/* opens or creates sqlite database */
void database_kill(DATABASE *db);	/* destroys db obj */

char *database_add_target(DATABASE *db, char *path);  /* add a target, ret sha1 hash */
int database_add_symbol(DATABASE *db, char *sym);     /* add a symbol */
int database_add_fcn_sig(DATABASE *db, char *symbol, char *sig);    /* add a fcn sig */

char **database_get_symbols(DATABASE *db, int *count);    /* get NULL term list of symbols */
char **database_get_sigs(DATABASE *db, int *resolved);    /* get NULL term list of signatures */

#endif /* DATABASE_H_ */
