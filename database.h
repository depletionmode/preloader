
#ifndef DATABASE_H_
#define DATABASE_H_

#include <sqlite3.h>

#define DATABASE_PATH "~/.ldpreloader.db"

typedef struct database {
  sqlite3 *db;
  int target_id;
} DATABASE;

DATABASE *database_init();			/* opens or creates sqlite database */
void database_kill(DATABASE *db);	/* destroys db obj */

char *database_add_file(DATABASE *db, char *path);  /* add a target, ret sha1 hash */
int database_add_fcn_sig(DATABASE *db, char *sig);  /* add a fcn sig */

#endif /* DATABASE_H_ */
