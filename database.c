
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
            /* DEBUG */ printf("SQL DEBUG: %s\n", query); \
          } while( 0 )
#define SQL_QUERY_WHILE_ROW                               \
          while( sqlite3_step( _Zstmt ) == SQLITE_ROW )
#define SQL_QUERY_END()                                   \
          sqlite3_finalize( _Zstmt )

static void _create( DATABASE *db ) {
  char *queries[] = {
      "CREATE TABLE targets ("              \
      "  id INTEGER PRIMARY KEY,"           \
      "  hash TEXT"                        \
      ");",
      "CREATE TABLE symbols ("              \
      "  id INTEGER PRIMARY KEY,"           \
      "  symbol TEXT"                       \
      ");",
      "CREATE TABLE symbol_link ("          \
      "  id INTEGER PRIMARY KEY,"           \
      "  target_id INTEGER,"                \
      "  symbol_id INTEGER"                 \
      ");",
      "CREATE TABLE sig_link ("             \
      "  id INTEGER PRIMARY KEY,"           \
      "  symbol_id INTEGER,"                \
      "  sig_id INTEGER"                    \
      ");",
      "CREATE TABLE signatures ("           \
      "  id INTEGER PRIMARY KEY,"           \
      "  signature TEXT"                    \
      ");",
      NULL
  };

  char **ptr = queries;

  while( *ptr ) {
    sqlite3_exec( db->db, *ptr, NULL, 0, NULL );
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

  printf("ID: %d\n", id);

  return id;
}

DATABASE *database_init()
{
  DATABASE *db = calloc( 1, sizeof( DATABASE ) );

  /* open db or create it */
  char path[500];
  sprintf( path, "%s/%s", getenv("HOME"), DATABASE_PATH );
  if( sqlite3_open( path, &db->db ) !=  SQLITE_OK) {
    fprintf( stderr, "error opening/creating DB (%s): %s!\n",
             path,
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

char *database_add_target(DATABASE *db, char *path)
{
  FILE *f;

  /* open target */
  if( ( f = fopen( path, "rb" ) ) < 0 ) {
    fprintf( stderr, "error opening target!\n" );
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

  static char sha1_str[SHA_DIGEST_LENGTH * 2 + 1];
  for( int i = 0; i < SHA_DIGEST_LENGTH; i++ )
    sprintf( sha1_str + i * 2, "%02X", (unsigned int)sha1[i] );

  if( (  db->target_id = _getid( db, "targets", "hash", sha1_str ) ) < 0 ) {
    /* new target, so add to db */
    SQL_QUERY_EXEC( db->db, "INSERT INTO targets ('hash') VALUES ('%s');", sha1_str );
    SQL_QUERY_WHILE_ROW;
    SQL_QUERY_END();

    db->target_id = sqlite3_last_insert_rowid( db->db );

    printf("ID: %d\n", db->target_id);
  }

  return sha1_str;
}

int database_add_symbol(DATABASE *db, char *sym)
{
  int symbol_id;

  if( !( symbol_id = _getid( db, "symbols", "symbol", sym ) ) ) {
    /* new symbol, so add to db */
    SQL_QUERY_EXEC( db->db, "INSERT INTO symbols ('symbol') VALUES ('%s');", sym);
    SQL_QUERY_WHILE_ROW;
    SQL_QUERY_END();

    symbol_id = sqlite3_last_insert_rowid( db->db );
  }

  /* link symbols and target */
  SQL_QUERY_EXEC( db->db, "INSERT INTO sym_link ('target_id','symbol_id') VALUES ('%s','%s');", db->target_id, symbol_id);
  SQL_QUERY_WHILE_ROW;
  SQL_QUERY_END();

  return symbol_id;
}

int database_add_fcn_sig(DATABASE *db, char *sig)
{
  // TODO: verify sig for format

  /* find function (symbol) by reading until '(' */
  char *fcn = calloc( 1, strcspn( sig, "(" ) + 1 );

  if( !db->target_id )
    return 0;

  int symbol_id;
  if( !( symbol_id = _getid( db, "symbols", "symbol", fcn ) ) )
    return 0; /* no such symobl */

  int sig_id;
  if( !( sig_id = _getid( db, "signatures", "signature", sig ) ) ) {
    /* add sig */
    SQL_QUERY_EXEC( db->db,
                    "INSERT INTO signatures VALUES ('%s');", sig );
    SQL_QUERY_WHILE_ROW;
    SQL_QUERY_END();

    sig_id = sqlite3_last_insert_rowid( db->db );
  }

  /* link sig to symbol */
  SQL_QUERY_EXEC( db->db,
                  "INSERT INTO sig_link VALUES ('%d', '%d');",
                  symbol_id,
                  sig_id );
  SQL_QUERY_WHILE_ROW;
  SQL_QUERY_END();

  free( fcn );

  return sqlite3_last_insert_rowid( db->db );
}

char *database_get_fcns(DATABASE *db)
{/*
  SQL_QUERY_EXEC( db->db,
                  "SELECT symbols.symbol FROM symbols, sym_link WHERE sym_link.='%s';",
                  table,
                  where,
                  match );
  SQL_QUERY_WHILE_ROW
    id = sqlite3_column_int( SQL_QUERY_PTR, 0 );
  SQL_QUERY_END();*/
}

char *database_get_sigs(DATABASE *db)
{

}
