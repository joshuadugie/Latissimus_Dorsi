#include "Harkledir.h"
#include "Harkleproc.h"
#include <fcntl.h>      // open() flags
#include "Map_Memory.h"
#include <stdbool.h>	// bool, true, false
#include <stdio.h>
#include <stdlib.h>     // calloc
#include <string.h>     // strlen, strstr

#ifndef HPROC_MAX_TRIES
// MACRO to limit repeated allocation attempts
#define HPROC_MAX_TRIES 3
#endif  // HPROC_MAX_TRIES


/*
    Purpose - Abstract-away copying names into heap-allocated arrays
    Input
        fileName - Nul-terminated character array
    Output - A heap-allocated char array containing a copy of fileName
    Notes:
        It is your responsibility to free the char* returned by this function
 */
char* copy_a_name(const char* fileName);


/*
    Purpose - Format buf into /proc/<PID>/ based on PID
    Input
        buf - [OUT] char array to build absolute path
        PID - <PID> in the above format
        bufSize - Size of buf... for safety
    Output - True on success, false on failure
    Notes:
        buf will be memset before work begins
 */
bool proc_builder(char* buf, char* PID, size_t bufSize);


pidDetails_ptr create_PID_struct(void)
{
    // LOCAL VARIABLES
    pidDetails_ptr retVal = NULL;
    int numTries = 0;

    while (!retVal && numTries < HPROC_MAX_TRIES)
    {
        retVal = (pidDetails_ptr)calloc(1, sizeof(pidDetails));
        numTries++;
    }

    if (!retVal)
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - create_PID_struct() - Failed to allocate a pidDetails struct pointer!\n");
    }

    // DONE
    return retVal;
}


/*
    Purpose - Populate a harklePIDDetails struct with PID information
    Input
        pidPath - Absolute path to /proc/<PID>
    Output
        A pointer to a heap-allocated harklePIDDetails struct complete
            with information about the PID found at pidPath on success.
            NULL on failure.
    Notes:
        Returns NULL if pidPath does not begin with /proc
        Returns NULL if the directory following /proc is not a PID (non-number)
        Returns a pointer if pidPath is missing but stillExists is False
 */
