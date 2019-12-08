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

task_array tasks; // tasks array

int main(void)
{
    int i; // iterator
    pthread_t input_thread;
    pthread_t threads[NUM_THREADS]; // array of threads to be joined upon
    int threadCreationResult;

    // initialize tasks array object
    memset(tasks.tasks, 0, sizeof(tasks.tasks));
    tasks.numOfTasks = 0;

    // 2nd arg NULL -> default attributes
    if (threadCreationResult = pthread_create(&input_thread, NULL, inputFunc, &tasks))
    {
        fprintf(stderr, "%s", strerror(errno));
        return -1;
    }

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

    fprintf(stderr, "Enter compid: ");
    *compidp = 0;
    if (fscanf(fptr, "%d", compidp) == EOF)
    {
        fprintf(stderr, "Error reading CompId!\n");
        errno = EINVAL;
        return -1;
    }
    fgetc(fptr); // consume \n
    
    fprintf(stderr, "Enter taskid: ");
    *taskidp = 0;
    if (fscanf(fptr, "%d", taskidp) == EOF)
    {
        fprintf(stderr, "Error reading TaskId!\n");
        errno = EINVAL;
        return -1;
    }
    fgetc(fptr); // consume \n
    
    for (i = 0; i < NUMTYPES; i++)
        fprintf(stderr, " %d = %s\n", i, typename[i]);
    fprintf(stderr, "Enter type: ");
    *typep = 0;
    if (fscanf(fptr, "%d", (int *) typep) == EOF)
    {
        fprintf(stderr, "Error reading Type!\n");
        errno = EINVAL;
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
            fprintf(stderr, "Maximum packet size exceeded\n");
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
    // fprintf(stderr, "\nPutPacket!\n\n");
    // check the file descriptor
    if (fd < 0)
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

/* The input thread monitors standard input and takes action according to the input it receives.
Write an input function that executes the following steps in a loop until it encounters anend-offile on standard input.
1. Read a packet from standard input by using getpacket.
2. Process the packet.
After falling through the loop, close writefd and call pthread_exit.
Processing a packet depends on the packet type */
void *inputFunc(void *param)
{
    task_array *tasksarray;
    tasksarray = (task_array *) param;

    /***** TASK *****/
    unsigned char buf[MAX_PACK_SIZE];
    int compid, taskid, tdatalen, result;
    packet_t type;
    int tin, tout;
    int error;
    /***** TASK *****/

    /***** MAKEARGV *****/
    char delim[] = " \t\n";
    int i;
    char **myargv;
    int numtokens;
    /***** MAKEARGV *****/

    /***** CHILD, THREAD & PIPE *****/
    pid_t childpid;

    int threadCreationResult;
    pthread_t output_thread;

    int inputPipe[2];
    int outputPipe[2];
    /***** CHILD, THREAD & PIPE *****/

    tin = STDIN_FILENO;
    tout = STDOUT_FILENO;

    while (((result = getpacket(tin, &compid, &taskid, &type, &tdatalen, buf)) != -1) && (errno != EINVAL))
    {
        fprintf(stderr, "Get Packet: %d %d %d %d %s\n", compid, taskid, type, tdatalen, buf);

        // 1 is returned by getpacket when BROADCAST, BARRIER or TERMINATE are received
        /* 1. Output an error message.
        2. Discard the packet. */
        if (result == 1)
            continue;

        /* 0 => NEWTASK
        1. If a child task is already executing, discard the packet and output an error message.
        2. Otherwise, if no child task exists, create two pipes to handle the task's input and output.
        3. Update the tasks object, and fork a child. The child should redirect its standard input
        and output to the pipes and use the makeargv function to construct the argument array before
        calling execvp to execute the command given in the packet.
        4. Create a detached output thread by calling pthread_create. Pass a key for the tasks
        entry of this task as an argument to the output thread. The key is just the index of the
        appropriate tasks array entry */
        if (type == 0)
        {
            /* If a child task is already executing, discard the packet and output an error message */
            if (tasksarray->numOfTasks >= 1)
            {
                fprintf(stderr, "Maximum number of tasks reached!\n");
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
                /* create input & output pipes */
                if ((pipe(inputPipe) < 0) || (pipe(outputPipe) < 0))
                {
                    fprintf(stderr, "%s", strerror(errno));
                    return NULL;
                }
                // add new task in tasks array
                // if ((taskind = addTask(tasksarray, compid, taskid, mypipefd[1], mypipefd[0])) == -1)
                if ((taskind = addTask(tasksarray, compid, taskid, inputPipe[1], inputPipe[0])) == -1)
                {
                    fprintf(stderr, "Error in adding a new task!\n");
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
                /* parent code */
                else if (childpid > 0)
                {
                    tasksarray->numOfTasks += 1;
                    tasksarray->tasks[taskind]->taskpid = childpid; // set child task PID
                    // close unneeded pipes
                    // close(outputPipe[0]);
                    close(outputPipe[1]);

                    // 2nd arg NULL -> default attributes
                    if (threadCreationResult = pthread_create(&output_thread, NULL, outputFunc, &taskind))
                    {
                        fprintf(stderr, "%s", strerror(errno));
                        return NULL;
                    }

                    wait(NULL); // wait for child

                    // char concat_str[MAX_PACK_SIZE];
                    // r_read(inputPipe[0], buf, MAX_PACK_SIZE);

                    // type = 1; // 1 => DATA
                    // tdatalen = strlen(buf);
                    // *buf = 0;
                    // strcpy(buf, concat_str);
                    // if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
                    //     break;
                }
                /* child code */
                else
                {
                    /* redirect stdin & stdout to pipe */
                    dup2(outputPipe[0], 0);
                    dup2(inputPipe[1], 1);

                    /* now exec the new image */
                    // we make the the argument array
                    if ((numtokens = makeargv(buf, delim, &myargv)) == -1)
                    {
                        fprintf(stderr, "Failed to construct an argument array for %s\n", buf);
                        return NULL;
                    }
                    // print argument array
                    myargv[numtokens-1] = NULL; // make $ = null
                    
                    // a null terminated array of character pointers
                    fprintf(stderr, "Executing...\n");
                    execvp(myargv[0], myargv);
                }
            }
        }
        /* 1 => DATA
        1. If the packet's communication and task IDs don't match those of the executing task or if
        the task's endinput is true, output an error message and discard the packet.
        2. Otherwise, copy the data portion to writefd.
        3. Update the recvpackets and recvbytes members of the appropriate task entry of the
        tasks object. */
        else if (type == 1)
        {
            /* check the task in tasks array */
            // result will have the index if found in tasks array
            result = checkTask(tasksarray, compid, taskid); // check the task in tasks array
            if ((result == -1) || (result == -2))
            {
                fprintf(stderr, "Computation Id and Task Id don't match or endinput is true!\n");
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
        /* 3 => DONE
        1. If the packet's computation and task IDs do not match those of the executing task,
        output an error message and discard the packet.
        2. Otherwise, close the writefd descriptor if it is still open.
        3. Set the endinput member for this task entry. */
        else if (type == 3)
        {
            /* check the task in tasks array */
            // result will have the index if found in tasks array
            result = checkTask(tasksarray, compid, taskid);
            if (result == -1)
            {
                fprintf(stderr, "Computation Id and Task Id don't match!\n");
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
                // tasksarray->tasks[result] = NULL;
                // tasksarray->numOfTasks -= 1;
            }
        }
    }

    // close pipe write end
    while (((error = close(inputPipe[1])) == -1) && (errno == EINTR));

    pthread_exit(0);
}

/* The output thread handles input from the readfd descriptor of a particular task. The output
thread receives a tasks object key to the task it monitors as a parameter. Write an output
function that executes the following steps in a loop until it encounters an end-of-file on readfd.
1. Read data from readfd.
2. Call putpacket to construct a DATA packet and send it to standard output.
3. Update the sentpackets and sentbytes members of the appropriate task entry in the tasks object. */
void *outputFunc(void *param)
{
    /***** TASK *****/
    unsigned char buf[MAX_PACK_SIZE];
    int key, compid, taskid, tdatalen, recvbytes;
    packet_t type;
    int tout, error;
    /***** TASK *****/

    key = *((int *) param);
    ntpvm_task_t *task = tasks.tasks[key];
    task->tasktid = pthread_self();
    compid = task->compid;
    taskid = task->taskid;
    recvbytes = task->recvbytes;
    tout = STDOUT_FILENO;

    while ((error = r_read(task->readfd, buf, MAX_PACK_SIZE)) != 0)
    {
        type = 1; // 1 => DATA
        tdatalen = strlen(buf);
        if (!tdatalen)
            continue;

        if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
            break;

        recvbytes = task->recvbytes - recvbytes;
        task->sentbytes += recvbytes;
        // packets += 1;
        task->sentpackets += 1;

        fprintf(stderr, "Sent: %d %d\n", task->sentbytes, task->sentpackets);

        fprintf(stderr, "\nTask Information:\n");
        fprintf(stderr, "CompID: %d, TaskID: %d\n", compid, taskid);
        fprintf(stderr, "Bytes Sent: %d, Packets Sent: %d\n", task->sentbytes, task->sentpackets);
        fprintf(stderr, "Bytes Received: %d, Packets Received: %d\n\n", task->recvbytes, task->recvpackets);
    }

    /* After falling through the loop because of an end-of-file or an error on readfd, the output thread
    does the following */
    // 1. Close the readfd and writefd descriptors for the task
    while (((error = close(task->writefd)) == -1) && (errno == EINTR));
    while (((error = close(task->readfd)) == -1) && (errno == EINTR));
    
    // 2. Execute wait for the child task
    wait(NULL);

    // 3. Send a DONE packet with the appropriate computation and task IDs to standard output
    type = 3; // 3 => DONE
    tdatalen = 0;
    *buf = 0;
    if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
        return NULL;

    /* 4. Output information about the finished task to standard error or to the remote logger.
    Include the computation ID, the task ID, the total bytes sent by the task, the total
    packets sent by the task, the total bytes received by the task and the total packets
    received by the task */
    fprintf(stderr, "\nTask Information:\n");
    fprintf(stderr, "CompID: %d, TaskID: %d\n", compid, taskid);
    fprintf(stderr, "Bytes Sent: %d, Packets Sent: %d\n", task->sentbytes, task->sentpackets);
    fprintf(stderr, "Bytes Received: %d, Packets Received: %d\n\n", task->recvbytes, task->recvpackets);

    // 5. Deactivate the task entry by setting the computation ID to –
    // task->compid = '-'; commented bcz I'm currently setting the entry to NULL when DONE packet is sent
    tasks.tasks[key] = NULL;
    tasks.numOfTasks -= 1;

    // 6. Call pthread_exit
    pthread_exit(0);
}