
#ifndef DATABASE_H_
#define DATABASE_H_

#include <sqlite3.h>

#include "ll.h"

#define DATABASE_PATH ".preloader-db"

typedef struct database {
  sqlite3 *db;
  int target_id;
} DATABASE;

DATABASE *database_init();			    /* opens or creates sqlite database */
void database_kill(DATABASE *db);	  /* destroys db obj */

char *database_add_target(DATABASE *db, char *path);            /* add a target, ret sha1 hash */
int database_add_symbol(DATABASE *db, char *sym);               /* add a symbol */
int database_add_sig(DATABASE *db, char *symbol, char *sig);    /* add a fcn sig */
int database_add_lib(DATABASE *db, char *name, char *path);     /* add a library */
int database_link_sym_lib(DATABASE *db, char *sym, char *lib_path);/* link symbol with lib */

LL *database_get_symbols(DATABASE *db);    /* get list of symbols */
LL *database_get_sigs(DATABASE *db, int *found);    /* get list of signatures (count = same as symbols) */

#endif /* DATABASE_H_ */