pidDetails_ptr populate_PID_struct(char* pidPath)
{
    // LOCAL VARIABLES
    pidDetails_ptr retVal = NULL;
    size_t pathLen = 0;  // strlen of pidPath
    char* newPIDPath = NULL;  // In case we need to add a trailing slash
    char* temp_ptr = NULL;  // Return value from string.h functions
    char* pidCommandline = NULL;  // Temporarily holds the absolute path to /proc/<PID>/cmdline
    size_t cmdlineLen = 0;  // The strlen of pidCommandline
    int numTries = 0;  // Check this against nax number calloc attempts
    bool success = true;  // If this is false prior to return, clean up
    mapMem_ptr cmdlineContents = NULL;  // This will hold the contents of /proc/<PID>/cmdline

    // 1. INPUT VALIDATION
    if (pidPath)
    {
        if (*pidPath)
        {
            pathLen = strlen(pidPath);

            // 1.1. Minimum length of a /proc/<PID> directory
            if (pathLen >= strlen("/proc/1"))
            {
                if ((*(pidPath + pathLen - 1)) != '/')
                {
                    // We want trailing slashes
                    // 1.1.1. Allocate a new array
                    while (!newPIDPath && numTries < HPROC_MAX_TRIES)
                    {
                        // pathLen for the original path
                        // 1 for the new trailing slash
                        // 1 for the new nul terminator
                        newPIDPath = (char*)calloc(pathLen + 2, sizeof(char));
                        numTries++;
                    }

                    if (newPIDPath)
                    {
                        // 1.1.2. Copy the old path in
                        temp_ptr = strncpy(newPIDPath, pidPath, pathLen);

                        if (temp_ptr == newPIDPath)
                        {
                            // 1.1.3. Add a trailing slash
                            temp_ptr = NULL;  // Don't need this anymore
                            (*(newPIDPath + pathLen)) = '/';
                            (*(newPIDPath + pathLen + 1)) = '\0';  // ...for safety
                            pathLen = strlen(newPIDPath);
                        }
                        else
                        {
                            fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - strncpy failed!\n");
                            success = false;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - calloc failed!\n");
                        success = false;
                    }
                }
                else
                {
                    newPIDPath = pidPath;
                }
            }
            else
            {
                fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - Path too short!\n");
                success = false;
            }

            // 1.2. Begins with /proc/
            if (success == true)
            {
                temp_ptr = strstr(newPIDPath, "/proc/");

                if (temp_ptr != newPIDPath)
                {
                    fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - Not a /proc path!\n");
                    success = false;
                }
            }

            // 1.3. Numerical directory name
            if (success == true)
            {

                temp_ptr = newPIDPath + strlen("/proc/");

                while (*temp_ptr != '/' && success == true)
                {
                    if (*temp_ptr < 48 || *temp_ptr > 57)
                    {
                        // Found a non-number
                        fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - Not a /proc/<PID> path!\n");
                        success = false;
                    }
                    else if (*temp_ptr == '\0')
                    {
                        // Missed the trailing slash?!
                        fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - Not a /proc/<PID>/ path!\n");
                        success = false;
                    }
                    else
                    {
                        temp_ptr++;
                    }
                }
            }
        }
        else
        {        
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - Empty string!\n");
            success = false;
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - NULL pointer!\n");
        success = false;
    }

    // 2. POPULATE STRUCT
    if (success == true)
    {
        // 2.1. Allocate struct
        retVal = create_PID_struct();

        if (retVal)
        {
            // 2.2. Copy pidName
            retVal->pidName = copy_a_name(newPIDPath);

            if (retVal->pidName)
            {
                numTries = 0;

                // 2.3. Read/allocate/copy /proc/<PID>/cmdline into char* pidCmdline
                while (!pidCommandline && numTries < HPROC_MAX_TRIES)
                {
                    // pathLen for /proc/<PID>/
                    // sizeof("cmdline") to make room for the filename
                    cmdlineLen = pathLen + sizeof("cmdline");
                    // 1 for the new nul-terminating character
                    pidCommandline = (char*)calloc(cmdlineLen + 1, sizeof(char));
                    numTries++;
                }

                if (pidCommandline)
                {
                    cmdlineLen = strlen(pidCommandline);  // ...for safety

                    if (cmdlineLen >= strlen("/proc/1/cmdline"))  // Minimum length
                    {
                        cmdlineContents = map_file_mode(pidCommandline, O_RDONLY);

                        if (cmdlineContents)
                        {
                            retVal->pidCmdline = copy_a_name(cmdlineContents->fileMem_ptr);

                            if (retVal->pidCmdline)
                            {
                                // 2.4. bool stillExists
                                retVal->stillExists = true;
                            }
                            else
                            {
                                fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - copy_a_name failed!\n");
                                success = false;
                            }
                        }
                        else
                        {
                            fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - map_file_mode failed!\n");
                            retVal->stillExists = false;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - /proc/<PID>/cmdline array too short!\n");
                        success = false;
                    }
                }
                else
                {
                    fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - /proc/<PID>/cmdline array allocation failed!\n");
                    success = false;
                }
            }
            else
            {
                fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - copy_a_name failed!\n");
                success = false;
            }
        }
        else
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - create_PID_struct failed!\n");
            success = false;
        }
    }

    // CLEAN UP?
    // If anything failed, clean up everything
    if (success == false)
    {
        if (false == free_PID_struct(&retVal))
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - free_PID_struct failed!\n");
        }
    }

    // Clean up this mapMem struct pointer regardless of success or failure
    if (cmdlineContents)
    {
        if (false == unmap_file(cmdlineContents, false))
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - unmap_file failed!\n");
        }

        free_struct(&cmdlineContents);
    }

    // Clean up this temporary char array regardless of success or failure
    if (pidCommandline)
    {
        if (*pidCommandline)
        {
            if (cmdlineLen <= 0)
            {
                cmdlineLen = strlen(pidCommandline);
            }

            if (cmdlineLen > 0)
            {
                // memset pidCommandline
                temp_ptr = memset(pidCommandline, 0x0, cmdlineLen);

                if (temp_ptr != pidCommandline)
                {
                    fprintf(stderr, "<<<ERROR>>> - Harkleproc - populate_PID_struct() - pidCommandline memset failed!\n");
                }
            }
        }
        // free pidCommandline
        free(pidCommandline);

        // NULL pidCommandline
        pidCommandline = NULL;
    }

    // DONE
    return retVal;
}


