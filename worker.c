//Created By: Matthew Kik
//Last Modified 08 Mar 2026
//Usage: Child processes for attached oss.c file, see attached README.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "shared.h"

int main(int argc, char **argv) {
	(void)argc;

	// Attach to shared memory
	int shm_id = shmget(SHM_KEY, CLOCK_SIZE, 0666);
	if (shm_id <= 0) {
		fprintf(stderr, "Error in Child Shared Memory.\n");
		exit(1);
	}

	int *shm_ptr = (int *)shmat(shm_id, NULL, 0);
	if (shm_ptr == (void *)-1) {
		fprintf(stderr, "Shared memory failed\n");
		exit(1);
	}

	// Attach to message queue
	int msqid = msgget(MSG_KEY, 0666);
	if (msqid == -1) {
		fprintf(stderr, "Error attaching to message queue.\n");
		exit(1);
	}

	int sec = shm_ptr[0];
	int nano = shm_ptr[1];

	int limitSec = atoi(argv[1]);
	int limitNano = atoi(argv[2]);

	int termSec = sec + limitSec;
	int termNano = nano + limitNano;

	if (termNano >= NANO_PER_SEC) {
		termSec++;
		termNano -= NANO_PER_SEC;
	}

	pid_t my_pid = getpid();
	pid_t parent_pid = getppid();


	//Starting status for workers
	printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n", my_pid, parent_pid, sec, nano);
	printf("TermTimeS: %d TermTimeNano: %d\n", termSec, termNano);
	printf("--Just Starting\n");

	int messagesReceived = 0;
	struct mymsg msg; //Called from shared.h

	do {
		msgrcv(msqid, &msg, sizeof(msg.mtext), my_pid, 0);
		messagesReceived++;

		if (shm_ptr[0] > termSec || (shm_ptr[0] == termSec && shm_ptr[1] >= termNano)) {
			printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n",
				my_pid, parent_pid, shm_ptr[0], shm_ptr[1]);
			printf("TermTimeS: %d TermTimeNano: %d\n", termSec, termNano);
			printf("--Terminating after sending message after %d received messages.\n",
				messagesReceived);
			msg.mtype = my_pid + REPLY_OFFSET;
			msg.mtext = 0;
			msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
			break;
		}
		else {
			printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n",
				my_pid, parent_pid, shm_ptr[0], shm_ptr[1]);
			printf("TermTimeS: %d TermTimeNano: %d\n", termSec, termNano);
			printf("--%d messages received\n", messagesReceived);
			msg.mtype = my_pid + REPLY_OFFSET;
			msg.mtext = 1;
			msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
		}
	} while (1);

	shmdt(shm_ptr);

	return EXIT_SUCCESS;
}
