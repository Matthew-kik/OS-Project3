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

struct PCB processTable[MAX_PROCESSES]; //Struct from shared.h fileld with MAX_PROCESS = 20.
struct mymsg msg; // Struct from shared.h
int shmid;
int msqid;
int* shm_ptr;

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

void usage(const char* app) {
	fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i interval] [-f logfile]\n", app);
	fprintf(stderr, "	-n proc, sets total number of children to launch.\n");
	fprintf(stderr, "	-s simul, max number of children running simultaneously.\n");
	fprintf(stderr, "	-t timeLimit, upper bound of time for children (e.g. 7 or 7.3).\n");
	fprintf(stderr, "	-i interval, interval in seconds between child launches (e.g. 0.5).\n");
	fprintf(stderr, "	-f logfile, path to log file for oss output.\n");
}

int main(int argc, char** argv) {

	signal(SIGALRM, kill_processes);
	signal(SIGINT, kill_processes);
	alarm(60);

	int proc = 0;
	int simul = 0;
	float timeLimit = 0;
	float interval = 0;
	srand(time(NULL));
	char* logfile = NULL;

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

	shm_ptr = (int*)shmat(shmid, NULL, 0);
	if (shm_ptr == (void*)-1) {
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
	FILE* logfp = NULL;
	if (logfile != NULL) {
		logfp = fopen(logfile, "w");
		if (logfp == NULL) {
			fprintf(stderr, "Failed to open log file: %s\n", logfile);
			exit(1);
		}
	}

	// Counting for variables
	int totalMsg = 0;
	int total = 0;
	int lastPrintSec = 0;
	int lastPrintNano = 0;
	int lastLaunchSec = 0;
	int lastLaunchNano = 0;
	int intervalSec = (int)interval; // Recasting float argument to int
	int intervalNano = (interval - intervalSec) * NANO_PER_SEC; // Recasting float argument to int
	int c = 0;

	// Main loop - logic work is in process
	while (total < proc || c > 0) {

		// Ensures clock only increments when children are launched. Or else C will divide by zero
		if (c > 0) {
			shm_ptr[1] += QUARTER_SEC_NS / c;
			if (shm_ptr[1] >= 1000000000) {
				shm_ptr[0]++;
				shm_ptr[1] -= 1000000000;
			}
		}


		int launchTargetSec = lastLaunchSec + intervalSec;
		int launchTargetNano = lastLaunchNano + intervalNano;
		if (launchTargetNano >= NANO_PER_SEC) {
			launchTargetSec++;
			launchTargetNano -= NANO_PER_SEC;
		}

		if (c < simul && total < proc &&
			(shm_ptr[0] > launchTargetSec || (shm_ptr[0] == launchTargetSec && shm_ptr[1] >= launchTargetNano))) {

			int rngSec = rand() % (int)timeLimit + 1;
			int rngNano = rand() % NANO_PER_SEC;
			pid_t worker = fork();

			if (worker == 0) {

				char secArg[20];
				char nanoArg[20];
				sprintf(secArg, "%d", rngSec);
				sprintf(nanoArg, "%d", rngNano);
				execl("./worker", "worker", secArg, nanoArg, NULL);

			}

			// Fills process table
			if (worker > 0) {

				int endSec = shm_ptr[0] + (rngSec * c);
				int endNano = shm_ptr[1] + (rngNano * c);
				if (endNano >= NANO_PER_SEC) {
					endSec += endNano / NANO_PER_SEC;
					endNano = endNano % NANO_PER_SEC;
				}

				for (int i = 0; i < 20; i++) {
					if (!processTable[i].occupied) {
						processTable[i].occupied = 1;
						processTable[i].pid = worker;
						processTable[i].startSeconds = shm_ptr[0];
						processTable[i].startNano = shm_ptr[1];
						processTable[i].endingTimeSeconds = endSec;
						processTable[i].endingTimeNano = endNano;
						processTable[i].messagesSent = 0;
						break;
					}
				}
				c++;
				total++;
				lastLaunchSec = shm_ptr[0];
				lastLaunchNano = shm_ptr[1];
			}
		}

		// Print process table every 0.5 simulated seconds and after each launch
		if (shm_ptr[0] > lastPrintSec || (shm_ptr[0] == lastPrintSec && shm_ptr[1] >= lastPrintNano + 500000000)) {

			printf("OSS PID: %d SysClockS: %d SysclockNano: %d\n", getpid(), shm_ptr[0], shm_ptr[1]);
			printf("Process Table:\n");
			printf("%-10s %-10s %-10s %-10s %-10s %-15s %-15s %-10s\n",
				"Entry", "Occupied", "PID", "StartS", "StartN", "EndingTimeS", "EndingTimeNano", "MessagesSent");
			for (int i = 0; i < 20; i++) {
				printf("%-10d %-10d %-10d %-10d %-10d %-15d %-15d %-10d\n", i, processTable[i].occupied,
					processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano,
					processTable[i].endingTimeSeconds, processTable[i].endingTimeNano, processTable[i].messagesSent);
			}

			lastPrintSec = shm_ptr[0];
			lastPrintNano = shm_ptr[1];
		}

		// Messaging
		if (c > 0) {


		}

	}

	// Log oss output to both stdout and logfp, handle this last. 

	// Cleanup
	if (logfp != NULL)
		fclose(logfp);

	shmdt(shm_ptr);
	shmctl(shmid, IPC_RMID, NULL);
	msgctl(msqid, IPC_RMID, NULL);

	return EXIT_SUCCESS;
}