bool free_PID_struct(pidDetails_ptr* pidDetailsStruct_ptr)
{
    // LOCAL VARIABLES
    bool retVal = true;
    pidDetails_ptr tmpStruct_ptr = NULL;  // Easier to deal with this way
    char* tmp_ptr = NULL;  // Easier to deal with char array pointers this way
    char* mem_ptr = NULL;  // Return value from memset function calls
    size_t length = 0;  // Length of a char array

    // INPUT VALIDATION
    if (pidDetailsStruct_ptr)
    {
        if (*pidDetailsStruct_ptr)
        {
            tmpStruct_ptr = *pidDetailsStruct_ptr;

            // 1. char* pidName;          // Absolute path of PID
            if (tmpStruct_ptr->pidName)
            {
                tmp_ptr = tmpStruct_ptr->pidName;

                if (*tmp_ptr)
                {
                    // 1.1. memset pidName
                    length = strlen(tmp_ptr);

                    if (length > 0)
                    {
                        mem_ptr = memset(tmp_ptr, 0x0, length);

                        if (mem_ptr != tmp_ptr)
                        {                            
                            fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_PID_struct() - memset failed!\n");
                            retVal = false;
                        }
                    }

                    // 1.2. free pidName
                    free(tmp_ptr);

                    // 1.3. NULL pidName
                    tmp_ptr = NULL;
                    tmpStruct_ptr->pidName = NULL;
                }
            }

            // 2. char* pidCmdline;       // Complete cmdline used to execute the PID
            if (tmpStruct_ptr->pidCmdline)
            {
                tmp_ptr = tmpStruct_ptr->pidCmdline;

                if (*tmp_ptr)
                {
                    // 1.1. memset pidCmdline
                    length = strlen(tmp_ptr);

                    if (length > 0)
                    {
                        mem_ptr = memset(tmp_ptr, 0x0, length);

                        if (mem_ptr != tmp_ptr)
                        {
                            fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_PID_struct() - memset failed!\n");
                            retVal = false;
                        }
                    }
                }

                // 1.2. free pidCmdline
                free(tmp_ptr);

                // 1.3. NULL pidCmdline
                tmp_ptr = NULL;
                tmpStruct_ptr->pidCmdline = NULL;
            }

            // 3. bool stillExists;       // False if PID ever disappears
            // Just in case someone would think about accessing this
            tmpStruct_ptr->stillExists = false;
            
            // 4. NULL pidDetailsStruct_ptr
            *pidDetailsStruct_ptr = NULL;
        }
        else
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_PID_struct() - NULL pointer!\n");
            retVal = false;
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_PID_struct() - NULL pointer!\n");
        retVal = false;
    }

    // DONE
    return retVal;
}


/*
    Purpose - To memset, free, and NULL all harklePIDDetails pointers
        in an array, to include the array itself
    Input
        pidDetails_arr - Pointer to a NULL-terminated array of harklePIDDetails pointers
    Output - True on success, False on failure
    Notes:
        Calls free_PID_struct() to free each pidDetails_ptr in the array
        Will free the array itself
        Will NULL the array pointer
 */
