
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ncurses.h>

#include "code.h"

char *code_gen( char *func, char *sig, char *lib )
{
  clear();
endwin();
  /* default headers */
  char headers[] = "#include <stdio.h>\n"       \
                   "#include <stdlib.h>\n"      \
                   "#include <string.h>\n"      \
                   "#include <dlfcn.h>\n";

  /* fcn type */
  char type[100];
  memset( type, 0, sizeof( type ) );
  memcpy( type, sig + 1, strchr( sig, ')' ) - sig - 1 );

  /* ptr to original function */
  char fptr[200];
  snprintf( fptr, sizeof( fptr ), "%s (*o_%s)();\n", type, func );

  /* dlopen() library */
  char dl[2000];
  snprintf( dl,
            sizeof( dl ),
            "  static void *h = NULL;\n"                                      \
            "  if (!h) {\n"                                                   \
            "    if (!(h = dlopen(\"%s\", 0))) {\n"                           \
            "      fprintf(stderr, \"fail: unable to dlopen() %s!\\n\");\n"   \
            "      exit(1);\n"                                                \
            "    }\n"                                                         \
            "    \n"                                                          \
            "    o_%s = dlsym(h, \"%s\");\n"                                  \
            "  }\n",
            lib,
            lib,
            func,
            func );

  char code[10000];
  snprintf( code,
            sizeof( code ),
            "\n"                                                            \
            "/* headers */\n%s\n"                                           \
            "/* ptr to orig fcn */\n%s\n"                                   \
            "/* wrapper */\n"                                               \
            "%s %s%s\n{\n"                                                  \
            "%s\n"                                                          \
            "  return o_%s%s;\n"                                             \
            "}\n",  /* TODO need to remove types from call! */
            headers,
            fptr,
            type,
            func,
            strchr( sig + 1, '(' ),
            dl,
            func,
            strchr( sig + 1, '(' ) );

  int len = strnlen( code, sizeof( code ) );

  char *dyn_code = calloc( 1, len );
  strncpy( dyn_code, code, len );

  return dyn_code;  /* don't forget to free() this after use! */
}
