#include <elf.h>
#include "Elf_Manipulation.h"
#include "Map_Memory.h"
#include <stdbool.h>		// bool, true, false
#include <stdio.h>          // fprintf
#include <string.h>         // strstr

#ifndef MAX_TRIES
// MACRO to limit repeated allocation attempts
#define MAX_TRIES 3
#endif // MAX_TRIES

bool is_elf(mapMem_ptr file)
{
    // LOCAL VARIABLES
    bool retVal = false;
    char* file_ptr = NULL;
    char* return_ptr = NULL;
    char magicNum[] = { 0x7F, 'E', 'L', 'F', 0x0 };
    
    // INPUT VALIDATION
    if (NULL != file)
    {
        if (NULL != file->fileMem_ptr)
        {
            // FIND ELF MAGIC NUMBER
            return_ptr = strstr(file->fileMem_ptr, magicNum);
            
            if (file->fileMem_ptr == return_ptr)
            {
                retVal = true;
            }
            else if (NULL != return_ptr)
            {
                retVal = true;
                fprintf(stdout, "Found the ELF magic number at 0x%p but the mapped file begins at 0x%p", return_ptr, file->fileMem_ptr);  // DEBUGGING
            }
        }
    }
    
    // DONE
    return retVal;
}


/*
	Purpose - Determine if a mappedMemory file is 32-bit ELF or 64-bit ELF
	Input - mappedMemory struct pointer
	Output
		ELFCLASSNONE if mappedMemory is not an ELF file
		ELFCLASS32 if mappedMemory is a 32-bit ELF file
		ELFCLASS64 if mappedMemory is a 64-bit ELF file
        -1 on error
        -999 on 'horrible' error
 */
int determine_elf_class(mapMem_ptr elfFile)
{
    // LOCAL VARIABLES
    int retVal = -999;
    char* magicNum = NULL;
    
    // INPUT VALIDATION
    // 0. Is the mapMem_ptr an actual pointer?
    if (NULL == elfFile)
    {
        retVal = -1;
    }    
    // 1. Is elfFile a valid mappedMemory struct
    else if (false == validate_struct(elfFile))
    {
        retVal = -1;
    }
    // 2. Is elfFile actually an ELF file
    else if (false == is_elf(elfFile))
    {
        retVal = ELFCLASSNONE;
    }    
    // 3. Is elfFile 32-bit or 64-bit?
    else
    {
        magicNum = elfFile->fileMem_ptr;
        
        if (NULL != magicNum)
        {
            // "The fifth byte identifies the architecture for this binary"
            retVal = (int)(*(magicNum + 4));
            // Validate what we found
            if (retVal != ELFCLASSNONE && \
                retVal != ELFCLASS32 && \
                retVal != ELFCLASS64)
            {
                fprintf(stderr, "determine_elf_class() - found an unsupported EI_CLASS value of %d!\n", retVal);
                retVal = -999;    
            }
        }
        else
        {
            retVal = -1;    
        }
    }
    
    // DONE
    return retVal;
}


/*
	Purpose - Allocate and populate a Mapped_Memory_Elf64 struct with ELF details
	Input - mappedMemory struct pointer to an ELF file
	Output - Dynamically allocated Mapped_Memory_Elf64 pointer
	Notes
		Calls validate_struct() to validate elfFile
		Calls is_elf() to verify format
		Calls determine_elf_class() to verify this is the right function
 */
