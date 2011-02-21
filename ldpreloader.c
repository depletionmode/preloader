
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>

#include "dynsym.h"

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

typedef struct display {
  int rows,
      cols;
} DISPLAY;

void _init_display(DISPLAY *d)
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

  init_pair( 1, COLOR_CYAN, bg_color );
  init_pair( 2, COLOR_WHITE, bg_color );
  init_pair( 3, COLOR_GREEN, bg_color );
  init_pair( 4, COLOR_RED, bg_color );
  init_pair( 5, COLOR_YELLOW, bg_color );
  init_pair( 6, COLOR_BLACK, COLOR_GREEN );
  init_pair( 7, COLOR_BLUE, bg_color );
  init_pair( 8, COLOR_WHITE, COLOR_BLUE );

  clear();
  getmaxyx( stdscr, d->rows, d->cols );
}

void _destroy_display(DISPLAY *d)
{
  clear();
  endwin();
}

int main(int ac, char *av[])
{
  /* test code */
  int fd = open( av[1], O_RDONLY );

  DYNSYM *ds = get_dynsyms( fd );

  close( fd );

  while( ds ) {
      printf( "%s\n", ds->name );
      ds = ds->nxt;
  }

  free_dynsyms( ds );

  /* add target to DB */
  /* get dynamic symbols */
  /* add symbols to DB ) */
  /* (auto-resolve sigs and add to DB) */
  /* pull symbols from DB and show symbol list (pull on every refresh - inefficient but probably quick) */
  /* allow for selection of symbols */

  return 0;
}