bool free_PID_struct_arr(pidDetails_ptr** pidDetails_arr)
{
    // LOCAL VARIABLES
    bool retVal = true;
    bool freeReturn = true;
    pidDetails_ptr currStruct_ptr = NULL;  // Will hold each struct pointer in the array
    pidDetails_ptr* currStruct_arr = NULL;  // Will hold the pointer to the array

    // INPUT VALIDATION
    if (pidDetails_arr)
    {
        if (*pidDetails_arr)
        {
            currStruct_arr = *pidDetails_arr;

            // 1. Free each struct pointer
            while (*currStruct_arr)
            {
                currStruct_ptr = *currStruct_arr;

                fprintf(stdout, "BEFORE currStruct_ptr:\t%p\n", currStruct_ptr);  // DEBUGGING
                fprintf(stdout, "BEFORE *currStruct_arr:\t%p\n", *currStruct_arr);  // DEBUGGING
                freeReturn = free_PID_struct(&currStruct_ptr);
                fprintf(stdout, "AFTER currStruct_ptr:\t%p\n", currStruct_ptr);  // DEBUGGING
                fprintf(stdout, "AFTER *currStruct_arr:\t%p\n", *currStruct_arr);  // DEBUGGING

                if (freeReturn == false)
                {
                    retVal = false;
                }

                // Next struct pointer
                currStruct_arr++;
            }

            // 2. Free the struct array
            free(*pidDetails_arr);

            // 3. NULL the struct array
            *pidDetails_arr = NULL;
        }
        else
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_PID_struct() - NULL pointer!\n");
            retVal = false;
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_PID_struct() - NULL pointer!\n");
        retVal = false;
    }

    // DONE
    return retVal;
}


char** parse_proc_PIDs(void)
{
    // LOCAL VARIABLES
    char** retVal = NULL;
    dirDetails_ptr procDetails_ptr = NULL;

    // 1. walk_proc()
    // puts("1. walk_proc()");  // DEBUGGING
    procDetails_ptr = walk_proc();

    // 2. parse_PID_dirs_to_arr()
    // puts("2. parse_PID_dirs_to_arr()");  // DEBUGGING
    if (procDetails_ptr)
    {
        retVal = parse_PID_dirs_to_arr(procDetails_ptr);
    }

    // 3. free_dirDetails_ptr()
    // puts("3. free_dirDetails_ptr()");  // DEBUGGING
    if (procDetails_ptr)
    {
        free_dirDetails_ptr(&procDetails_ptr);
    }

    // DONE
    return retVal;
}


/*
    Purpose - Provide a 'list' of running PIDs based on /proc
    Input - None
    Ouput - A NULL-terminated, heap-allocated array of heap-allocated harklePIDDetails structs, one-per-directory
    Notes:
        Does all the 'heavy lifting' to walk /proc and filter out <PID> dirs
        This function calls walk_proc() to procure a list of /proc directories
        This function free()s the dirDetails_ptr resulting from walk_proc() before returning
        It is your responsibility to free each pidDetails_ptr and the pidDetails_ptr* itself 
            (or call free_PID_struct_arr())
 */
pidDetails_ptr* parse_proc_PID_structs(void)
{
    // LOCAL VARIABLES
    pidDetails_ptr* retVal = NULL;
    dirDetails_ptr procDetails_ptr = NULL;

    // 1. walk_proc()
    puts("1. parse_proc_PID_structs() calls walk_proc()");  // DEBUGGING
    procDetails_ptr = walk_proc();

    // 2. ???
    // puts("2. parse_proc_PID_structs() calls ???");  // DEBUGGING
    if (procDetails_ptr)
    {
        retVal = parse_PID_dirs_to_arr(procDetails_ptr);
    }

    // 3. free_dirDetails_ptr()
    puts("3. parse_proc_PID_structs() calls free_dirDetails_ptr()");  // DEBUGGING
    if (procDetails_ptr)
    {
        free_dirDetails_ptr(&procDetails_ptr);
    }

    // DONE
    return retVal;
}


