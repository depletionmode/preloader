
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>

#include "dynsym.h"
#include "ldd.h"
#include "exec.h"
#include "database.h"
#include "ll.h"

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
  STATE_NOTIFICATION,
  STATE_PROCESSING_LIBS,
  STATE_PROCESSING_SYMS,
};

typedef struct symbol_list {
  LL *func;               /* ordered list of function names */
  LL *sig;                /* ordered list of sets of function params */
  LL *lib;                /* ordered list of function linked libraries */
  int *selected;          /* list of selected items */
  int display_offset,     /* current item as top of list to display */
      selected_offset,    /* the current selected item */
      count,              /* number of symbols */
      num_sigs,           /* number of found fcn sigs */
      num_libs;           /* number of found fcn linked libs */
} SYMBOL_LIST;

typedef struct display {
  DATABASE *db;
  char filename[200];     /* target filename */
  int rows,
      cols;
  int running,
      show_error;
  SYMBOL_LIST symbols;
  int state;
  void *extra;
} DISPLAY;

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

  /* this stuff is very messy - I know! */

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
  sl->selected = calloc( 1, sl->count * sizeof( int ) );
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

static char *_get_input(DISPLAY *d, char *str, char *dflt)
{
  //TODO KEY_ESC cancelation
  static char in[1000];
  char *ptr = in;

  noecho();
  attron( COLOR_PAIR( 2 ) );

  mvchgat( d->rows - 4, 2, -1, A_INVIS, 0, NULL );
  move( d->rows - 3, 2 );
  for( int i = 0; i < d->cols; i++ ) addch( ' ' );
  mvprintw( d->rows - 3, 2, "%s > ", str );

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
      if( strspn( (char*)&c, " ()*,.")
          || BTWN( c, 0x30, 0x39 )
          || BTWN( c, 0x41, 0x5a )
          || BTWN( c, 0x61, 0x7a ) )
        *(ptr++) = c;
    }

    mvprintw( y, x, "%s", in );
    for( int i = 0; i < d->cols - strlen( in ) - 3; i++ ) addch( ' ' );
    move( y, x + strlen( in ) );

  } while ( !end && ( c = getch() ) );

  curs_set( 0 );
  attroff( A_BOLD );

  attroff( COLOR_PAIR( 2 ) );
  noecho();

  return in;
}

static void _draw_display(DISPLAY *d)
{
  char buf[1024];
  int len, pos_x = 0, pos_y = 0;

  getmaxyx( stdscr, d->rows, d->cols );

  clear();

  /* draw title */
  attron( COLOR_PAIR( 6 ) );
  attron( A_BOLD );
  strcpy( buf, "  -= preloader by 2of1 =-" );
  printw( "%s", buf );
  for( int i = 0; i < d->cols - pos_x - strlen( buf ); i++ ) addch( ' ' );
  attroff( A_BOLD );
  attroff( COLOR_PAIR( 6 ) );
  pos_y++;

  /* draw status bar */
  move( d->rows - 1, 0 );
  attron( COLOR_PAIR( 8 ) );
  sprintf( buf, "  %s", d->filename );
  attron( A_BOLD );
  printw( "%s", buf );
  attroff( A_BOLD );
  len = strlen( buf );
  sprintf( buf,
           "  [%d symbols, %d sig maches]",
           d->symbols.count,
           d->symbols.num_sigs );
  printw( "%s", buf );
  len += strlen( buf );
  for( int i = 0; i < d->cols - pos_x - len; i++ ) addch( ' ' );;
  attroff( COLOR_PAIR( 8 ) );

  switch( d->state ) {
  case STATE_NORMAL:
  {
    /* draw symbols */
    /* work out number of rows we have for list */
    /* print from list item ptr to end of list or num free rows (item ptr is moved up and down by up/down arrow) */
    /* format: *_if_selected function_name_bold(sig...normal) */
    pos_y = 2;
    int list_rows = d->rows - 1 /* top */ - 3 /* bottom */;
    for( int i = 0; i < list_rows; i ++ ) {
      if( d->symbols.display_offset + i == d->symbols.count) break;

      move( pos_y, 2 );

      attron( COLOR_PAIR( 5 ) );
      attron( A_BOLD );
      printw( "%c ",
              d->symbols.selected[d->symbols.display_offset + i] ? '*' : ' ' );
      attroff( A_BOLD );
      attroff( COLOR_PAIR( 5 ) );

      if (i == d->symbols.selected_offset - d->symbols.display_offset)
        attron( COLOR_PAIR( 1 ) );
      else
        attron( COLOR_PAIR( 2 ) );

      attron( A_BOLD );
      printw( "%s ",
              ll_access( d->symbols.func, d->symbols.display_offset + i ) );
      attroff( A_BOLD );

      printw( "%s",
              ll_access( d->symbols.sig, d->symbols.display_offset + i ) );

      char *str = _strip_path( ll_access( d->symbols.lib,
                               d->symbols.display_offset + i ) );
      mvprintw( pos_y, d->cols - strlen( str ) - 4, "[%s]", str );

      if (i == d->symbols.selected_offset - d->symbols.display_offset)
        attroff( COLOR_PAIR( 1 ) );
      else
        attroff( COLOR_PAIR( 2 ) );

      pos_y++;
    }
    break;
  }
  case STATE_PROCESSING_SYMS:
  case STATE_PROCESSING_LIBS:
  {
    static int count = 0;
    static char swirl[] = "|/-\\";
    static char *c = swirl;

    if( count >> 24 != d->state ) {
      count = d->state << 24;
    }

    move( 2, 2 );
    attron( COLOR_PAIR( 2 ) );
    attron( A_BOLD );

    if( *(++c) == '\0' ) c = swirl;
    printw( "%c Processing %s %d, please be patient...",
            *c,
            d->state == STATE_PROCESSING_SYMS ? "symbol" : "library",
            ++count & 0xffffff );

    attroff( A_BOLD );

    printw( " (%s)", (char *)d->extra );

    attroff( COLOR_PAIR( 2 ) );

    break;
  }
  }

  refresh();
}