mapElf64_ptr populate_mapElf64_struct(mapMem_ptr elfFile)
{
    // LOCAL VARIABLES
    mapElf64_ptr retVal = NULL;
	int numTries = 0;
    
	// INPUT VALIDATION
	// 1. Validate mappedMemory struct pointer
	if (false == validate_struct(elfFile))
	{
		fprintf(stderr, "populate_mapElf64_struct() - elfFile is an invalid mappedMemory struct pointer!\n");	
	}
	// 2. Verify elfFile is actually an ELF file
	else if (false == is_elf(elfFile))
	{
		fprintf(stderr, "populate_mapElf64_struct() - elfFile is, in fact, NOT and ELF file!\n");
	}
	// 3. Verify this ELF file is, in fact, 64-bit ELF
	else if (ELFCLASS64 != determine_elf_class(elfFile))
	{
		fprintf(stderr, "populate_mapElf64_struct() - elfFile is not a 64-bit ELF file!\n");
	}
	else
	{	
		// ALLOCATE A STRUCT
		while (NULL == retVal && numTries < MAX_TRIES)
		{
			retVal = (mapElf64_ptr)calloc(1, sizeof(mapElf64));
			numTries++;
		}
		
		if (NULL == retVal)
		{
			fprintf(stderr, "populate_mapElf64_struct() - allocation of Mapped_Memory_Elf64 struct has failed %d times!\n", numTries);
		}
		else
		{
			// MAP ELF FILE HEADERS
			// 1. mappedMemory struct pointer
			retVal->binary_ptr = elfFile;
			// 2. ELF Header pointer
			retVal->binaryEhdr_ptr = retVal->binary_ptr->fileMem_ptr;
			// 3. Program Header pointer
			retVal->binaryPhdr_ptr = retVal->binary_ptr->fileMem_ptr + retVal->binaryEhdr_ptr->e_phoff;
			// 4. Section Header pointer
			retVal->binaryShdr_ptr = retVal->binary_ptr->fileMem_ptr + retVal->binaryEhdr_ptr->e_shoff;
		}		
	}
	
    // DONE
    return retVal;
}


/*
    Purpose - Search an ELF binary for the largest section of 'nothing'
    Input - elfBinary - struct* mappedMemory
    Output - struct* mappedMemory which holds the address and size of the 
        largest section of 'nothing'
 */
mapMem_ptr find_code_cave(mapMem_ptr elfBinary)
{
    // LOCAL VARIABLES
    mapMem_ptr retVal = NULL;
    char* tempAddr_ptr = NULL;  // Pointer to the beginning of the current section
    char* currChar_ptr = NULL;  // Pointer used to validate each char
    int byteCounter = 0;        // Counts the size of the 'nothing' segment

    // INPUT VALIDATION
    if (true == validate_struct(elfBinary))
    {
        // puts("MADE IT PAST validate_struct()");  // DEBUGGING
        // 1. Create the return struct
        retVal = create_mapMem_ptr();
        retVal->fileMem_ptr = elfBinary->fileMem_ptr;
        retVal->memSize = 0;
        retVal->memType = MM_TYPE_CAVE;
        currChar_ptr = elfBinary->fileMem_ptr;
        tempAddr_ptr = elfBinary->fileMem_ptr;

        // puts("MADE IT HERE!");  // DEBUGGING
        // 2. Start counting
        while (currChar_ptr < (elfBinary->fileMem_ptr + elfBinary->memSize))
        {
            // puts("ITERATING!");  // DEBUGGING
            // putchar(*currChar_ptr);  // DEBUGGING
            // Found empty char
            if (0x0 == *currChar_ptr)
            {
                // puts("MADE IT HERE!");  // DEBUGGING
                byteCounter++;
            }
            // End of empty chars
            else
            {
                // Larger than value stored in retVal
                if (byteCounter > retVal->memSize)
                {
                    // puts("FOUND ONE!");  // DEBUGGING
                    // Set struct to beginning of this section
                    retVal->fileMem_ptr = tempAddr_ptr;
                    // Set the size
                    retVal->memSize = byteCounter;
                    // fprintf(stdout, "Current winner pointer:\t%p\n", retVal->fileMem_ptr);  // DEBUGGING
                    // fprintf(stdout, "Current winner length:\t%zu\n", retVal->memSize);  // DEBUGGING
                }

                // Reset...
                // ...byteCounter
                byteCounter = 0;
                // ...tempAddr_ptr
                tempAddr_ptr = currChar_ptr + 1;
            }

            // Increment to next byte
            currChar_ptr++;
        }
        // Trailing "code cave"
        // Larger than value stored in retVal
        if (byteCounter > retVal->memSize)
        {
            // puts("FOUND ONE!");  // DEBUGGING
            // Set struct to beginning of this section
            retVal->fileMem_ptr = tempAddr_ptr;
            // Set the size
            retVal->memSize = byteCounter;
            // fprintf(stdout, "Current winner pointer:\t%p\n", retVal->fileMem_ptr);  // DEBUGGING
            // fprintf(stdout, "Current winner length:\t%zu\n", retVal->memSize);  // DEBUGGING
        }
    }

    // Never found one
    if (0 == retVal->memSize && retVal->fileMem_ptr == elfBinary->fileMem_ptr)
    {
        retVal->fileMem_ptr = NULL;
    }

    // DONE
    return retVal;
}

