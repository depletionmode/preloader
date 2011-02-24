
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exec.h"

int exec_target(char *target_path, char *params, char *lib_path)
{
  // TODO: check target path
  // TODO: check library path

  /* allow dynamic linker/loader to find wrapper code */
  if( setenv( "LD_PRELOAD", lib_path, 1 ) < 0 ) {
    fprintf( stderr, "error: failed to set environment variable (LD_PRELOAD)!\n" );
    return EXEC_EENV;
  }

  /* execute the target */
  int params_len = params ? strlen( params ) : 0;
  char *cmd = malloc( strlen( target_path ) + params_len + 2 );
  sprintf( cmd, "%s %s", target_path, params ? params : "" );
  if( system( cmd ) < 0 ) {
    fprintf( stderr, "error: failed to execute target (%s %s)!\n", target_path, params );
    return EXEC_EEXEC;
  }

  /* parent process should disable ncurses before running this and re-enable it after */
  return 1;
}
