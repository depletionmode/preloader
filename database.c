
#include <stdio.h>
#include <stdlib.h>

#include "database.h"

static void _create( DATABASE *db ) {
  char *queries[] = {
      "CREATE TABLE targets ("              \
      "  id INT NOT NULL AUTO_INCREMENT,"   \
      "  hash TEXT,"                        \
      "  name TEXT,"                        \
      "  PRIMARY KEY (id)"                  \
      ");",
      "CREATE TABLE symbols ("              \
      "  id INT NOT NULL AUTO_INCREMENT,"   \
      "  symbol TEXT,"                      \
      "  PRIMARY KEY (id)"                  \
      ");",
      "CREATE TABLE symbol_link ("          \
      "  id INT NOT NULL AUTO_INCREMENT,"   \
      "  target_id INT,"                    \
      "  symbol_id INT,"                    \
      "  PRIMARY KEY (id)"                  \
      ");",
      "CREATE TABLE signatures ("           \
      "  id INT NULL AUTO_INCREMENT,"       \
      "  signature TEXT,"                   \
      "  PRIMARY KEY (id)"                  \
      ");",
      NULL
  };

  char *ptr = *queries;

  while( ptr ) {
    sqlite3_exec( db->db, ptr, NULL, 0, NULL );
    ptr++;
  }
}

DATABASE *database_init()
{
  DATABASE *db = calloc( 1, sizeof( DATABASE ) );

  /* open db or create it */
  if( sqlite3_open( DATABASE_PATH, &db->db ) !=  SQLITE_OK) {
    fprintf( stderr, "error opening/creating DB: %s",
             sqlite3_errmsg( db->db ) );

    exit( EXIT_FAILURE );
  }

  /* check if db is new, init it if so */
  if( sqlite3_exec( db->db,
      "SELECT * FROM sqlite_master WHERE name='targets';",
      NULL, 0, NULL ) != SQLITE_OK ) _create( db );

  return db;
}

void database_kill(DATABASE *db)
{

}
