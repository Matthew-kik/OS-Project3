//Created by: Matthew Kik
//Last Modified: 08 Mar 2026
//Usage: See below or read attached README utilize Makefile for compiling 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include "shared.h"

struct PCB processTable[MAX_PROCESSES];
int shmid;
int msqid;
int *shm_ptr;

// Cleans up shared memory, message queue, and kills children
void kill_processes(int sig) {
	for (int i = 0; i < MAX_PROCESSES; i++) {
		if (processTable[i].occupied) {
			kill(processTable[i].pid, SIGTERM);
		}
	}
	shmdt(shm_ptr);
	shmctl(shmid, IPC_RMID, NULL);
	msgctl(msqid, IPC_RMID, NULL);
	exit(1);
}

void usage(const char *app) {
	fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i interval] [-f logfile]\n", app);
	fprintf(stderr, "	-n proc, sets total number of children to launch.\n");
	fprintf(stderr, "	-s simul, max number of children running simultaneously.\n");
	fprintf(stderr, "	-t timeLimit, upper bound of time for children (e.g. 7 or 7.3).\n");
	fprintf(stderr, "	-i interval, interval in seconds between child launches (e.g. 0.5).\n");
	fprintf(stderr, "	-f logfile, path to log file for oss output.\n");
}

int main(int argc, char **argv) {

	signal(SIGALRM, kill_processes);
	signal(SIGINT, kill_processes);
	alarm(60);

	int proc = 0;
	int simul = 0;
	float timeLimit = 0;
	float interval = 0;
	char *logfile = NULL;

	char opt;
	while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
		switch (opt) {
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'n':
				proc = atoi(optarg);
				break;
			case 's':
				simul = atoi(optarg);
				break;
			case 't':
				timeLimit = atof(optarg);
				break;
			case 'i':
				interval = atof(optarg);
				break;
			case 'f':
				logfile = optarg;
				break;
			default:
				printf("Incorrect input, please follow the usage below.\n");
				usage(argv[0]);
				return EXIT_FAILURE;
		}
	}

	// Setup shared memory
	shmid = shmget(SHM_KEY, CLOCK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "Error in generating shared memory for parent.\n");
		exit(1);
	}

	shm_ptr = (int *)shmat(shmid, NULL, 0);
	if (shm_ptr == (void *)-1) {
		fprintf(stderr, "Failed to attach memory\n");
		exit(1);
	}

	shm_ptr[0] = 0; // Seconds
	shm_ptr[1] = 0; // Nanoseconds

	// Setup message queue
	msqid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	if (msqid == -1) {
		fprintf(stderr, "Error creating message queue.\n");
		exit(1);
	}

	// Open log file if specified
	FILE *logfp = NULL;
	if (logfile != NULL) {
		logfp = fopen(logfile, "w");
		if (logfp == NULL) {
			fprintf(stderr, "Failed to open log file: %s\n", logfile);
			exit(1);
		}
	}

	//  Main loop
	// - Increment clock by (QUARTER_SEC_NS / number_of_running_children)
	// - Alternate sending messages to each child via message queue
	// - Receive responses, check if child is terminating
	// - Launch new children obeying simul and interval limits
	// - Random time for each child between 1s and timeLimit
	// - Print process table every 0.5 simulated seconds and after each launch
	// - Log oss output to both stdout and logfp

	// Cleanup
	if (logfp != NULL)
		fclose(logfp);

	shmdt(shm_ptr);
	shmctl(shmid, IPC_RMID, NULL);
	msgctl(msqid, IPC_RMID, NULL);

	return EXIT_SUCCESS;
}
