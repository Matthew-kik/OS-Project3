//Created by: Matthew Kik
//Last Modified: 22 Feb 2026
//Usage: See below or read attached README utilize Makefile for compiling 


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>

// Macro defined key and buffer size
#define SHMKEY 859285
#define BUFF_SZ 2 * sizeof (int) // Two *  ints for Nanosec and Seconds

//Process control table
struct PCB {

	int occupied;
	pid_t pid;
	int startSeconds;
	int startNano;
	int endingTimeSeconds;
	int endingTimeNano;

};

struct PCB processTable[20];
int shmid;
int *shm_ptr;

//Loops throuch process table and clears shared memory if 60 seconds have passed
void kill_processes(int sig){

	for(int i = 0; i < 20; i++){
		if (processTable[i].occupied){
			kill(processTable[i].pid, SIGTERM);
			}
	}	
// Cleans shared memory
shmdt(shm_ptr);
shmctl(shmid, IPC_RMID, NULL);
exit(1);

}

void usage (const char * app) {
	fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i interval]\n",app);
	fprintf(stderr, " 	-n proc, sets total number of children to launch.\n");
	fprintf(stderr, "	-s simul, max number of children running simultaneously.\n");
	fprintf(stderr, "	-t timeLimit, time limit for childen processes set in seconds e.g 4.5.\n");
	fprintf(stderr, "	-i interval of seconds for children processes to launch in seconds e.g. 0.5 seconds.\n");
}

int main(int argc, char** argv) {

// Signal to kill all children processes, then frees up memory after 60 seconds. 
signal(SIGALRM, kill_processes); // If 60 seconds pass
signal(SIGINT, kill_processes); // Handles freeing memory if ctrl-c is utilized, 
				// I didn't add SIGSTP with ctrl-z, as that seemed overly complex.
alarm(60);

// Setup counters for counting processes and totals
int c = 0; // Counts children
int total = 0; // total launched children
int proc; // Processes
int simul;// Simuntaneous processes.
float timeLimit; // Time limit passed through argument.
float interval; // Interval time passed through arguement.
float lastPrintTime = 0; // Counter for last print time.
float currentTime = 0; // Counter for tracking currentTime within clock.
float lastLaunchTime = 0; // Counter for tracking time of last launched process.

//Setup Shared memory, error checking if no id is found

shmid = shmget ( SHMKEY, BUFF_SZ, 0666 | IPC_CREAT );

if ( shmid == -1 ) {
         fprintf(stderr, "Error in generating Shard memory for parent.\n");
          exit (1);
}

// Intitializes pointer and attaches ID to whatever memory location the OS decides. With error checking
shm_ptr = (int *)shmat(shmid, NULL, 0);
if (shm_ptr == (void *) -1) {
	fprintf(stderr, "Failed to attach memory\n");
		exit(1);
}

shm_ptr[0] = 0; // Counter for seconds
shm_ptr[1] = 0; // Counter for nanoseconds 

// Switch case for taking in arguments
char opt;
while ((opt = getopt(argc, argv, "hn:s:t:i:")) != -1)
	{
	switch (opt) {
		case 'h':
			usage(argv[0]);
			return (EXIT_SUCCESS);
		
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
		
		default:
			printf("Incorrect input, please follow the usage below.\n");
			usage(argv[0]);
			return (EXIT_FAILURE);
	}
        
	}

	// Declarations for status, pid
	int status;
	pid_t pid;
	
	// Loop for logic
	while ( total  < proc || c > 0 ) {
	
	shm_ptr[1] += 1000; //slowed down for better counting		 
	if (shm_ptr[1] >= 1000000000){ 
		shm_ptr[0]++; 
		shm_ptr[1] -= 1000000000;
	}

	currentTime = shm_ptr[0] + (shm_ptr[1] / 1000000000.0);

	// Non-blocking wait call checking child termination
	pid = waitpid(-1, &status, WNOHANG);
	if (pid > 0){
		
		// Loops and counts process tables to check for terminated pids
		for (int i = 0; i < 20; i++) {
			if (processTable[i].pid == pid) {
			    processTable[i].occupied = 0;
			    c--;
			    break;
			}
		}

	}
	
	// Checks for every 0.5 seconds and prints PCB table and processes
	if (currentTime - lastPrintTime >= 0.5) {
	   
		printf("OSS PID: %d SysClockS: %d SysclockNano: %d\n", getpid(), shm_ptr[0], shm_ptr[1]);		
		printf("Process Table:\n");
		printf("%-10s %-10s %-10s %-10s %-10s %-15s %-15s\n",
			"Entry", "Occupied", "PID", "StartS", "StartN", "EndingTimeS", "EndingTimeNano");
		for (int i = 0; i < 20; i++) {
			printf("%-10d %-10d %-10d %-10d %-10d %-15d %-15d\n", i, processTable[i].occupied,
				processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano,
				processTable[i].endingTimeSeconds, processTable[i].endingTimeNano);
		}
		lastPrintTime = currentTime;
	}
	
	// Launch new child is conditions are met. 
	if ( c < simul && total < proc && (currentTime - lastLaunchTime) >= interval) {
	
	// Pass arguements back from int to string
	char secArg[20];
	char nanoArg[20];
	
	// recasts timelimit to drop decimal 
	int workerSec = (int)timeLimit;
	int workerNano = (timeLimit - workerSec) * 1000000000;
	sprintf(secArg, "%d", workerSec);
	sprintf(nanoArg, "%d", workerNano);
	
	pid_t worker = fork(); // Forks worker, starts execl call with error checking
	if ( worker == 0) {
	
		execl("./worker", "worker", secArg, nanoArg, NULL);
		fprintf(stderr, "Excel Failed\n");
		exit(1);
	}
	
	if (worker > 0) {

		// Fills PCB counts to 20 for maximum and determines if nothing is occupied
		for (int i = 0; i < 20; i++) {
			if (!processTable[i].occupied) {
				processTable[i].occupied = 1;
				processTable[i].pid = worker;
				processTable[i].startSeconds = shm_ptr[0];
				processTable[i].startNano = shm_ptr[1];
				break;
			}	
		}
		lastLaunchTime = currentTime;
                c++;
                total++;
		}
	}

	}
// Final print after terminating
printf("OSS PID:%d Terminating\n", getpid());
printf("%d workers were launched and terminated\n", total);

 return(EXIT_SUCCESS);
}




