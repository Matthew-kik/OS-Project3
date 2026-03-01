//Created By: Matthew Kik
//Last Modified 22 Feb 2026
//Usage: Child processes for attached oss.c file, see attahced README.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

const int BUFF_SZ = sizeof(int)*2; // Two ints to account for seconds and nanoseconds.

int main(int argc, char** argv) {
	
	// Gets Shared memory ID from oss.c, buffer set as const int as above 
int shm_id = shmget(859285, BUFF_SZ, 0666);
if (shm_id <= 0 ) {
	fprintf(stderr, "Error in Child Shared Memory.\n");
	exit(1);
	}

// Shared memory pointer
int*shm_ptr = (int *)shmat(shm_id, NULL, 0);
if (shm_ptr == (void *)-1){
	fprintf(stderr, "Shared memory failed\n");
	exit(1);
	}		

int sec = shm_ptr[0]; //Seconds
int nano = shm_ptr[1]; //Nanosecond

int limitSec = atoi(argv[1]); //Limits set for seconds given by oss arguement
int limitNano = atoi(argv[2]); // Limits set for nanoseconds via arguement

int termSec = sec + limitSec; // Termination = Seconds + limitSec
int termNano = nano + limitNano; // Termination = nano + limitNano

// Converts Nano seconds to seconds
if (termNano >= 1000000000) {
	termSec++;
	termNano -= 1000000000;
	}		

pid_t my_pid = getpid(); // Process ID
pid_t parent_pid = getppid(); // Get Parent ID (from oss)

// Inital counts and printf to display pid, ppid, and clocks. 
printf("Worker Starting, PID:%d PPID:%d\n", getpid(), getppid());
printf("Called with:\n");
printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", sec, nano, termSec, termNano);
printf("--Just Starting\n");


int timePassed = 0; // counts how many seconds have passed
int lastSec = sec; // Last second to be counted, passed from sec

// Clock hasn't reached termination time, run an if loop to loop print and counts.
while (shm_ptr[0] <  termSec || (shm_ptr[0] == termSec && shm_ptr[1] < termNano)){

	
if (shm_ptr[0] > lastSec) {
	timePassed++;
	lastSec = shm_ptr[0];
	printf("WORKER PID:%d PPID:%d\n", my_pid, parent_pid);
	printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", shm_ptr[0], shm_ptr[1], termSec, termNano);
	printf("--%d seconds have passed since starting.\n",timePassed);
	}
}

// After loop ends, print final counts and shows the loop is finished
printf("WORKER PID:%d PPID:%d\n", my_pid, parent_pid);
printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", shm_ptr[0], shm_ptr[1], termSec, termNano);
printf("--Terminating\n");

shmdt(shm_ptr); // Cleans shared memory

return EXIT_SUCCESS;
}
