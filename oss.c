/**
* oss.c
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: 
*/
#include "oss.h"
#include "osutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>

// Global vars, need to be global for signal handling purposes
struct stime_t *g_stm;
int g_stmId;

struct pcb_t *g_pcb;
int g_pcbId;

int g_mschId;
int g_mossId;

FILE *g_output;

/**
* Flags all shared mem segments for removal
* *Must* be called before any exit of process
*/
void cleanUp() {
	fclose(g_output);

	int success = 1;

	// Detach from segment
	if (shmdt(g_stm) == -1) {
		perror("Detatch shared memory failed in OSS");
		success = 0;
	}

	// Signal shared segment for removal
	if (shmctl(g_stmId, IPC_RMID, NULL) == -1) {
		perror("Failed to mark shared number for removal");
		success = 0;
	}
	
	// Detach from segment
	if (shmdt(g_pcb) == -1) {
		perror("Detatch shared memory failed in OSS");
		success = 0;
	}

	// Signal shared segment for removal
	if (shmctl(g_pcbId, IPC_RMID, NULL) == -1) {
		perror("Failed to mark shared number for removal");
		success = 0;
	}

	// Signal scheduler message queue for removal
	if (msgctl(g_mschId, IPC_RMID, NULL) == -1) {
		perror("Failed to remove OSS message queue");
		success = 0;
	}

	// Signal oss message queue for removal
	if (msgctl(g_mossId, IPC_RMID, NULL) == -1) {
		perror("Failed to remove OSS message queue");
		success = 0;
	}

	if (!success) {
		fprintf(stderr, "OSS failed to clean up\n");
		exit(EXIT_FAILURE);
	}
}

