#include "Elf_Manipulation.h"
#include "Map_Memory.h"
#include <stdbool.h>	        // bool, true, false
#include <stdio.h>              // fprintf

int main(int argc, char *argv[])
{
    // LOCAL VARIABLES
    int retVal = -999;  // Change to 0 if map_file() succeeds and go from there
    mapMem_ptr elfBinary = NULL;
    mapMem_ptr codeCave = NULL;
    mapElf64_ptr elfStruct_ptr = NULL;
    
    // INPUT VALIDATION
    if (2 > argc)
    {
        fprintf(stderr, "main() - Not enough arguments!\n");
    }
    else if (2 < argc)
    {
        fprintf(stderr, "main() - Too many arguments!\n");
    }
    else
    {
        // 1. MAP FILE INTO MEMORY
        // 1.1. Map the file
        elfBinary = map_file(argv[1]);
        // 1.2. Verify mapping succeeded
        // 1.2.1. Verify the struct pointer was allocated
        if (NULL == elfBinary)
        {
            fprintf(stderr, "main() - map_file() failed to allocate a mappedMemory struct!\n");
            retVal = -1;
        }
        else
        {
            retVal = 0;  // Going good so far

            // 1.2.2. Verify the struct's memSize is present
            if (0 >= elfBinary->memSize)
            {
                fprintf(stderr, "main() - map_file() appears to have improperly calculated the size of '%s' as %zu!\n", argv[1], elfBinary->memSize);
                retVal = -2;
            }
            
            // 1.2.3. Verify the struct's memType is correct
            if (MM_TYPE_MMAP != elfBinary->memType)
            {
                fprintf(stderr, "main() - map_file() appears to have improperly set the memory type of '%s' as %d!\n", argv[1], elfBinary->memType);
                retVal = -3;
            }
            
            // 1.2.4. Verify the struct's fileMem pointer is present
            if (NULL == elfBinary->fileMem_ptr)
            {
                fprintf(stderr, "main() - map_file() failed to map '%s'!\n", argv[1]);
                retVal = -4;
            }
            else
            {
                // 1.3. Verify argv[1] is in the ELF format
                if (false == is_elf(elfBinary))
                {
                    fprintf(stderr, "main() - is_elf() reports that '%s' does not appear to be in the ELF!\n", argv[1]);
                    retVal = -5;
                }
            }
        }        
    }
    

    // 2. OPERATE ON ELF BINARY
    // 2.1. Map elfBinary to Mapped_Memory_Elf64 struct pointer
    if (0 == retVal)
    {
        if (ELFCLASS64 != determine_elf_class(elfBinary))
        {
            fprintf(stdout, "This ELF binary class is not supported.\n");
        }
        else
        {
            elfStruct_ptr = populate_mapElf64_struct(elfBinary);
            if (NULL == elfStruct_ptr)
            {
                fprintf(stdout, "main() - populate_mapElf64_struct() has returned NULL!\n");
                retVal = -6;
            }
        }
    }

    // 2.2. Determine entry point
    if (0 == retVal)
    {
        // DEBUGGING
        // Verify address
        fprintf(stdout, "ELF Header:\t\t%p\nProgram Header:\t\t%p\nSection Header:\t\t%p\n", \
                (void*) elfStruct_ptr->binaryEhdr_ptr, \
                (void*) elfStruct_ptr->binaryPhdr_ptr, \
                (void*) elfStruct_ptr->binaryShdr_ptr);  // DEBUGGING
        fprintf(stdout, "Entry Point Offset:\t%p\n", (void*) elfStruct_ptr->binaryEhdr_ptr->e_entry);  // DEBUGGING
        fprintf(stdout, "Absolute Entry Point:\t%p\n", (void*) (elfStruct_ptr->binaryEhdr_ptr + \
                elfStruct_ptr->binaryEhdr_ptr->e_entry));  // DEBUGGING
    }

    // 2.3. Find a Code Cave
    codeCave = find_code_cave(elfBinary);
    if (false == validate_struct(codeCave))
    {
        fprintf(stderr, "main() - validate_struct() doesn't agree with the return value from find_code_cave()!\n");
        retVal = -7;
    }
    else
    {
        fprintf(stdout, "Found a code cave!\nPointer:\t%p\nMem Size:\t%zu\n", codeCave->fileMem_ptr, codeCave->memSize);
        fprintf(stdout, "Offset:\t\t%p\n", (void*)(codeCave->fileMem_ptr - elfBinary->fileMem_ptr));
    }

    
    // X. CLEAN UP
    if (NULL != elfBinary)
    {
        free_struct(&elfBinary);
    }
    if (NULL != elfStruct_ptr)
    {
        free(elfStruct_ptr);
        elfStruct_ptr = NULL;
    }

    // DONE
    return retVal;
}
