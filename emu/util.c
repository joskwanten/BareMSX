#include <stdio.h>
#include <string.h>
#include "util.h"

bool read_rom(char *filename, uint32_t size, uint8_t *ptr)
{
    /* Open file for both reading and writing */
    FILE *fp = fopen(filename, "rb");
    if (fp)
    {        
        uint32_t read_size = fread(ptr, 1, size, fp);
        if (read_size == size)
        {        
            fclose(fp);
            return true;
        }

        fclose(fp);
    }

    return false;
}