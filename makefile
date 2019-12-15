FLAGS = -pthread

all: ntpvm

ntpvm: helper.c  helper.h  main.c  ntpvm.h  tasks.c  tasks.h
	gcc $(FLAGS) main.c helper.c tasks.c -o ntpvm

clean:
	rm -f *.o

