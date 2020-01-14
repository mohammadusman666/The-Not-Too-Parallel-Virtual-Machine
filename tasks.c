/***************

Group Members:
Mohammad Usman (20-10558)
Muhammad Omer Khalil (20-10671)

Course:
CSCS440 Systems Programming

Term:
Fall 2019

***************/

#include <stdio.h>
#include <stdlib.h>
#include "tasks.h"

/* returns index of 1st element that is NULL
or returns -1 if none */
int findFreePlace(task_array *tasksarray)
{
    ntpvm_task_t *task;
    int i;

    for (i = 0; i < MAX_TASKS; i++)
    {
        task = tasksarray->tasks[i];

        if (!task)
        {
            return i;
        }
    }
    // if not found
    return -1;
}

/* returns index of task if added successfully
or returns -1 in case of an error */
int addTask(task_array *tasksarray, int compid, int taskid, int writefd, int readfd)
{
    int index;

    // if compid, taskid already exists
    index = checkTask(tasksarray, compid, taskid);
    if ((index != -1) && (index != -2))
    {
        return -1;
    }
    // if no free place is available
    if ((index = findFreePlace(tasksarray)) == -1)
    {
        return -1;
    }
    else
    {
        ntpvm_task_t *task;
        
        if ((task = (ntpvm_task_t *) malloc(sizeof(ntpvm_task_t))) == NULL)
        {
            return -1;
        }
        else
        {
            task->compid = compid;
            task->taskid = taskid;
            task->writefd = writefd;
            task->readfd = readfd;
            task->recvbytes = 0;
            task->recvpackets = 0;
            task->sentbytes = 0;
            task->sentpackets = -1;

            task->taskpid = -1;
            task->tasktid = -1;
            task->barrier = -1;
            // task->mlock = -1;

            task->endinput = 0;

            tasksarray->tasks[index] = task;

            return index;
        }
    }
}

/* returns index if compid and taskid match
or returns -1 if compid and taskid don't match
or returns -2 if endinput is true */
int checkTask(task_array *tasksarray, int compid, int taskid)
{
    ntpvm_task_t *task;
    int i;

    for (i = 0; i < MAX_TASKS; i++)
    {
        task = tasksarray->tasks[i];

        if (task)
        {
            // if it matches and endinput is false
            if ((task->compid == compid) && (task->taskid == taskid) && (task->endinput == 0))
            {
                return i;
            }
            // if it matches and endinput is true
            else if ((task->compid == compid) && (task->taskid == taskid) && (task->endinput == 1))
            {
                return -2;
            }
        }
    }
    // if it doesn't matches
    return -1;
}