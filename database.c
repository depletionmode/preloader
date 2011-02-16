
#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha1.h>

#include "database.h"

/* this code is not threadsafe! */

static sqlite3_stmt *stmt;
#define SQL_QUERY_PTR stmt
#define SQL_QUERY_EXEC(X,Y,...)                         \
          do {                                          \
            char query[1000];                           \
            snprintf( query, 1000, Y, __VAR_ARGS__ );   \
            sqlite3_prepare( X, query, -1, &stmt, 0 );  \
          } while( 0 )
#define SQL_QUERY_WHILE_ROW                             \
          while( sqlite3_step( stmt ) == SQLITE_ROW )
#define SQL_QUERY_END()                                 \
          sqlite3_finalize( stmt )

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

  /* check if db is new, create it if so */
  int rows = 0;
  SQL_QUERY_EXEC( db->db, "SELECT * FROM sqlite_master WHERE name='targets';" );
  SQL_QUERY_WHILE_ROW rows++;
  SQL_QUERY_END();
  if( !rows ) _create( db );

  return db;
}

void database_kill(DATABASE *db)
{
  sqlite3_close( db->db );
  free( db );
}

char *database_add_file(DATABASE *db, char *path)
{
  FILE *f;

  /* open target */
  if( ( f = fopen( path, O_RDONLY ) ) < 0 ) {
    fprintf( stderr, "error opening target" );
    exit( EXIT_FAILURE );
  }

  /* read target */
  fseek( f, 0, SEEK_END );
  int len = ftell( f );
  rewind( f );

  char *buf = malloc( len );
  fread( buf, 1, len, f );

  fclose( f );

  /* calc sha1 hash */
  static char sha1[SHA_DIGEST_LENGTH + 1];
  memset( sha1, 0, sizeof( sha1 ) );

  SHA_CTX c;
  SHA1_Init( &c );
  SHA1_Update( &c, buf, len );
  SHA1_Final( sha1, &c );

  free( buf );

  /* check if target already present in db */
  SQL_QUERY_EXEC( db->db, "SELECT * FROM targets WHERE hash='%s';", sha1 );
  SQL_QUERY_WHILE_ROW
    db->target_id = sqlite3_column_int( stmt2, 0 ); /* target exists */
  SQL_QUERY_END();

  if( !exists ) { /* target is new, so add to db */
    SQL_QUERY_EXEC( db->db, "INSERT INTO targets VALUES ('%s');", sha1 );
    SQL_QUERY_END();
  }

  return sha1;
}