static void _exec_target(char *target_path, char *params, char *lib_path)
{
  _disable_display();

  exec_target( target_path, params, lib_path );

  fprintf( stderr,"\n\nPress enter to continue...\n" );
  getchar();

  _init_display();
}

static void _list_scroll(DISPLAY *d, int direction, int inc)
{
#define SCROLL_UP 0
#define SCROLL_DOWN 1

  for( int i = 0; i < inc; i ++ ) {
    if( direction == SCROLL_UP ) {
      if( !d->symbols.selected_offset-- )
        d->symbols.selected_offset = 0;
      if( d->symbols.selected_offset ==  d->symbols.display_offset - 1 )
        d->symbols.display_offset--;
    } else {
      if( ++d->symbols.selected_offset == d->symbols.count )
        d->symbols.selected_offset = d->symbols.count - 1;
      if( d->symbols.selected_offset >= d->rows - 4 + d->symbols.display_offset )
        d->symbols.display_offset++;
    }
  }
}

static void _parse_input(DISPLAY *d)
{
  static char *params = NULL;
  int c = getch();

  switch( c ) {
  case KEY_UP:
    _list_scroll( d, SCROLL_UP, 1 );
    break;
  case KEY_DOWN:
    _list_scroll( d, SCROLL_DOWN, 1 );
    break;
  case KEY_NPAGE:
    _list_scroll( d, SCROLL_DOWN, d->rows - 4 );
    break;
  case KEY_PPAGE:
    _list_scroll( d, SCROLL_UP, d->rows - 4 );
    break;
  case '\n':  /* enter */
  {
    char *sig = ll_access( d->symbols.sig, d->symbols.selected_offset );
    sig = _get_input( d, "function signature", sig );

    if( ( sig = _validate_sig( sig ) ) )
      database_add_sig( d->db, (char *)ll_access( d->symbols.func, d->symbols.selected_offset ), sig );

    /* refresh sig list */
    d->symbols.sig = database_get_sigs( d->db, &d->symbols.num_sigs );
  }
    break;
  case 0x20:  /* space */
    /* select symbol for ld_preloading */
    // TODO: check for sig
    d->symbols.selected[d->symbols.selected_offset] ^= 1;
    break;
  case 'R':   /* enter params then execute */
  {
    char tmp[300];
    sprintf( tmp, "'%s' args", _strip_path( d->filename ) );
    params = _get_input( d, tmp, params );
  }
  case 'r':   /* execute with previous params */
    _exec_target( d->filename, params, NULL );
    break;
  case 'q':
    d->running = 0;
    break;
  }
}

int main(int ac, char *av[])
{
  DISPLAY d;
  memset( &d, 0, sizeof( d ) );

  snprintf( d.filename,
            sizeof( d.filename ),
            "%s%s", *av[1] == '/' || *av[1] == '.' ? "" : "./",
                av[1] );

  _init_display();
  d.state = STATE_PROCESSING_SYMS;

  d.db = database_init();
  database_add_target( d.db, d.filename ); /* add target to db */

  /* get symbols from target target */
  int fd = open( av[1], O_RDONLY );
  DYNSYM *ds = get_dynsyms( fd, DYNSYM_UNDEFINED_ONLY );
  close( fd );

  /* add symbols to db */
  DYNSYM *p_ds = ds;
  while( p_ds ) {
    d.extra = p_ds->name;
    _draw_display( &d );
    database_add_symbol( d.db, p_ds->name );
    p_ds = p_ds->nxt;
  }

  d.state = STATE_PROCESSING_LIBS;

  /* add libs to db */
  LL *lib_sym_info = ll_calloc();

  LIBS *libs = get_libs(d.filename);
  LIBS *p_libs = libs;
  while( p_libs ) {
    d.extra = p_libs->path;
    _draw_display( &d );
    database_add_lib( d.db, p_libs->name, p_libs->path );

    p_libs = p_libs->nxt;
  }

  /* match symbols to libs */
  /* immensely inefficient, should call get_dynsyms() ONCE for each lib!! (TODO) */
  p_ds = ds;
  while( p_ds ) {   /* for each symbol in target */
    int found = 0;

    LIBS *p_lib = libs;
    while( p_lib && !found ) {  /* search each lib for match */

      int fd_lib = open( p_lib->path, O_RDONLY );
      DYNSYM *ds_lib = get_dynsyms( fd_lib, DYNSYM_DEFINED_ONLY );    /* get symbols from lib */

      DYNSYM *p_ds_lib = ds_lib;
      while( p_ds_lib ) { /* for each symbol in library */
        if( strcmp( p_ds_lib->name, p_ds->name ) == 0 ) {
          database_link_sym_lib( d.db, p_ds->name, p_lib->path );
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

  _populate_symbol_list( d.db, &d.symbols );

  d.state = STATE_NORMAL;
  d.running = 1;

  while( d.running ) {
    usleep(1000);

    _draw_display( &d );
    _parse_input( &d );
  }

  _disable_display();

  database_kill( d.db );

  /* free symbol list */
  ll_free( d.symbols.func );
  ll_free( d.symbols.sig );

  return 0;
}
