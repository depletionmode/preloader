
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <openssl/sha.h>

#include "database.h"

/* this code is not threadsafe! */

static sqlite3_stmt *_Zstmt;
#define SQL_QUERY_PTR _Zstmt
#define SQL_QUERY_EXEC(X,Y,...)                           \
          do {                                            \
            char query[1000];                             \
            snprintf( query, 1000, Y, ##__VA_ARGS__ );    \
            sqlite3_prepare( X, query, -1, &_Zstmt, 0 );  \
          } while( 0 )
#define SQL_QUERY_WHILE_ROW                               \
          while( sqlite3_step( _Zstmt ) == SQLITE_ROW )
#define SQL_QUERY_END()                                   \
          sqlite3_finalize( _Zstmt )

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

static int _getid( DATABASE *db, char *table, char *where, char *match )
{
  int id = -1;

  /* get id */
  SQL_QUERY_EXEC( db->db,
                  "SELECT * FROM %s WHERE %s='%s';",
                  table,
                  where,
                  match );
  SQL_QUERY_WHILE_ROW
    id = sqlite3_column_int( SQL_QUERY_PTR, 0 );
  SQL_QUERY_END();

  return id;
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
  unsigned char sha1[SHA_DIGEST_LENGTH];
  memset( sha1, 0, SHA_DIGEST_LENGTH );

  SHA_CTX c;
  SHA1_Init( &c );
  SHA1_Update( &c, buf, len );
  SHA1_Final( sha1, &c );

  free( buf );

  static char sha1_asc[SHA_DIGEST_LENGTH * 2 + 1];
  for( int i = 0; i < SHA_DIGEST_LENGTH; i++ )
    sprintf( sha1_asc + i * 2, "%02X", (unsigned int)sha1[i] );

  if( (  db->target_id = _getid( db, "targets", "hash", sha1_asc ) ) < 0 ) {
    /* target is new, so add to db */
    SQL_QUERY_EXEC( db->db, "INSERT INTO targets VALUES ('%s');", sha1_asc );
    SQL_QUERY_END();

    db->target_id = _getid( db, "targets", "hash", sha1_asc );
  }

  return sha1_asc;
}
