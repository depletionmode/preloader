
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "dynsym.h"

/* test code */
int main(int ac, char *av[])
{
    int fd = open( av[1], O_RDONLY );

    DYNSYM *ds = get_dynsyms( fd );

    close( fd );

    while( ds ) {
        printf( "%s\n", ds->name );
        ds = ds->nxt;
    }

    free_dynsyms( ds );

    return 0;
}