/*
	Purpose - Walk the /proc directory for a list of files and dirs
	Input - None
	Output - A pointer to a dirDetails struct as defined in Harkledir.h
	Notes:
		It is your responsibility to call free_dirDetails_ptr(&dirDetails_ptr)
 */
dirDetails_ptr walk_proc(void)
{
    // LOCAL VARIABLES
    dirDetails_ptr retVal = open_dir("/proc");

    if (!retVal)
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - walk_proc() - open_dir failed!\n");
    }

    // DONE
    return retVal;
}


char** parse_PID_dirs_to_arr(dirDetails_ptr procWalk_ptr)
{
    // LOCAL VARIABLES
    char** retVal = NULL;  // Return value
    char** temp_ptr = NULL;  // Iterating variable
    char** tempFN_ptr = NULL;  // Iterating variable for the dirName_arr
    int numTries = 0;  // Tracks attempts to avoid upper end limit of allocation attempts
    int numNames = 0;  // Number of names to add
    int i = 0;  // Loop iterating variable

    // INPUT VALIDATION
    if (procWalk_ptr)
    {
        // Allocate the array of char pointers
        if (procWalk_ptr->numDirs > 0 && procWalk_ptr->dirName_arr)
        {
            numNames = procWalk_ptr->numDirs;
            tempFN_ptr = procWalk_ptr->dirName_arr;
            while (retVal == NULL && numTries < HPROC_MAX_TRIES)
            {
                retVal = (char**)calloc(numNames + 1, sizeof(char*));
                numTries++;
            }

            if (retVal)
            {
                temp_ptr = retVal;
                
                for (i = 0; i < numNames; i++)
                {
                    if (true == is_it_a_PID((*tempFN_ptr)))
                    {
                        // fprintf(stdout, "%s is a PID!\n", *tempFN_ptr);  // DEBUGGING
                        (*temp_ptr) = copy_a_name(*tempFN_ptr);

                        if(!(*temp_ptr))
                        {
                            fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_arr() - copy_a_name failed!\n");
                        }
                        else
                        {
                            temp_ptr++;
                        }
                    }
                    // else
                    // {
                    //     fprintf(stdout, "%s is not a PID.\n", *tempFN_ptr);  // DEBUGGING
                    // }
                    tempFN_ptr++;
                }
            }
            else
            {
                fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_arr() - calloc failed!\n");
            }
        }
        else
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_arr() - invalid dirDetails struct!\n");
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_arr() - NULL pointer!\n");
    }

    // DONE
    return retVal;
}


/*
    Purpose - Abstract-away the translation of a dirDetail array of directories into an array of pidDetails struct pointers
    Input
        procWalk_ptr - A dirDetails_ptr of /proc
    Output - A NULL-terminated, heap-allocated array of heap-allocated pidDetails struct pointers, one-per-directory
    Notes:
        This function will ignore files and filter out non-<PID> directories in /proc
 */
