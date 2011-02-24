
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

enum {
  STATE_NORMAL,
  STATE_NOTIFICATION,
  STATE_PROCESSING
};

typedef struct symbol_list {
  char **func;            /* ordered list of function names */
  char **sig;             /* ordered list of sets of function params */
  int *selected;          /* list of selected items */
  int display_offset,     /* current item as top of list to display */
      selected_offset,    /* the current selected item */
      count,              /* number of symbols */
      no_sigs;           /* number of found fcn sigs */
} SYMBOL_LIST;

typedef struct display {
  char *filename;         /* target filename */
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

static void _populate_symbol_list(DATABASE *db, SYMBOL_LIST *sl)
{
  memset( sl, 0, sizeof( SYMBOL_LIST ) );

  sl->func = database_get_symbols( db, &sl->count );
  sl->sig = database_get_sigs( db, &sl->no_sigs );
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
  static char in[1000];
  char *ptr = in;

  echo();
  attron( COLOR_PAIR( 2 ) );

  mvchgat( d->rows - 4, 2, -1, A_INVIS, 0, NULL );
  move( d->rows - 3, 2 );
  for( int i = 0; i < d->cols; i++ ) addch( ' ' );
  mvprintw( d->rows - 3, 2, "%s > ", str );
//089719000
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
    {
      int r, c;
      getyx( stdscr, r, c );
      if( ptr > in ) {
        addch(' ');
        move( r, c );
        ptr--;
      } else
        move( r, c + 1 );
    }
      break;
    case '\n':
      end = 1;
      c = '\0';
    default:
      *(ptr++) = c;
    }

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
           d->symbols.no_sigs );
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
      printw( "%c ", d->symbols.selected[d->symbols.display_offset + i] ? '*' : ' ' );
      attroff( A_BOLD );
      attroff( COLOR_PAIR( 5 ) );

      if (i == d->symbols.selected_offset - d->symbols.display_offset) attron( COLOR_PAIR( 1 ) );
      else attron( COLOR_PAIR( 2 ) );

      attron( A_BOLD );
      printw( "%s ", d->symbols.func[d->symbols.display_offset + i] );
      attroff( A_BOLD );

      printw( "%s", d->symbols.sig[d->symbols.display_offset + i] ? d->symbols.sig[d->symbols.display_offset + i] : "(int)()" );

      if (i == d->symbols.selected_offset - d->symbols.display_offset) attroff( COLOR_PAIR( 1 ) );
      else attroff( COLOR_PAIR( 2 ) );

      pos_y++;
    }
    break;
  }
  case STATE_PROCESSING:
  {
    static int symbol_count = 0;
    static char swirl[] = "|/-\\";
    static char *c = swirl;

    move( 2, 2 );
    attron( COLOR_PAIR( 2 ) );
    attron( A_BOLD );

    if( *(++c) == '\0' ) c = swirl;
    printw( "%c Processing symbol %d, please be patient...", *c, ++symbol_count );

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
  case 0xd: // ???
    /* edit symbol (sig +/ code?) */
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

  d.filename = av[1];

  //get_libs(d.filename); //TODO

  _init_display();
  d.state = STATE_PROCESSING;

  DATABASE *db = database_init();
  database_add_target( db, d.filename ); /* add target to db */

  /* get symbols from target target */
  int fd = open( av[1], O_RDONLY );
  DYNSYM *ds = get_dynsyms( fd );
  close( fd );

  /* add symbols to db */
  DYNSYM *p_ds = ds;
  while( p_ds ) {
    d.extra = p_ds->name;
    _draw_display( &d );
    database_add_symbol( db, p_ds->name );
    if( !strcmp( p_ds->name, "memcpy" ) ) database_add_sig( db, p_ds->name, "(void*)(void* destination, const void* source, size_t num)" );
    if( !strcmp( p_ds->name, "atoi" ) ) database_add_sig( db, p_ds->name, "(int)(const char* str)" );
    if( !strcmp( p_ds->name, "free" ) ) database_add_sig( db, p_ds->name, "(void)(void* ptr)" );
    if( !strcmp( p_ds->name, "malloc" ) ) database_add_sig( db, p_ds->name, "(void*)(size_t size)" );
    if( !strcmp( p_ds->name, "printf" ) ) database_add_sig( db, p_ds->name, "(int)(const char* format, ...)" );
    if( !strcmp( p_ds->name, "putchar" ) ) database_add_sig( db, p_ds->name, "(int)(int character)" );
    if( !strcmp( p_ds->name, "strlen" ) ) database_add_sig( db, p_ds->name, "(size_t)(const char* str)" );
    p_ds = p_ds->nxt;
  }

  free_dynsyms( ds );

  _populate_symbol_list( db, &d.symbols );

  /* add target to DB */
  /* get dynamic symbols */
  /* add symbols to DB ) */
  /* (auto-resolve sigs and add to DB) */
  /* pull symbols from DB and show symbol list (pull on every refresh - inefficient but probably quick) */
  /* allow for selection of symbols */

  d.state = STATE_NORMAL;
  d.running = 1;

  while( d.running ) {
    usleep(1000);

    _draw_display( &d );
    _parse_input( &d );
  }

  _disable_display();

  database_kill( db );

  /* free symbol list */
  for( int i = 0; i < d.symbols.count; i++ ) {
    if( d.symbols.func ) N_FREE( d.symbols.func[i] );
    if( d.symbols.sig ) N_FREE( d.symbols.sig[i] );
  }

  return 0;
}
