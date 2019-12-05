#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "ntpvm.h"

#define MAX_LINE_SIZE 100
#define TERMINATE_STRING "$\n"

static char *typename[] = {"Start Task", "Data", "Broadcast", "Done", "Terminate", "Barrier"};

int getpacket(int, int *, int *, packet_t *, int *, unsigned char *);
int putpacket(int, int, int, packet_t, int, unsigned char *);
int makeargv(const char *s, const char *delimiters, char ***argvp);

int main(void)
{
    unsigned char buf[MAX_PACK_SIZE];
    int compid, taskid, tdatalen;
    packet_t type;
    int tin, tout;

    char delim[] = " \t\n"; // delimeters
    int i; // iterator
    char **myargv; // argument array
    int numtokens; // number of tokens

    pid_t child;
    int mypipefd[2];
    
    tin = STDIN_FILENO;
    tout = STDOUT_FILENO;
    
    while (getpacket(tin, &compid, &taskid, &type, &tdatalen, buf) != -1)
    {
        fprintf(stderr, "\n\n%d %d %d %d %s\n\n", compid, taskid, type, tdatalen, buf);

        /* create pipe first */
        if (pipe(mypipefd) < 0)
        {
            fprintf(stderr, "%s", strerror(errno));
            return -1;
        }
        /* pipe has been created */
        child = fork();
        if (child < 0)
        {
            fprintf(stderr, "%s", strerror(errno));
            return -1;
        }
        else if (child > 0)
        {
            wait(NULL); // wait for child

            char concat_str[MAX_PACK_SIZE];

            close(mypipefd[1]); // cloade write end
            read(mypipefd[0], concat_str, MAX_PACK_SIZE); // read from pipe
            // printf("Concatenated string %s\n", concat_str);
            close(mypipefd[0]);

            type = 1; // 1 = DATA
            tdatalen = strlen(concat_str);
            *buf = 0;
            strcpy(buf, concat_str);
            if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
                break;
        }
        else
        {
            /* child code */
            /* redirect stdin & stdout to pipe */
            dup2(mypipefd[1], 1);
            dup2(mypipefd[0], 0);

            close(mypipefd[0]);
            close(mypipefd[1]);

            /* now exec the new image */
            // we make the the argument array
            if ((numtokens = makeargv(buf, delim, &myargv)) == -1)
            {
                fprintf(stderr, "Failed to construct an argument array for %s\n", buf);
                return -1;
            }
            // print argument array
            myargv[numtokens-1] = NULL; // make $ = null
            // printf("The argument array contains:\n");
            // for (i = 0; i < numtokens; i++)
            //     printf("%d: %s\n", i, myargv[i]);
            
            // a null terminated array of character pointers
            // char *args[] = {"printenv", NULL};
            fprintf(stderr, "Executing...\n\n");
            execvp(myargv[0], myargv);
        }
    }

    // close pipe read & write ends
    close(mypipefd[0]);
    close(mypipefd[1]);

    type = 3; // 3 = DONE
    tdatalen = 0;
    *buf = 0;
    if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1)
        return -1;
    
    return 0;
}

