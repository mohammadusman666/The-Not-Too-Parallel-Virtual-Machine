/***************

Group Members:
Mohammad Usman (20-10558)
Muhammad Omer Khalil (20-10671)

Course:
CSCS440 Systems Programming

Term:
Fall 2019

***************/

#include "ntpvm.h"

typedef struct {
    ntpvm_task_t *tasks[MAX_TASKS]; // array of pointers to tasks
    int numOfTasks;
} task_array;

int findFreePlace(task_array *tasksarray);

int addTask(task_array *tasksarray, int compid, int taskid, int writefd, int readfd);

int checkTask(task_array *tasksarray, int compid, int taskid);