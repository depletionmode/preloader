
#ifndef DATABASE_H_
#define DATABASE_H_

#include <sqlite3.h>

#define DATABASE_PATH "~/.ldpreloader.db"

typedef struct database {
  sqlite3 *db;
} DATABASE;

DATABASE *database_init();
void database_kill(DATABASE *db);

#endif /* DATABASE_H_ */
