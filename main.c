#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "helper.h"
#include "tasks.h"

#define MAX_LINE_SIZE 100
#define TERMINATE_STRING "$\n"
#define NUM_THREADS 1
// #define NUM_THREADS 2

static char *typename[] = {"Start Task", "Data", "Broadcast", "Done", "Terminate", "Barrier"};

int getpacket(int, int *, int *, packet_t *, int *, unsigned char *);
int putpacket(int, int, int, packet_t, int, unsigned char *);
void *inputFunc(void *param);
void *outputFunc(void *param);

int main(void)
{
    task_array tasks;
    int i; // iterator
    pthread_t input_thread, output_thread;
    pthread_t threads[NUM_THREADS]; // array of threads to be joined upon
    int threadCreationResult;

    // initialize tasks array object
    memset(tasks.tasks, 0, sizeof(tasks.tasks));
    tasks.numOfTasks = 0;

    // 2nd argument is NULL indicating to create thread with default attributes
    // 4th argument is NULL indicating no arguments to the inputFunc function
    if (threadCreationResult = pthread_create(&input_thread, NULL, inputFunc, &tasks))
    {
        fprintf(stderr, "%s", strerror(errno));
        return -1;
    }
    // if (threadCreationResult = pthread_create(&output_thread, NULL, outputFunc, NULL))
    // {
    //     fprintf(stderr, "%s", strerror(errno));
    //     return -1;
    // }

    threads[0] = input_thread;
    // threads[1] = output_thread;

    // block until all threads complete
	for (i = 0; i < NUM_THREADS; i++)
	{
		pthread_join(threads[i], NULL); // wait for thread to execute
	}
    
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

    if ((*typep != 0) && (*typep != 1) && *typep != 3)
    {
        fprintf(stderr, "Got invalid packet! (%d)\n", *typep);
        return 1;
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

void *inputFunc(void *param)
{
    task_array *tasksarray;
    tasksarray = (task_array *) param;

    unsigned char buf[MAX_PACK_SIZE];
    int compid, taskid, tdatalen, result;
    packet_t type;
    int tin, tout;
    int error;

    char delim[] = " \t\n"; // delimeters
    int i; // iterator
    char **myargv; // argument array
    int numtokens; // number of tokens

    /***** CHILD & PIPE *****/
    pid_t childpid;
    int mypipefd[2];

    int inputPipe[2];
    int outputPipe[2];
    /***** CHILD & PIPE *****/

    tin = STDIN_FILENO;
    tout = STDOUT_FILENO;

    while ((result = getpacket(tin, &compid, &taskid, &type, &tdatalen, buf)) != -1)
    {
        fprintf(stderr, "\n\n%d %d %d %d %s\n\n", compid, taskid, type, tdatalen, buf);

        // 1 is returned by getpacket when BROADCAST, BARRIER or TERMINATE are received
        if (result == 1)
            continue;

        /* 0 => NEWTASK */
        if (type == 0)
        {
            /* If a child task is already executing, discard the packet and output an error message */
            if (tasksarray->numOfTasks >= 1)
            {
                fprintf(stderr, "\n\n***** Maximum number of tasks reached! *****\n\n");
                continue;
            }
            /* if no child task exists, create two pipes to handle the task's input and output
            Update the tasks object, and fork a child. The child should redirect its standard input
            and output to the pipes and use the makeargv function of Program 2.2 to construct the
            argument array before calling execvp to execute the command given in the packet
            Create a detached output thread by calling pthread_create. Pass a key for the tasks
            entry of this task as an argument to the output thread. The key is just the index of the
            appropriate tasks array entry */
            else
            {
                int taskind;
                /* create input pipe */
                if (pipe(inputPipe) < 0)
                {
                    fprintf(stderr, "%s", strerror(errno));
                    return NULL;
                }
                /* create output pipe */
                if (pipe(outputPipe) < 0)
                {
                    fprintf(stderr, "%s", strerror(errno));
                    return NULL;
                }
                // add new task in tasks array
                // this call is to be changed, first we have to create 2 pipes and then pass them to addTask
                // if ((taskind = addTask(tasksarray, compid, taskid, mypipefd[1], mypipefd[0])) == -1)
                if ((taskind = addTask(tasksarray, compid, taskid, inputPipe[1], outputPipe[0])) == -1)
                {
                    fprintf(stderr, "\nError in adding a new task!\n");
                    return NULL;
                }

                // after creating the output thread, pass the task object returned by the addTask to the thread
                // so that it can update the output thread id attribute of the task object

                /* pipes have been created */
                childpid = fork();
                if (childpid < 0)
                {
                    fprintf(stderr, "%s", strerror(errno));
                    return NULL;
                }
                else if (childpid > 0)
                {
                    wait(NULL); // wait for child
                    tasksarray->tasks[taskind]->taskpid = childpid; // set child task PID
                    // close unneeded pipes
                    close(inputPipe[0]);
                    close(outputPipe[1]);

                    char concat_str[MAX_PACK_SIZE];

                    // close(mypipefd[1]); // cloade write end
                    // read(mypipefd[0], concat_str, MAX_PACK_SIZE); // read from pipe
                    // // printf("Concatenated string %s\n", concat_str);
                    // close(mypipefd[0]);

                    type = 1; // 1 => DATA
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
                    // dup2(mypipefd[1], 1);
                    // dup2(mypipefd[0], 0);

                    dup2(inputPipe[1], 0);
                    dup2(outputPipe[0], 1);

                    // close(mypipefd[0]);
                    // close(mypipefd[1]);

                    /* now exec the new image */
                    // we make the the argument array
                    if ((numtokens = makeargv(buf, delim, &myargv)) == -1)
                    {
                        fprintf(stderr, "Failed to construct an argument array for %s\n", buf);
                        return NULL;
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
        }
        /* 1 => DATA */
        else if (type == 1)
        {
            /* check the task in tasks array */
            // result will have the index if found in tasks array
            result = checkTask(tasksarray, compid, taskid); // check the task in tasks array
            if ((result == -1) || (result == -2))
            {
                fprintf(stderr, "\n\nComputation Id and Task Id don't match or endinput is true!\n\n");
                continue;
            }
            else
            {
                /* copy the data portion to writefd */
                // if ((error = r_write(mypipefd[1], buf, tdatalen)) == -1)
                if ((error = r_write(inputPipe[1], buf, tdatalen)) == -1)
                {
                    fprintf(stderr, "%s", strerror(errno));
                    break; // break the loop and exit
                }

                /* Update the recvpackets and recvbytes members of the appropriate task entry of the tasks object */
                tasksarray->tasks[result]->recvpackets += 1;
                tasksarray->tasks[result]->recvbytes += tdatalen;
            }
        }
        /* 3 => DONE */
        else if (type == 3)
        {
            /* check the task in tasks array */
            // result will have the index if found in tasks array
            result = checkTask(tasksarray, compid, taskid);
            if (result == -1)
            {
                fprintf(stderr, "\n\nComputation Id and Task Id don't match!\n\n");
                continue;
            }
            else
            {
                /* close the writefd descriptor if it is still open */
                // while (((error = close(mypipefd[1])) == -1) && (errno == EINTR));
                while (((error = close(inputPipe[1])) == -1) && (errno == EINTR));
                // if (error== -1)
                // {
                //     ;
                // }
                /* set the endinput member for this task entry */
                tasksarray->tasks[result]->endinput = 1;
                tasksarray->tasks[result] = NULL;
            }
        }
    }

    // close pipe read & write ends
    // close(mypipefd[0]);
    // close(mypipefd[1]);

    while (((error = close(inputPipe[1])) == -1) && (errno == EINTR));

    pthread_exit(0);
}

void *outputFunc(void *param)
{
    pthread_exit(0);
}