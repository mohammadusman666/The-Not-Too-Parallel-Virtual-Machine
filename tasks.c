#include <stdio.h>
#include "tasks.h"

int addTask(task_array tasksarray)
{
    if (tasksarray.numOfTasks >= MAX_TASKS)
    {
        fprintf(stderr, "***** Maximum task limit exceeded *****\n");
        return -1;
    }

    
}