pidDetails_ptr* parse_PID_dirs_to_struct_arr(dirDetails_ptr procWalk_ptr)
{
    // LOCAL VARIABLES
    pidDetails_ptr* retVal = NULL;  // Return value
    pidDetails_ptr* temp_ptr = NULL;  // Iterating variable
    char** tempFN_ptr = NULL;  // Iterating variable for the dirName_arr
    int numTries = 0;  // Tracks attempts to avoid upper end limit of allocation attempts
    int numNames = 0;  // Number of names to add
    int i = 0;  // Loop iterating variable
    char templateProc[32] = { "/proc/4194303/" };  // Array to utilize as /proc/<PID>/ absolute template
    int templateSize = sizeof(templateProc) / sizeof(*templateProc);

    // INPUT VALIDATION
    if (procWalk_ptr)
    {
        // Allocate the array of char pointers
        if (procWalk_ptr->numDirs > 0 && procWalk_ptr->dirName_arr)
        {
            numNames = procWalk_ptr->numDirs;
            tempFN_ptr = procWalk_ptr->dirName_arr;
            while (retVal == NULL && numTries < HPROC_MAX_TRIES)
            {
                retVal = (pidDetails_ptr*)calloc(numNames + 1, sizeof(pidDetails_ptr));
                numTries++;
            }

            if (retVal)
            {
                temp_ptr = retVal;
                
                for (i = 0; i < numNames; i++)
                {
                    if (true == is_it_a_PID(*tempFN_ptr))
                    {
                        fprintf(stdout, "%s is a PID!\n", *tempFN_ptr);  // DEBUGGING
                        // Create an absolute path to the PID dir
                        if (true == proc_builder(templateProc, *tempFN_ptr, templateSize))
                        {
                            *temp_ptr = populate_PID_struct(templateProc);

                            if(!(*temp_ptr))
                            {
                                fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_struct_arr() - populate_PID_struct failed!\n");
                            }
                            else
                            {
                                temp_ptr++;
                            }
                        }
                        else
                        {
                            fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_struct_arr() - proc_builder failed!\n");
                        }
                    }
                    // else
                    // {
                    //     fprintf(stdout, "%s is not a PID.\n", *tempFN_ptr);  // DEBUGGING
                    // }
                    tempFN_ptr++;
                }
            }
            else
            {
                fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_struct_arr() - calloc failed!\n");
            }
        }
        else
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_struct_arr() - invalid dirDetails struct!\n");
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - parse_PID_dirs_to_struct_arr() - NULL pointer!\n");
    }

    // DONE
    return retVal;
}


