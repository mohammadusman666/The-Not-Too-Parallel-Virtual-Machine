#include "ntpvm.h"

typedef struct {
    ntpvm_task_t *tasks[MAX_TASKS]; // array of pointers to tasks
    int numOfTasks;
} task_array;

int findFreePlace(task_array *tasksarray);

int addTask(task_array *tasksarray, int compid, int taskid, int writefd, int readfd);

int checkTask(task_array *tasksarray, int compid, int taskid);