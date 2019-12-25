#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "helper.h"
#include "tasks.h"

#define MAX_LINE_SIZE 100
#define TERMINATE_STRING "$\n"

static volatile sig_atomic_t doneFlag = 0;

static char *typename[] = {"Start Task", "Data", "Broadcast", "Done", "Terminate", "Barrier"};

int getpacket(int, int *, int *, packet_t *, int *, unsigned char *);
int putpacket(int, int, int, packet_t, int, unsigned char *);
void *inputFunc(void *param);
void *outputFunc(void *param);

task_array tasks; // tasks array
pthread_mutex_t lock; // thread mutex lock

static void intHandler(int signo)
{
    doneFlag = 1;
}

int main(void)
{
    int i; // iterator
    pthread_t input_thread;
    int threadCreationResult;
    struct sigaction act;

    // initialize tasks array object
    memset(tasks.tasks, 0, sizeof(tasks.tasks));
    tasks.numOfTasks = 0;

    // Ctrl-C handler
    act.sa_handler = intHandler;
    act.sa_flags = 0;
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1))
    {
        fprintf(stderr, "Failed to install SIGINT signal handler\n");
        return -1;
    }

    // create the mutex lock
    // 2nd arg NULL -> default attributes
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("Mutex init has failed!\n");
        return -1;
    }
    // create the input thread
    // 2nd arg NULL -> default attributes
    if (threadCreationResult = pthread_create(&input_thread, NULL, inputFunc, &tasks))
    {
        fprintf(stderr, "%s", strerror(errno));
        return -1;
    }

    // block until all threads complete
    pthread_join(input_thread, NULL); // wait for thread to execute

    pthread_mutex_destroy(&lock); // destroy thread mutex lock

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
    {
        fprintf(stderr, " %d = %s\n", i, typename[i]);
    }
    fprintf(stderr, "Enter type: ");
    *typep = 0;
    if (fscanf(fptr, "%d", (int *) typep) == EOF)
    {
        fprintf(stderr, "Error reading Type!\n");
        errno = EINVAL;
        return -1;
    }
    fgetc(fptr); // consume \n

    if ((*typep < 0) && (*typep > 5))
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
        {
            break;
        }
        if (strcmp(TERMINATE_STRING, bufptr) == 0)
        {
            break;
        }
        
        bufptr = bufptr + linelen;
        pack.length = pack.length + linelen;
        
        if (pack.length >= MAX_PACK_SIZE)
        {
            fprintf(stderr, "Maximum packet size exceeded\n");
            return -1;
        }
        
        fprintf(stderr, "Enter data ($ to terminate): ");
    }

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

    int writefdarray[MAX_TASKS];

    /***** TASK *****/
    unsigned char buf[MAX_PACK_SIZE];
    int compid, taskid, tdatalen, result;
    packet_t type;
    int tin, tout;
    int error;
    /***** TASK *****/

    /***** MAKEARGV *****/
    char delim[] = " \t\n";
    char **myargv;
    int numtokens;
    /***** MAKEARGV *****/

    /***** CHILD, THREAD & PIPE *****/
    pid_t childpid;

    int threadCreationResult;
    pthread_t output_thread;
    /***** CHILD, THREAD & PIPE *****/

    tin = STDIN_FILENO;
    tout = STDOUT_FILENO;

    while (((result = getpacket(tin, &compid, &taskid, &type, &tdatalen, buf)) != -1) && (errno != EINVAL) && (!doneFlag))
    {
        fprintf(stderr, "Get Packet: %d %d %d %d %s\n", compid, taskid, type, tdatalen, buf);

        // 1 is returned if packet if of invalid type
        if (result == 1)
        {
            continue;
        }

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
            if (tasksarray->numOfTasks >= 10)
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
                int inputPipe[2], outputPipe[2];
                /* create input & output pipes */
                if ((pipe(inputPipe) < 0) || (pipe(outputPipe) < 0))
                {
                    fprintf(stderr, "%s", strerror(errno));
                    return NULL;
                }
                // add new task in tasks array
                if ((taskind = addTask(tasksarray, compid, taskid, inputPipe[1], inputPipe[0])) == -1)
                {
                    fprintf(stderr, "Error in adding a new task!\n");
                    return NULL;
                }
                writefdarray[taskind] = inputPipe[1]; // save the task's writefd

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
                // if ((error = r_write(writefdarray[result], buf, tdatalen)) == -1)
                // {
                //     fprintf(stderr, "%s", strerror(errno));
                //     break; // break the loop and exit
                // }
                if (putpacket(writefdarray[result], compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
                {
                    continue;
                }

                /* Update the recvpackets and recvbytes members of the appropriate task entry of the tasks object */
                tasksarray->tasks[result]->recvpackets += 1;
                tasksarray->tasks[result]->recvbytes += tdatalen;
            }
        }
        /* 2 => BROADCAST
        When the dispatcher receives a BROADCAST request from standard input, it forwards the packet
        on the writefd descriptors for each task whose computation ID matches that of the BROADCAST
        packet. If the dispatcher receives a BROADCAST request from one of the readfd descriptors, it
        forwards the packet on the writefd descriptors for each task whose computation ID matches
        that in the BROADCAST packet. Since, in a future extension, tasks from the computation may
        reside on other hosts, the dispatcher also forwards the packet on its standard output.
        */
        else if (type == 2)
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
                int i;
                ntpvm_task_t *task;

                for (i = 0; i < MAX_TASKS; i++)
                {
                    task = tasksarray->tasks[i];

                    if (task)
                    {
                        // if compid matches
                        if ((task->compid == compid))
                        {
                            if (putpacket(writefdarray[i], compid, taskid, type, tdatalen, buf) == -1)
                            {
                                continue;
                            }
                            /* Update the recvpackets and recvbytes members of the appropriate task entry of the tasks object */
                            tasksarray->tasks[i]->recvpackets += 1;
                            tasksarray->tasks[i]->recvbytes += tdatalen;
                        }
                    }
                }

                // the dispatcher also forwards the broadcast packet on its standard output
                if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1)
                {
                    continue;
                }
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
                while (((error = close(writefdarray[result])) == -1) && (errno == EINTR));

                /* set the endinput member for this task entry */
                tasksarray->tasks[result]->endinput = 1;
            }
        }
        /* 4 => TERMINATE */
        else if (type == 4)
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
                while (((error = close(writefdarray[result])) == -1) && (errno == EINTR));

                /* set the endinput member for this task entry */
                tasksarray->tasks[result]->endinput = 1;
            }
        }
        /* 5 => BARRIER
        When the dispatcher receives a BARRIER packet from a task, it sets the barrier member for
        that task to the barrier number specified by the packet data. When all the tasks in a
        computation have reported that they are waiting for the barrier, the dispatcher sends a
        BARRIER message on standard output.
        When the dispatcher reads a BARRIER packet for that barrier number from standard input, it
        resets the barrier member to –1 and sends a SIGUSR1 signal to all the tasks in the
        computation. The BARRIER packet from standard input signifies that all tasks in the computation
        are waiting at the designated barrier and that they can be released. Assume that the dispatcher
        never receives a second BARRIER packet from standard input before it has forwarded a
        corresponding BARRIER packet on standard output.
        Implement the barrier on the task side by blocking the SIGUSR1 signal, writing a BARRIER
        packet to standard output, and then executing sigsuspend in a loop until the SIGUSR1 signal
        arrives. */
        else if (type == 5)
        {
            // result = checkTask(tasksarray, compid, taskid); // check the task in tasks array
            // if (result == -1)
            // {
            //     fprintf(stderr, "Computation Id and Task Id don't match or endinput is true!\n");
            // }
            // else
            // {
            //     int barrierNum;

            //     error = sscanf(buf, "%d", &barrierNum);
            //     // if no error occurs
            //     if ((error > 0) && (error != EOF))
            //     {
            //         fprintf(stderr, "%s%d\n", "Barrier Number: ", barrierNum);

            //         tasksarray->tasks[result]->barrier = barrierNum;
            //     }
            // }
        }
    }

    ntpvm_task_t *taski;
    int i;

    fprintf(stderr, "\nProgram terminating ...\n\n");

    for (i = 0; i < MAX_TASKS; i++)
    {
        taski = tasks.tasks[i];

        if (taski)
        {
            while (((error = close(taski->writefd)) == -1) && (errno == EINTR));
            while (((error = close(taski->readfd)) == -1) && (errno == EINTR));
        }
    }

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

    unsigned char tempBuf[MAX_PACK_SIZE];
    /***** TASK *****/

    key = *((int *) param);
    ntpvm_task_t *task = tasks.tasks[key];
    task->tasktid = pthread_self();
    compid = task->compid;
    taskid = task->taskid;
    recvbytes = task->recvbytes;
    tout = STDOUT_FILENO;

    // 1st time the task is run
    while ((error = r_read(task->readfd, buf, MAX_PACK_SIZE)) != 0)
    {
        pthread_mutex_lock(&lock); // lock thread mutex
        task->mlock = lock; // save thread mutex

        type = 1; // 1 => DATA
        tdatalen = strlen(buf);
        if (!tdatalen)
        {
            continue;
        }

        if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
        {
            break;
        }

        recvbytes = task->recvbytes - recvbytes;
        task->sentbytes += recvbytes;
        task->sentpackets += 1;

        fprintf(stderr, "Sent: %d %d\n", task->sentbytes, task->sentpackets);

        fprintf(stderr, "\nTask Information:\n");
        fprintf(stderr, "CompID: %d, TaskID: %d\n", compid, taskid);
        fprintf(stderr, "Bytes Sent: %d, Packets Sent: %d\n", task->sentbytes, task->sentpackets);
        fprintf(stderr, "Bytes Received: %d, Packets Received: %d\n\n", task->recvbytes, task->recvpackets);

        pthread_mutex_unlock(&lock); // unlock thread mutex
        // task->mlock = NULL; // remove thread mutex
        break;
    }
    fprintf(stderr, "%s\n\n", "Out of 1st loop!");

    taskpacket_t pack;
    int wsize = sizeof(taskpacket_t);

    while ((error = r_read(task->readfd, &pack, wsize)) != 0)
    {
        // pthread_mutex_lock(&lock); // lock thread mutex
        task->mlock = lock; // save thread mutex

        fprintf(stderr, "Output Thread: %d %d %d %d\n", pack.compid, pack.taskid, pack.type, pack.length);

        r_read(task->readfd, &tempBuf, pack.length);

        fprintf(stderr, "%s\n", tempBuf);

        /* 1 => DATA */
        if (pack.type == 1)
        {
            if (!pack.length)
            {
                continue;
            }

            if (putpacket(tout, pack.compid, pack.taskid, pack.type, pack.length, tempBuf) == -1) // 4th argument (1 = DATA)
            {
                break;
            }

            recvbytes = task->recvbytes - recvbytes;
            task->sentbytes += recvbytes;
            task->sentpackets += 1;
        }
        /* 2 => BROADCAST */
        else if (pack.type == 2)
        {
            int i;
            ntpvm_task_t *taski;

            if (!pack.length)
            {
                continue;
            }

            for (i = 0; i < MAX_TASKS; i++)
            {
                taski = tasks.tasks[i];

                if (taski)
                {
                    // if compid matches
                    if ((taski->compid == pack.compid) && (taski->taskid) != taskid)
                    {
                        fprintf(stderr, "Here!");
                        if (putpacket(taski->writefd, pack.compid, pack.taskid, pack.type, pack.length, tempBuf) == -1)
                        {
                            continue;
                        }
                        /* Update the recvpackets and recvbytes members of the appropriate task entry of the tasks object */
                        tasks.tasks[i]->recvpackets += 1;
                        tasks.tasks[i]->recvbytes += tdatalen;
                    }
                }
                // task->sentbytes += recvbytes;
                // task->sentpackets += 1;
            }
        }

        fprintf(stderr, "Sent: %d %d\n", task->sentbytes, task->sentpackets);

        fprintf(stderr, "\nTask Information:\n");
        fprintf(stderr, "CompID: %d, TaskID: %d\n", compid, taskid);
        fprintf(stderr, "Bytes Sent: %d, Packets Sent: %d\n", task->sentbytes, task->sentpackets);
        fprintf(stderr, "Bytes Received: %d, Packets Received: %d\n\n", task->recvbytes, task->recvpackets);

        // pthread_mutex_unlock(&lock); // unlock thread mutex
        // task->mlock = NULL; // remove thread mutex
    }
    fprintf(stderr, "%s\n\n", "Out of 2nd loop!");

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
    pthread_mutex_lock(&lock); // lock thread mutex
    task->mlock = lock; // save thread mutex

    if (putpacket(tout, compid, taskid, type, tdatalen, buf) == -1) // 4th argument (1 = DATA)
    {
        return NULL;
    }

    /* 4. Output information about the finished task to standard error or to the remote logger.
    Include the computation ID, the task ID, the total bytes sent by the task, the total
    packets sent by the task, the total bytes received by the task and the total packets
    received by the task */
    fprintf(stderr, "\nTask Information:\n");
    fprintf(stderr, "CompID: %d, TaskID: %d\n", compid, taskid);
    fprintf(stderr, "Bytes Sent: %d, Packets Sent: %d\n", task->sentbytes, task->sentpackets);
    fprintf(stderr, "Bytes Received: %d, Packets Received: %d\n\n", task->recvbytes, task->recvpackets);

    pthread_mutex_unlock(&lock); // unlock thread mutex

    // 5. Deactivate the task entry by setting the computation ID to –
    // task->compid = '-';
    tasks.tasks[key] = NULL;
    tasks.numOfTasks -= 1;

    // 6. Call pthread_exit
    pthread_exit(0);
}