bool free_char_arr(char*** charArr_ptr)
{
    // LOCAL VARIABLES
    bool retVal = true;
    char* currChar_ptr = NULL;  // C-string to be cleared
    char* temp_ptr = NULL;  // Return value from memset
    char** currChar_arr = NULL;  // Array of C-strings to be cleared
    size_t currLen = 0;  // Length of the current string

    // INPUT VALIDATION
    if (charArr_ptr)
    {
        if (*charArr_ptr)
        {
            currChar_arr = *charArr_ptr;

            while (*currChar_arr)
            {
                currChar_ptr = *currChar_arr;

                if (currChar_ptr)
                {
                    // memset char*
                    if (*currChar_ptr)
                    {
                        currLen = strlen(currChar_ptr);

                        if (currLen > 0)
                        {
                            temp_ptr = memset(currChar_ptr, 0x0, currLen);

                            if (temp_ptr != currChar_ptr)
                            {
                                fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_char_arr() - memset failed!\n");
                                retVal = false;
                            }
                            else
                            {
                                temp_ptr = NULL;
                            }
                        }
                    }

                    // free char*
                    free(currChar_ptr);

                    // NULL char*
                    currChar_ptr = NULL;
                    *currChar_arr = NULL;
                }
                // Next char*
                currChar_ptr++;
            }

            // free char**
            free(*charArr_ptr);

            // NULL char**
            charArr_ptr = NULL;
        }
        else
        {
            fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_char_arr() - NULL pointer!\n");
            retVal = false;
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - free_char_arr() - NULL pointer!\n");
        retVal = false;
    }

    // DONE
    return retVal;
}


bool is_it_a_PID(char* dirName)
{
    // LOCAL VARIABLES
    bool retVal = true;  // Default answer to prove incorrect
    char* temp_ptr = NULL;  // Iterating variable

    // INPUT VALIDATION
    if (dirName)
    {
        if (*dirName)
        {
            temp_ptr = dirName;

            while (*temp_ptr && retVal == true)
            {
                if (*temp_ptr < 48 || *temp_ptr > 57)
                {
                    retVal = false;
                }
                else
                {
                    temp_ptr++;
                }
            }
        }
        else
        {
            retVal = false;
        }
    }
    else
    {
        retVal = false;
    }

    // DONE
    return retVal;
}


//////////////////////////////////////////////////////////////////////////////
///////////////////////// LOCAL FUNCTION DEFINITIONS /////////////////////////
//////////////////////////////////////////////////////////////////////////////


char* copy_a_name(const char* fileName)
{
    // LOCAL VARIABLES
    int numTries = 0;  // Max number to calloc attempts
    char* retVal = NULL;  // Allocated char array
    char* temp_ptr = NULL;  // Holds return value from memcpy
    size_t fNameLen = 0;  // Length of fileName
    
    // INPUT VALIDATION
    if (fileName)
    {
        if (*fileName)
        {    
            // 1. Size the fileName
            fNameLen = strlen(fileName);
            
            if (fNameLen > 0)
            {            
                // 2. Allocate a properly-sized array
                while (retVal == NULL && numTries < HPROC_MAX_TRIES)
                {
                    retVal = (char*)calloc(fNameLen + 1, sizeof(char));
                    numTries++;
                }
                
                if (retVal)
                {                
                    // 3. Copy fileName into that array
                    temp_ptr = (char*)memcpy((void*)retVal, (void*)fileName, fNameLen * sizeof(char));
                    
                    if (temp_ptr != retVal)
                    {
                        fprintf(stderr, "<<<ERROR>>> - Harkleproc - copy_a_name() - memcpy failed!\n");
                        // Free retVal
                        free(retVal);
                        // NULL retVal
                        retVal = NULL;
                    }
                }
                else
                {
                    fprintf(stderr, "<<<ERROR>>> - Harkleproc - copy_a_name() - calloc failed!\n");
                }                
            }
        }
    }
    
    // DONE
    return retVal;
}


/*
    Purpose - Format buf into /proc/<PID>/ based on PID
    Input
        buf - [OUT] char array to build absolute path
        PID - <PID> in the above format
        bufSize - Size of buf... for safety
    Output - True on success, false on failure
    Notes:
        buf will be memset before work begins
 */
bool proc_builder(char* buf, char* PID, size_t bufSize)
{
    // LOCAL VARIABLES
    bool retVal = true;
    char* temp_ptr = NULL;  // Holds return values from string.h function calls
    char* tempBuf_ptr = buf;  // Iterating variable starting at buf
    char procPath[] = { "/proc/"};

    // INPUT VALIDATION
    if (!buf)
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - proc_builder() - NULL pointer!\n");
        retVal = false;
    }
    else if (!PID)
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - proc_builder() - NULL pointer!\n");
        retVal = false;
    }
    else if (!(*PID))
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - proc_builder() - empty PID!\n");
        retVal = false;
    }
    else if (bufSize < 0)
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - proc_builder() - invalid buf size!\n");
        retVal = false;
    }
    else if (bufSize < (strlen(PID) + strlen("/proc//" + 1)))
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - proc_builder() - buf too small!\n");
        retVal = false;
    }

    // BUILD /proc/<PID>/
    // 1. memset buf
    temp_ptr = memset(buf, 0x0, bufSize);

    if (temp_ptr == buf)
    {
        // 2. Precursor
        temp_ptr = procPath;

        while (*temp_ptr)
        {
            *tempBuf_ptr = *temp_ptr;
            tempBuf_ptr++;
            temp_ptr++;
        }

        // 3. PID
        temp_ptr = PID;

        while (*temp_ptr)
        {
            *tempBuf_ptr = *temp_ptr;
            tempBuf_ptr++;
            temp_ptr++;
        }

        // 4. Trailing slash
        tempBuf_ptr--;  // Walk it back one to check for a trailing slash
        if (*tempBuf_ptr != '/')
        {
            tempBuf_ptr++;
            *tempBuf_ptr = '/';
        }
    }
    else
    {
        fprintf(stderr, "<<<ERROR>>> - Harkleproc - proc_builder() - memset failed!\n");
        retVal = false;
    }

    // DONE
    return retVal;
}




