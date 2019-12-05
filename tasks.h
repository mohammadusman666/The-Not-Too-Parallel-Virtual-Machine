#include "ntpvm.h"

typedef struct {
    ntpvm_task_t tasks[MAX_TASKS];
    int numOfTasks;
} task_array;

int addTask(task_array tasksarray);