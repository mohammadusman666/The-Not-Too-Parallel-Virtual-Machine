/***************

Group Members:
Mohammad Usman (20-10558)
Muhammad Omer Khalil (20-10671)

Course:
CSCS440 Systems Programming

Term:
Fall 2019

***************/

#include "helper.h"

int makeargv(const char *s, const char *delimiters, char ***argvp)
{
    int error; // for error handling
    int i; // iterator
    int numtokens; // number of tokens
    const char *snew; // temporary string representing original string except for the starting delimeters
    char *t; // temporary string to tokenize

    // arguments should be given
    if ((s == NULL) || (delimiters == NULL) || (argvp == NULL))
    {
        errno = EINVAL;
        return -1;
    }

    // empty the array
    *argvp = NULL;
    snew = s + strspn(s, delimiters); /* snew is real start of string - removes the leading delimeters */

    // allocate space for the 'to be' tokenized string
    if ((t = malloc(strlen(snew) + 1)) == NULL)
        return -1;

    // copy the string into 't'
    strcpy(t, snew);
    numtokens = 0;

    /* count the number of tokens in s */
    if (strtok(t, delimiters) != NULL)
        for (numtokens = 1 ; strtok(NULL, delimiters) != NULL ; numtokens++) ;
    
    /* create argument array for ptrs to the tokens - allocate space for the argument array */
    if ((*argvp = malloc((numtokens + 1) * sizeof(char *))) == NULL)
    {
        // in case malloc fails
        error = errno;
        free(t);
        errno = error;
        return -1;
    }

    /* insert pointers to tokens into the argument array */
    if (numtokens == 0)
        free(t);
    else {
        strcpy(t, snew);
        **argvp = strtok(t, delimiters);
        for (i = 1 ; i < numtokens ; i++)
            *((*argvp) + i) = strtok(NULL, delimiters);
    }
    *((*argvp) + numtokens) = NULL; /* put in final NULL pointer */
    
    return numtokens;
}

ssize_t r_read(int fd, void *buf, size_t size)
{
    ssize_t retval;
    while (retval = read(fd, buf, size), retval == -1 && errno == EINTR);
    return retval;
}

ssize_t r_write(int fd, void *buf, size_t size)
{
    char *bufp;
    size_t bytestowrite;
    ssize_t byteswritten;
    size_t totalbytes;
    
    for (bufp = buf, bytestowrite = size, totalbytes = 0;
    bytestowrite > 0;
    bufp += byteswritten, bytestowrite -= byteswritten)
    {
        byteswritten = write(fd, bufp, bytestowrite);

        if ((byteswritten) == -1 && (errno != EINTR))
            return -1;
        
        if (byteswritten == -1)
            byteswritten = 0;

        totalbytes += byteswritten;
    }
    return totalbytes;
}