/**
* Uses setupSegment function to allocate the needed memory segments
*/
void setupMemory(int numProc) {
	// Allocate segment of shared memory
	if ((g_stmId = shmget(STM_KEY, sizeof(struct stime_t), IPC_CREAT | 0660)) < 0) {
		perror("Allocating shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((g_stm = shmat(g_stmId, NULL, 0)) == (struct stime_t*) -1) {
		perror("Attach shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	// Allocate segment of shared memory for pcb array
	if ((g_pcbId = shmget(PCB_KEY, sizeof(struct pcb_t) * numProc, IPC_CREAT | 0660)) < 0) {
		perror("Allocating shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach pcb to segment for access
	if ((g_pcb = shmat(g_pcbId, NULL, 0)) == (struct pcb_t*) -1) {
		perror("Attach shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Initialize shared seconds to 0
	(*g_stm).sec = 0;

	// Initialize shared nanoseconds to 0
	(*g_stm).nnsec = 0;

	// Create message queue used for child processes critical section lock
	if ((g_mschId = msgget(SCH_KEY, IPC_CREAT | 0666)) < 0) {
		perror("Failed to create OSS message queue in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Create message queue for receiving messages in OSS
	if ((g_mossId = msgget(OSS_KEY, IPC_CREAT | 0666)) < 0) {
		perror("Failed to create OSS message queue in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}
}

void abortAll(pid_t *childPids, int spawns) {
	int i;

	for (i = 0; i < spawns; i++) {
		kill(childPids[i], SIGABRT);
	}

	while (wait(NULL) > 0);
}

/**
* Handles SIGINT ^C, waits for children then cleans up exits
*/
void sigHandler(int sig) {
	if (sig == SIGINT) {
		// Wait for child processess to handle interrupt
		while (wait(NULL) > 0);
		// Clean up actions on shared memory
		cleanUp();
		fprintf(stderr, "OSS was interupted, exiting...\n");
		exit(EXIT_FAILURE);	
	}
}

void generateChild(pid_t *childPids, int spawns, int index) {
	if (childPids[index] != -1) {
		kill(childPids[index], SIGABRT);
		waitpid(childPids[index], NULL, 0);
	}

	struct pcb_t pcb;
	struct stime_t endTemp = {-1, -1};
	pid_t thisPid;

	pcb.id = index + 1;
	pcb.priority = 1;
	pcb.time_created = *g_stm;
	pcb.time_ended = endTemp;
	pcb.time_waiting = 0;
	pcb.time_cpu = 0;

	pcb.burst_needed = rand() % 5000 + 1000;
	pcb.last_burst = 0;

	g_pcb[index] = pcb;

	thisPid = fork();

	// Child 
	if (thisPid == 0) {
		char indexArg[4];
		char spawnArg[4];
		snprintf(indexArg, 4, "%d", index);
		snprintf(spawnArg, 4, "%d", spawns);

		// Exec Slave from child process
		execl("./Slave", indexArg, spawnArg, NULL);

		// Should never reach here
		perror("Failed to execute Slave");
		exit(EXIT_FAILURE);
	}
	// Parent
	else {
		childPids[0] = thisPid;
	}
}

void incrementTime(int delta) {
	g_stm->nnsec += delta;

	// Once full second has been reached, increment sim time second and reset ns
	if (g_stm->nnsec == NS_PER_S) {
		g_stm->sec++;
		g_stm->nnsec = 0;
	}	
}

/**
* Main function
*/
int main(int argc, char **argv) {
	// getopt-use variables
	char *optErrMsg = "try \'%s -h\' for more information\n";
	char *helpMsg = "%s options:\n"
			"-h: displays this help message\n"
			"-s [integer]: number of child processes spawned (max 18)\n"
			"-t [integer]: number of seconds OSS will wait\n"
			"-l [filename]: name of file where log will be written\n"
			"-c [integer]: amount sim time will be incremented (try 10 or 100 etc.)\n";
	int c = 0;
	
	// preference variables
	int spawns = DFLT_SPAWNS;
	int waitReal = DFLT_RWAIT;
	int waitSim = DFLT_SWAIT;
	char *filename = DFLT_FILEN;

	// process variables
	pid_t *childPids;
	long long realEndTime;
	
	int i = 0;
	int j = 0;
	int lineCount = 1;

	struct sigaction sa;
	struct oss_msgbuf ossBuf;
	struct sch_msgbuf schBuf;

	// Setup signal handler for SIGINT

	sa.sa_handler = sigHandler;
  	sigemptyset(&sa.sa_mask);
  	sa.sa_flags = 0;

	if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
		return 1;
    }

	// Handle getopt
	while ( (c = getopt( argc, argv, "hs:l:t:" )) != -1 ) {
		switch ( c ) {
			case 'h':
				printf(helpMsg, argv[0]);
				return 0;
			// Number of children spawned
			case 's':
				spawns = strtol(optarg, NULL, 10);
				if (spawns > 18) {
					fprintf(stderr, "Max number of spawned Slaves is 18\n");
					return 1;
				}
				break;
			// File to save log entries to
			case 'l':
				filename = strdup(optarg);
				break;
			// Time OSS waits for slaves to finish
			case 't':
				waitReal = strtol(optarg, NULL, 10);
				break;
			case '?':
			default:
				fprintf(stderr, optErrMsg, argv[0]);
				return 1;
		}
	}

	// Allocate and init shared memory
	setupMemory(spawns);

	// Allocate memory for array to store all children pids	
	if ((childPids = calloc(1, sizeof(pid_t) * spawns)) == NULL) {
		perror("Failed to allocate memory for pid array");
		cleanUp();
		return 1;
	}

	memset(childPids, -1, spawns);

	// Attempt to open file in append mode
	if ((g_output = fopen(filename, "a")) == NULL) {
		perror("Failed to open file in OSS");
		cleanUp();
		return 1;
	}

	// Start of log entry message in file
	fputs("***LOG ENTRY BEGIN***\n", g_output);

	// The real time in nanoseconds that OSS should terminate if hasn't already finished
	realEndTime = nsSinceEpoch() + (waitReal * ((long long) NS_PER_S));

	srand(time(NULL));

	generateChild(childPids, spawns, 0);
	
	schBuf.mtype = g_pcb[0].id;
	schBuf.quantum = 700;


	if (msgsnd(g_mschId, &schBuf, sizeof(struct sch_msgbuf), 0) == -1) {
			perror("OSS failed to send sch messege");
			cleanUp();
			exit(EXIT_FAILURE);
	}

	if (msgrcv(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), 1, 0) == -1){
			perror("OSS failed to receive return message");
			cleanUp();
			exit(EXIT_FAILURE);
	}


	printf("OSS got message from child: %d\n", ossBuf.interrupt);

/*
	// Main loop
	while (1) {

		// Once full second has been reached, increment sim time second and reset ns
		if (*g_shn == NS_PER_S) {
			*g_shs += 1;
			*g_shn = 0;
		}


		// Check for real system time reaching end point
		if (nsSinceEpoch() >= realEndTime) {
			printf("Real time ended\n");
			break;
		}
	}
*/	

	abortAll(childPids, spawns);

	cleanUp();

	printf("OSS exiting...\n");

	return 0;
}
