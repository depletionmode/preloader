
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dynsym.h"
#include "ldd.h"
#include "exec.h"
#include "database.h"
#include "ll.h"
#include "code.h"

#define N_FREE(X) \
  do {            \
    if( X ) {     \
      free( X );  \
      X = NULL;   \
    }             \
  } while( 0 )

#define EXIT(X) \
  do {          \
    clear();    \
    endwin();   \
    exit( X );  \
  } while( 0 )

#define BTWN(X,Y,Z) ( X >= Y && X <= Z )

enum {
  STATE_NORMAL,
  STATE_PROCESSING_LIBS,
  STATE_PROCESSING_SYMS,
  STATE_RESOLVING_SYMBOLS
};

enum {
  SYMBOL_SELECTED = 1,
  SYMBOL_INVALID
};

typedef struct symbol_list {
  LL *func;               /* ordered list of function names */
  LL *sig;                /* ordered list of sets of function params */
  LL *lib;                /* ordered list of function linked libraries */
  int *flags;             /* list of selected items */
  int display_offset,     /* current item as top of list to CTX */
      selected_offset,    /* the current selected item */
      count,              /* number of symbols */
      num_sigs,           /* number of found fcn sigs */
      num_libs;           /* number of found fcn linked libs */
} SYMBOL_LIST;

typedef struct context {
  DATABASE *db;
  char filename[200];     /* target filename */
  char *hash;
  int rows,
      cols;
  int running;
  SYMBOL_LIST symbols;
  int state;
  void *extra;
} CTX;

static char *_strip_path(char *file_path)
{
  char *ptr;

  if( ( ptr = strrchr( file_path, '/' ) ) ) ptr++;
  else ptr = file_path;

  return ptr;
}

static void _skip_spaces(char **ptr)
{
  while( *ptr && **ptr == ' ' ) (*ptr)++;
}

static void _truncate_spaces(char **ptr)
{
  while( *ptr && *(*ptr + 1) && **ptr == ' ' && *(*ptr + 1) == ' ' ) (*ptr)++;
}

static char *_validate_sig(char *sig)
{
  /* this function first tries to force signature to conform and failing that returns NULL */

  /* this stuff is very messy - I know! Needs *SERIOUS* work!!!! */

  static char final_sig[1024];
  memset( final_sig, 0, sizeof( final_sig) );

  char *ptr = sig;
  char *f_sig_ptr = final_sig;

  /* skip spaces at start */
  _skip_spaces( &ptr );

  /* check for open bracket */
  if( *ptr != '(' ) return NULL;
  *(f_sig_ptr++) = *(ptr++);

  /* skip spaces */
  _skip_spaces( &ptr );

  /* loop until closing bracket */
  while( *ptr != ')' ) {
    /* truncate spaces to a single one between items */
    _truncate_spaces( &ptr );

    /* handle if '*' */
    if( *ptr == '*' && *(ptr - 1) != ' ' )
      *(f_sig_ptr++) = ' ';

    *(f_sig_ptr++) = *(ptr++);
  }
  /* skip trailing space before bracket */
  if( *(f_sig_ptr - 1) == ' ' )
    f_sig_ptr--;
  *(f_sig_ptr++) = *ptr;

  /* skip spaces */
  _skip_spaces( &ptr );

  ptr++;

  /* check for open bracket */
  if( *ptr != '(' ) return NULL;
  *(f_sig_ptr++) = *(ptr++);

  /* loop until closing bracket */
  while( *ptr != ')' ) {
    /* skip spaces after bracket */
    if( *(ptr - 1) == '(' )
      _skip_spaces( &ptr );

    /* truncate spaces to a single one between items */
    _truncate_spaces( &ptr );

    /* make sure there is a space before '*' */
    if( *ptr == '*' && *(ptr - 1) != ' ' )
      *(f_sig_ptr++) = ' ';

    /* skip space after '*' */
    if( *ptr == '*' && *(ptr + 1) == ' ' ) {
      ptr++;
      _skip_spaces( &ptr );
      *(f_sig_ptr++) = '*';
      continue;
    }

    /* skip space before comma */
    if( *ptr == ' ' && *(ptr + 1) == ',' ) ptr++;

    /* make sure there is a space after comma */
    if( *(ptr - 1) == ',' && *ptr != ' ' )
      *(f_sig_ptr++) = ' ';

    *(f_sig_ptr++) = *(ptr++);
  }
  /* skip trailing space before bracket */
  if( *(f_sig_ptr - 1) == ' ' )
    f_sig_ptr--;
  *f_sig_ptr = *ptr;

  /* if there is no last bracket, add it
  if( *f_sig_ptr  != ')' )
    *(++f_sig_ptr) = ')';*/

  //printf("c: %c ptr: %s\tfinal_sig: %s\n", *ptr, ptr, final_sig);

  return final_sig;
}

