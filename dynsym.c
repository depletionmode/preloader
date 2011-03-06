
#include <gelf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dynsym.h"

DYNSYM *get_dynsyms(int fd, int unresolved_only) {
  DYNSYM *f_ds = NULL, *ds = NULL;

  Elf *e = NULL;
  Elf_Scn *s = NULL;

  elf_version( EV_CURRENT );  /* init libelf */

  if( !( e = elf_begin( fd, ELF_C_READ, NULL ) ) ) {
    fprintf( stderr, "error reading ELF file!\n" ); /* read ELF file */
    exit( EXIT_FAILURE );
  }

  while( ( s = elf_nextscn( e, s ) ) ) {  /* loop through sections */
    GElf_Shdr shdr;

    gelf_getshdr( s, &shdr );

    if( shdr.sh_type == SHT_DYNSYM ) {
      Elf_Data *d = NULL;

      GElf_Sym sym;
      int num_syms, i;

      d = elf_getdata( s, d );
      num_syms = shdr.sh_size / shdr.sh_entsize;

      for( i = 0; i < num_syms; i++ ) {
        gelf_getsym( d, i, &sym );

        if( ELF32_ST_TYPE( sym.st_info ) == STT_FUNC ) { /* also good for 64-bit */
          if( unresolved_only ^ 1 || !sym.st_value ) {
            DYNSYM *n_ds = calloc( 1, sizeof( DYNSYM ) );

            char *name = elf_strptr( e, shdr.sh_link, sym.st_name );

            n_ds->name = malloc( strlen( name ) + 1 );
            strcpy( n_ds->name, name );

            if( !f_ds ) f_ds = n_ds;
            else ds->nxt = n_ds;

            ds = n_ds;
          }
        }
      }
    }
  }

  elf_end( e );

  return f_ds;
}

void free_dynsyms(DYNSYM *ds)
{
  DYNSYM *nxt;

  while( ds ) {
    nxt = ds->nxt;

    free(ds->name);
    free(ds);

    ds = nxt;
  }
}

