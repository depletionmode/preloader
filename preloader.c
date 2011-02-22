
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>

#include "dynsym.h"
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

typedef struct symbol_list {
  char **func;            /* ordered list of function names, NULL term */
  char **sig;             /* ordered list of sets of function params, NULL term */
  int display_offset,     /* current item as top of list to display */
      selected_offset,    /* the current selected item */
      count,              /* number of symbols */
      unresolved;         /* number of unresolved symbols */
} SYMBOL_LIST;

typedef struct display {
  char *filename;         /* target filename */
  int rows,
      cols;
  int running,
      show_error;
  SYMBOL_LIST symbols;
} DISPLAY;

static void _populate_symbol_list(DATABASE *db, SYMBOL_LIST *sl)
{
  memset( sl, 0, sizeof( SYMBOL_LIST ) );

  sl->func = database_get_symbols( db, &sl->count );
  sl->sig = database_get_sigs( db );
}

static void _init_display(DISPLAY *d)
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

static void _destroy_display()
{
  clear();
  endwin();
}

static void _draw_display(DISPLAY *d)
{
  char buf[1024];
  int i, len, pos_x = 0, pos_y = 0;

  getmaxyx( stdscr, d->rows, d->cols );

  clear();

  /* draw title */
  attron( COLOR_PAIR( 6 ) );
  attron( A_BOLD );
  strcpy( buf, "  -= preloader by 2of1 =-" );
  printw( "%s", buf );
  for( i = 0; i < d->cols - pos_x - strlen( buf ); i++ ) printw( " " );
  attroff( A_BOLD );
  attroff( COLOR_PAIR( 6 ) );
  pos_y++;

  /* draw status bar */
  /* XX symbols (YY unresolved) */
  move( d->rows - 1, 0 );
  attron( COLOR_PAIR( 8 ) );
  sprintf( buf, "  %s", d->filename );
  attron( A_BOLD );
  printw( "%s", buf );
  attroff( A_BOLD );
  len = strlen( buf );
  sprintf( buf,
           "  [%d symbols, %d unresolved]",
           d->symbols.count,
           d->symbols.unresolved );
  printw( "%s", buf );
  len += strlen( buf );
  for( i = 0; i < d->cols - pos_x - len; i++ ) printw( " " );
  attroff( COLOR_PAIR( 8 ) );

  /* draw symbols */
  /* work out number of rows we have for list */
  /* print from list item ptr to end of list or num free rows (item ptr is moved up and down by up/down arrow) */
  /* format: *_if_selected function_name_bold(sig...normal) */
  pos_y = 2;
  int list_rows = d->rows - 2 /* top */ - 3 /* bottom */;
  for( i = 0; i < list_rows; i ++ ) {
    if( d->symbols.display_offset + i == d->symbols.count) break;

    move( pos_y, 2 );

    if (i == d->symbols.selected_offset + d->symbols.display_offset) attron( COLOR_PAIR( 1 ) );
    else attron( COLOR_PAIR( 2 ) );

    attron( A_BOLD );
    printw( "%c %s ", '*', d->symbols.func[d->symbols.display_offset + i] );
    attroff( A_BOLD );

    printw( "(char *, int n, char *, ...)" );

    if (i == d->symbols.selected_offset + d->symbols.display_offset) attroff( COLOR_PAIR( 1 ) );
    else attroff( COLOR_PAIR( 2 ) );

    pos_y++;
  }

  refresh();
}

static void _parse_input(DISPLAY *d)
{
  int c = getch();

  switch( c ) {
  case KEY_UP:
    if( !d->symbols.selected_offset-- )
      d->symbols.selected_offset = 0;
    break;
  case KEY_DOWN:
    if( ++d->symbols.selected_offset == d->symbols.count )
      d->symbols.selected_offset = d->symbols.count - 1;
    break;
  case 0xd: // ???
    /* edit symbol (sig +/ code?) */
    break;
  case 0x20:  /* space */
    /* select symbol for ld_preloading */
    break;
  case 'q':
    _destroy_display();
    d->running = 0;
    break;
  }
}

int main(int ac, char *av[])
{
  /* test code */

  DATABASE *db = database_init();
  database_add_target( db, av[1] ); /* add target to db */

  /* get symbols from target target */
  int fd = open( av[1], O_RDONLY );
  DYNSYM *ds = get_dynsyms( fd );
  close( fd );

  /* add symbols to db */
  while( ds ) {
    database_add_symbol( db, ds->name );
    ds = ds->nxt;
  }

  free_dynsyms( ds );

  DISPLAY d;
  memset( &d, 0, sizeof( d ) );

  _populate_symbol_list( db, &d.symbols );

  /* add target to DB */
  /* get dynamic symbols */
  /* add symbols to DB ) */
  /* (auto-resolve sigs and add to DB) */
  /* pull symbols from DB and show symbol list (pull on every refresh - inefficient but probably quick) */
  /* allow for selection of symbols */

  d.filename = av[1];
  d.running = 1;

  _init_display( &d );

  while( d.running ) {
    usleep(1000);

    _draw_display( &d );
    _parse_input( &d );
  }

  _destroy_display();

  database_kill( db );

  /* free symbol list */
  for( int i = 0; i < d.symbols.count; i++ ) {
    if( d.symbols.func ) N_FREE( d.symbols.func[i] );
    if( d.symbols.sig ) N_FREE( d.symbols.sig[i] );
  }

  return 0;
}