static void _populate_symbol_list(DATABASE *db, SYMBOL_LIST *sl)
{
  memset( sl, 0, sizeof( SYMBOL_LIST ) );
  sl->func = database_get_symbols( db );
  sl->count = ll_size( sl->func );
  sl->sig = database_get_sigs( db, &sl->num_sigs );
  sl->lib = database_get_libs( db, &sl->num_libs );
  sl->flags = calloc( 1, sl->count * sizeof( int ) );
}

static void _init_display()
{
  initscr();
  cbreak();
  keypad( stdscr, TRUE );
  curs_set( FALSE );
  noecho();

  start_color();

  int bg_color = COLOR_BLACK;
  if( use_default_colors() == OK )
    bg_color = -1;

  init_pair( 1, COLOR_BLACK, COLOR_WHITE );
  init_pair( 2, COLOR_WHITE, bg_color );
  init_pair( 3, COLOR_GREEN, bg_color );
  init_pair( 4, COLOR_RED, bg_color );
  init_pair( 5, COLOR_YELLOW, bg_color );
  init_pair( 6, COLOR_WHITE, COLOR_GREEN );
  init_pair( 7, COLOR_WHITE, COLOR_RED );
  init_pair( 8, COLOR_WHITE, COLOR_BLUE );

  clear();
}

static void _disable_display()
{
  clear();
  endwin();
}

/* show notification */
/* TODO: line wrapping */
static void _show_notification(CTX *ctx, char *notice)
{
  int pos_y = ctx->rows / 2 - 1;
  int pos_x = ctx->cols / 2 - strlen( notice ) / 2 - 2;

  attron( COLOR_PAIR( 7 ) );
  attron( A_BOLD );
  move( pos_y++, 6 );
  for( int i = 0; i < ctx->cols - 12; i++ ) addch( ' ' );
  move( pos_y++, 6 );
  for( int i = 0; i < ctx->cols - 12; i++ ) addch( ' ' );
  move( pos_y--, 6 );
  for( int i = 0; i < ctx->cols - 12; i++ ) addch( ' ' );
  mvprintw( pos_y++, pos_x, "  %s  ", notice );
  attroff( A_BOLD );
  attroff( COLOR_PAIR( 7 ) );

  refresh();

  getch();  /* wait until keypress */
}

static char *_get_input(CTX *ctx, char *str, char *dflt)
{
  //TODO KEY_ESC cancelation
  static char in[1000];
  char *ptr = in;

  noecho();
  attron( COLOR_PAIR( 2 ) );

  mvchgat( ctx->rows - 4, 2, -1, A_INVIS, 0, NULL );
  move( ctx->rows - 3, 2 );
  for( int i = 0; i < ctx->cols; i++ ) addch( ' ' );
  mvprintw( ctx->rows - 3, 2, "%s > ", str );

  int y, x;
  getyx( stdscr, y, x );

  attron( A_BOLD );
  curs_set( 2 );

  if( dflt ) {
    strcpy( ptr, dflt );
    printw( dflt );
    ptr += strlen( ptr );
  } else
    memset( in, 0, sizeof( in ) );

  int end = 0,
      c = getch();
  do {
    refresh();

    switch( c ) {
    case KEY_BACKSPACE:
      if( ptr != in )
        *(--ptr) = '\0';
      break;
    case '\n':
      end = 1;
      c = '\0';
    default:
      if( strspn( (char*)&c, " ()*,._")
          || BTWN( c, 0x30, 0x39 )
          || BTWN( c, 0x41, 0x5a )
          || BTWN( c, 0x61, 0x7a ) )
        *(ptr++) = c;
    }

    mvprintw( y, x, "%s", in );
    for( int i = 0; i < ctx->cols - strlen( in ) - 3; i++ ) addch( ' ' );
    move( y, x + strlen( in ) );

  } while ( !end && ( c = getch() ) );

  curs_set( 0 );
  attroff( A_BOLD );

  attroff( COLOR_PAIR( 2 ) );
  noecho();

  return in;
}