/* The getpacket function reads a taskpacket_t header from fd and then reads into buf the
number of bytes specified by the length member. If successful, getpacket returns 0. If
unsuccessful, getpacket returns –1 and sets errno. The getpacket function sets *compidp,
*taskidp, *typep and *lenp from the compid, taskid, type and length members of the
packet header, respectively. If getpacket receives an end-of-file while trying to read a packet,
it returns –1 and sets errno. Since errno will not automatically be set, you must pick an
appropriate value. There is no standard error number to represent end-of-file. One possibility is
to use EINVAL. */
int getpacket(int fd, int *compidp, int *taskidp, packet_t *typep, int *lenp, unsigned char *buf)
{
    char *buff;
    char *bufptr;
    int i, linelen;
    taskpacket_t pack;
    FILE *fptr;

    if (!(fptr = fdopen(fd, "r")))
    {
        printf("Error opening file\n");
        return -1;
    }

    fprintf(stderr, "GetPacket!\n");
    fprintf(stderr, "Ready for packet!\n");

    fprintf(stderr, "Reading compid: ");
    *compidp = 0;
    if (fscanf(fptr, "%d", compidp) == EOF)
    {
        fprintf(stderr, "Error reading CompId!\n");
        return -1;
    }
    fgetc(fptr); // consume \n
    
    fprintf(stderr, "Reading taskid: ");
    *taskidp = 0;
    if (fscanf(fptr, "%d", taskidp) == EOF)
    {
        fprintf(stderr, "Error reading TaskId!\n");
        return -1;
    }
    fgetc(fptr); // consume \n
    
    for (i = 0; i < NUMTYPES; i++)
        fprintf(stderr, " %d = %s\n", i, typename[i]);
    fprintf(stderr, "Reading type: ");
    *typep = 0;
    if (fscanf(fptr, "%d", (int *) typep) == EOF)
    {
        fprintf(stderr, "Error reading Type!\n");
        return -1;
    }
    fgetc(fptr); // consume \n

    if (*typep != 0)
    {
        fprintf(stderr, "Got invalid packet! (%d), NEWTASK is allowed!\n", *typep);
        return -1;
    }

    pack.length = 0;
    buff = malloc(sizeof(char) * (MAX_PACK_SIZE + MAX_LINE_SIZE));
    bufptr = buff;
    *bufptr = 0;
    *buf = 0;
    fprintf(stderr, "Enter data ($ to terminate): ");
    while ((bufptr = fgets(bufptr, MAX_LINE_SIZE, fptr)) != NULL)
    {
        linelen = strlen(bufptr);

        if (linelen == 0)
            break;
        if (strcmp(TERMINATE_STRING, bufptr) == 0)
            break;
        
        bufptr = bufptr + linelen;
        pack.length = pack.length + linelen;
        
        if (pack.length >= MAX_PACK_SIZE)
        {
            fprintf(stderr, "***** Maximum packet size exceeded *****\n");
            return -1;
        }
        
        // fprintf(stderr, "Received=%d, total=%d, Enter line ($ to terminate):\n",
        //         linelen, pack.length);
        fprintf(stderr, "Enter data ($ to terminate): ");
    }

    // fprintf(stderr, "Pack length = %d\n", pack.length);
    *lenp = pack.length;
    strcpy(buf, buff);

    return 0;
}

/* The putpacket function assembles a taskpacket_t header from compid, taskid, type and
len. It then writes the packet header to fd followed by len bytes from buf. If successful,
putpacket returns 0. If unsuccessful, putpacket returns –1 and sets errno. */
int putpacket(int fd, int compid, int taskid, packet_t type, int len, unsigned char *buf)
{
    fprintf(stderr, "\nPutPacket!\n\n");
    // check the file descriptor
    if (fd == -1)
    {
        fprintf(stderr, "Invalid File Descriptor!\n");
        errno = EBADF; // bad file descriptor
        return -1;
    }
    if ((type < 0) || (type >= NUMTYPES))
    {
        fprintf(stderr, "Got invalid packet! (%d)\n", type);
        return 1;
    }
    // check the buffer length
    if (len >= MAX_PACK_SIZE)
    {
        fprintf(stderr, "Packet size should be < %d\n", MAX_PACK_SIZE);
        errno = EMSGSIZE; // message too long
        return -1;
    }

    // packet header
    taskpacket_t pack;
    int wsize = sizeof(taskpacket_t);
    pack.compid = compid;
    pack.taskid = taskid;
    pack.type = type;
    pack.length = len;
    // write packet header
    fprintf(stderr, "Writing packet header: %d %d %d %d\n", pack.compid, pack.taskid, (int) pack.type, pack.length);

    if (write(fd, &pack, wsize) != wsize)
    {
        // errno will be set by write syscall
        fprintf(stderr, "Error writing packet!\n");
        return -1;
    }
    // write buffer
    fprintf(stderr, "Writing %d bytes\n", pack.length);
    if (write(fd, buf, pack.length) != pack.length)
    {
        // errno will be set by write syscall
        fprintf(stderr, "Error writing packet!\n");
        return -1;
    }

    return 0;
}

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