static void _draw_display(CTX *ctx)
{
  char buf[1024];
  int len, pos_x = 0, pos_y = 0;

  getmaxyx( stdscr, ctx->rows, ctx->cols );

  clear();

  /* draw title */
  attron( COLOR_PAIR( 6 ) );
  attron( A_BOLD );
  strcpy( buf, "  -= preloader by 2of1 =-" );
  printw( "%s", buf );
  for( int i = 0; i < ctx->cols - pos_x - strlen( buf ); i++ ) addch( ' ' );
  attroff( A_BOLD );
  attroff( COLOR_PAIR( 6 ) );
  pos_y++;

  /* draw status bar */
  move( ctx->rows - 1, 0 );
  attron( COLOR_PAIR( 8 ) );
  sprintf( buf, "  %s", ctx->filename );
  attron( A_BOLD );
  printw( "%s", buf );
  attroff( A_BOLD );
  len = strlen( buf );
  sprintf( buf,
           "  [%d symbol%s, %d sig match%s]",
           ctx->symbols.count,
           ctx->symbols.count > 1 ? "s" : "",
           ctx->symbols.num_sigs,
           ctx->symbols.num_sigs > 1 ? "es" : "" );
  printw( "%s", buf );
  len += strlen( buf );
  for( int i = 0; i < ctx->cols - pos_x - len; i++ ) addch( ' ' );;
  attroff( COLOR_PAIR( 8 ) );

  switch( ctx->state ) {
  case STATE_NORMAL:
  {
    /* draw symbols */
    /* work out number of rows we have for list */
    /* print from list item ptr to end of list or num free rows (item ptr is moved up and down by up/down arrow) */
    /* format: *_if_selected function_name_bold(sig...normal) */
    pos_y = 2;
    int list_rows = ctx->rows - 1 /* top */ - 3 /* bottom */;
    for( int i = 0; i < list_rows; i ++ ) {
      if( ctx->symbols.display_offset + i == ctx->symbols.count) break;

      move( pos_y, 2 );

      attron( COLOR_PAIR( 5 ) );
      attron( A_BOLD );
      printw( "%c ",
              ctx->symbols.flags[ctx->symbols.display_offset + i] & SYMBOL_SELECTED ? '*' : ' ' );
      attroff( A_BOLD );
      attroff( COLOR_PAIR( 5 ) );

      if (i == ctx->symbols.selected_offset - ctx->symbols.display_offset)
        attron( COLOR_PAIR( 1 ) );
      else
        attron( COLOR_PAIR( 2 ) );

      attron( A_BOLD );
      printw( "%s ",
              ll_access( ctx->symbols.func, ctx->symbols.display_offset + i ) );
      attroff( A_BOLD );

      printw( "%s",
              ll_access( ctx->symbols.sig, ctx->symbols.display_offset + i ) );

      char *str = _strip_path( ll_access( ctx->symbols.lib,
                               ctx->symbols.display_offset + i ) );
      mvprintw( pos_y, ctx->cols - strlen( str ) - 4, "[%s]", str );

      if (i == ctx->symbols.selected_offset - ctx->symbols.display_offset)
        attroff( COLOR_PAIR( 1 ) );
      else
        attroff( COLOR_PAIR( 2 ) );

      pos_y++;
    }

    break;
  }
  case STATE_PROCESSING_SYMS:
  case STATE_PROCESSING_LIBS:
  case STATE_RESOLVING_SYMBOLS:
  {
    static int count = 0;
    static char swirl[] = "|/-\\";
    static char *c = swirl;

    if( count >> 24 != ctx->state ) {
      count = ctx->state << 24;
    }

    move( 2, 2 );
    attron( COLOR_PAIR( 2 ) );
    attron( A_BOLD );

    if( *(++c) == '\0' ) c = swirl;
    printw( "%c %s %s %d, please be patient...",
            *c,
            ctx->state == STATE_RESOLVING_SYMBOLS ? "matching" : "processing",
            ctx->state == STATE_PROCESSING_SYMS || STATE_RESOLVING_SYMBOLS ? "symbol" : "library",
            ++count & 0xffffff );

    attroff( A_BOLD );

    printw( " (%s)", (char *)ctx->extra );

    attroff( COLOR_PAIR( 2 ) );

    break;
  }
  }

  refresh();
}

enum exec_flag {
  EXEC_NOPROMPT = 1,
  EXEC_PROMPT = 2
};

static void _exec_target(char *target_path, char *params, char *lib_path, int flags)
{
  _disable_display();

  exec_target( target_path, params, lib_path );

  if( flags & EXEC_PROMPT ) {
    fprintf( stderr,"\n\nPress enter to continue...\n" );
    getchar();
  }

  _init_display();
}

static void _list_scroll(CTX *ctx, int direction, int inc)
{
#define SCROLL_UP 0
#define SCROLL_DOWN 1

  for( int i = 0; i < inc; i ++ ) {
    if( direction == SCROLL_UP ) {
      if( !ctx->symbols.selected_offset-- )
        ctx->symbols.selected_offset = 0;
      if( ctx->symbols.selected_offset ==  ctx->symbols.display_offset - 1 )
        ctx->symbols.display_offset--;
    } else {
      if( ++ctx->symbols.selected_offset == ctx->symbols.count )
        ctx->symbols.selected_offset = ctx->symbols.count - 1;
      if( ctx->symbols.selected_offset >= ctx->rows - 4 + ctx->symbols.display_offset )
        ctx->symbols.display_offset++;
    }
  }
}

static void _parse_input(CTX *ctx)
{
  static char *params = NULL;
  int c = getch();

  switch( c ) {
  case KEY_UP:
    _list_scroll( ctx, SCROLL_UP, 1 );
    break;
  case KEY_DOWN:
    _list_scroll( ctx, SCROLL_DOWN, 1 );
    break;
  case KEY_NPAGE:
    _list_scroll( ctx, SCROLL_DOWN, ctx->rows - 4 );
    break;
  case KEY_PPAGE:
    _list_scroll( ctx, SCROLL_UP, ctx->rows - 4 );
    break;
  case '\n':  /* edit & compile code */
  {
    char *func = ll_access( ctx->symbols.func, ctx->symbols.selected_offset );
    char *sig = ll_access( ctx->symbols.sig, ctx->symbols.selected_offset );
    char *lib = ll_access( ctx->symbols.lib, ctx->symbols.selected_offset );

    /* check for signature */
    /* the assumption here is that the sig by this stage has been validated elsewhere */
    if( memcmp( sig, "??", 2 ) != 0 ) {   /* has signature */
      char path[500];
      sprintf( path,
               "%s/.preloader/%s-%s.%s.c",
               getenv("HOME"),
               ctx->hash,
               _strip_path( ctx->filename ),
               func );

      /* check if file exists */
      struct stat s;
      /* populate if file doesn't exist or is empty */
      if( stat( path, &s ) < 0 || !s.st_size ) {
        FILE *f = fopen( path, "a" );
        if( f ) {
          char *code = code_gen( func, sig, _strip_path( lib ) );
          if( code ) {
            fwrite( code, 1, strlen( code ), f );
            N_FREE( code );
          }
          fclose(f);
        }
      }

      /* TODO: hard-coded to vim now; allow editor selection through config */
      _exec_target( "vim", path, NULL, EXEC_NOPROMPT );

      /* compile code */

      /* remove previous object file */
      char obj_path[500];
      strncpy( obj_path, path, sizeof( obj_path )- 1 );
      obj_path[strlen( obj_path ) - 1] = '\0';
      strncat( obj_path, "o", sizeof( obj_path ) - 1);
      _exec_target( "rm -f", obj_path, NULL, EXEC_NOPROMPT );

      /* compile PIC code */
      strncat( path, " -o ", sizeof( path ) - 1);
      strncat( path, obj_path, sizeof( path ) - 1);
      _exec_target( "gcc -fPIC -c", path, NULL, EXEC_NOPROMPT );

      /* check if object file is present */
      if( stat( obj_path, &s ) < 0 ) {
        /* compilation failed */
        _exec_target( "echo", "Compilation failed! Please fix errors.", NULL, EXEC_PROMPT );
        ctx->symbols.flags[ctx->symbols.selected_offset] |= SYMBOL_INVALID;
        ctx->symbols.flags[ctx->symbols.selected_offset] &= ~SYMBOL_SELECTED;
      } else {
        /* compilation succeeded */
        ctx->symbols.flags[ctx->symbols.selected_offset] &= ~SYMBOL_INVALID;
      }
    } else {  /* no signature */
      _show_notification( ctx, "Please first enter a signature for this function!" );
    }
  }
    break;
  case 's':   /* edit function signature */
  {
    char *sig = _get_input( ctx,
                            "function signature",
                            (char *)ll_access( ctx->symbols.sig,
                                               ctx->symbols.selected_offset ) );

    if( ( sig = _validate_sig( sig ) ) )
      database_add_sig( ctx->db,
                        (char *)ll_access( ctx->symbols.func,
                                           ctx->symbols.selected_offset ),
                        sig );

    /* refresh sig list */
    ctx->symbols.sig = database_get_sigs( ctx->db, &ctx->symbols.num_sigs );
  }
    break;
  case 0x20:  /* select symbol for preloading */
    if( ctx->symbols.flags[ctx->symbols.selected_offset] & SYMBOL_INVALID ) {
      _show_notification( ctx, "Could not compile this wrapper. Please check code!" );
    } else {
      ctx->symbols.flags[ctx->symbols.selected_offset] ^= SYMBOL_SELECTED;
    }
    break;
  case 'r':   /* enter params then execute */
    /* todo: check if function with invalid code is selected */
  {
    char tmp[300];
    sprintf( tmp, "'%s' args", _strip_path( ctx->filename ) );
    params = _get_input( ctx, tmp, params );
  }
  case 'R':   /* execute with previous params */
    /* todo: check if function with invalid code is selected */
    _exec_target( ctx->filename, params, NULL, EXEC_PROMPT );
    break;
  case 'q':
    ctx->running = 0;
    break;
  }
}

int main(int ac, char *av[])
{
  /* create .preloader directory */
  char path[500];
  sprintf( path, "%s/.preloader", getenv("HOME") );
  if( mkdir( path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) < 0 )  /* mkdir failed */
    if( errno !=  EEXIST ) {
      fprintf( stderr, "error: failed to create %s!\n", path );
      exit( 0 );
    }


  CTX ctx;
  memset( &ctx, 0, sizeof( ctx ) );

  snprintf( ctx.filename,
            sizeof( ctx.filename ),
            "%s%s", *av[1] == '/' || *av[1] == '.' ? "" : "./",
            av[1] );

  _init_display();
  ctx.state = STATE_PROCESSING_SYMS;

  ctx.db = database_init();
  ctx.hash = database_add_target( ctx.db, ctx.filename ); /* add target to db */

  /* get symbols from target target */
  int fd = open( av[1], O_RDONLY );
  DYNSYM *ds = get_dynsyms( fd, DYNSYM_UNDEFINED_ONLY );
  close( fd );

  /* add symbols to db */
  DYNSYM *p_ds = ds;
  while( p_ds ) {
    ctx.extra = p_ds->name;
    _draw_display( &ctx );
    database_add_symbol( ctx.db, p_ds->name );
    p_ds = p_ds->nxt;
  }

  ctx.state = STATE_PROCESSING_LIBS;

  /* add libs to db */
  LL *lib_sym_info = ll_calloc();

  LIBS *libs = get_libs(ctx.filename);
  LIBS *p_libs = libs;
  while( p_libs ) {
    ctx.extra = p_libs->path;
    _draw_display( &ctx );
    database_add_lib( ctx.db, p_libs->name, p_libs->path );

    p_libs = p_libs->nxt;
  }

  ctx.state = STATE_RESOLVING_SYMBOLS;
  /* match symbols to libs */
  /* immensely inefficient, should call get_dynsyms() ONCE for each lib!! (TODO) */
  p_ds = ds;
  while( p_ds ) {   /* for each symbol in target */
    int found = 0;

    ctx.extra = p_ds->name;
    _draw_display( &ctx );

    LIBS *p_lib = libs;
    while( p_lib && !found ) {  /* search each lib for match */

      int fd_lib = open( p_lib->path, O_RDONLY );
      DYNSYM *ds_lib = get_dynsyms( fd_lib, DYNSYM_DEFINED_ONLY );    /* get symbols from lib */

      DYNSYM *p_ds_lib = ds_lib;
      while( p_ds_lib ) { /* for each symbol in library */
        if( strcmp( p_ds_lib->name, p_ds->name ) == 0 ) {
          database_link_sym_lib( ctx.db, p_ds->name, p_lib->path );
          found = 1;
          break;
        }
        p_ds_lib = p_ds_lib->nxt;   /* move to next symbol in library */
      }
      free_dynsyms( ds_lib );

      close( fd_lib );

      p_lib = p_lib->nxt;   /* move to next library */
    }

    p_ds = p_ds->nxt; /* move to next symbol */
  }

  ll_free( lib_sym_info );

  free_libs( libs );

  free_dynsyms( ds );

  // TODO iter through and free free_dynsyms( ds_lib );

  _populate_symbol_list( ctx.db, &ctx.symbols );

  ctx.state = STATE_NORMAL;
  ctx.running = 1;

  while( ctx.running ) {
    usleep(1000);

    _draw_display( &ctx );
    _parse_input( &ctx );
  }

  _disable_display();

  database_kill( ctx.db );

  /* free symbol list */
  ll_free( ctx.symbols.func );
  ll_free( ctx.symbols.sig );

  return 0;
}
