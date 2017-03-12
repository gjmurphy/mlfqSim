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
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>

// Default values of preferences
#define DFLT_SPAWNS 5
#define DFLT_RWAIT 20
#define DFLT_SWAIT 2
#define DFLT_FILEN "test.out"

// Global vars, need to be global for signal handling purposes
int *g_shs;
int *g_shn;
int g_shsId;
int g_shnId;

int *g_pcb;
int g_pcbId;

int g_mschId;
int g_mossId;

FILE *g_output;

int destroySegment(int shId, int *shp) {
	int success = 1;

	// Detach from segment
	if (shmdt(shp) == -1) {
		perror("Detatch shared memory failed in master");
		success = 0;
	}

	// Signal shared segment for removal
	if (shmctl(shId, IPC_RMID, NULL) == -1) {
		perror("Failed to mark shared number for removal");
		success = 0;
	}

	return success;
}

/**
* Flags all shared mem segments for removal
* *Must* be called before any exit of process
*/
void cleanUp() {
	fclose(g_output);

	int success;

	success = destroySegment(g_shsId, g_shs);

	success = destroySegment(g_shnId, g_shn);
	
	success = destroySegment(g_pcbId, g_pcb);

	// Signal scheduler message queue for removal
	if (msgctl(g_mschId, IPC_RMID, NULL) == -1) {
		perror("Failed to remove OSS message queue");
		success = 1;
	}

	// Signal oss message queue for removal
	if (msgctl(g_mossId, IPC_RMID, NULL) == -1) {
		perror("Failed to remove OSS message queue");
		success = 1;
	}

	if (!success) {
		fprintf(stderr, "OSS failed to clean up\n");
		exit(EXIT_FAILURE);
	}
}

void createSegment(int *shId, int **shp, key_t key, size_t size) {
	// Allocate segment of shared memory
	if ((*shId = shmget(key, size, IPC_CREAT | 0660)) < 0) {
		perror("Allocating shared memory failed in OSS");
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((*shp = shmat(*shId, NULL, 0)) == (int*) -1) {
		perror("Attach shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}	
}

/**
* Uses setupSegment function to allocate the needed memory segments
*/
void setupMemory(int numProc) {
	createSegment(&g_shsId, &g_shs, SHS_KEY, sizeof(int));

	createSegment(&g_shnId, &g_shn, SHN_KEY, sizeof(int));
	
	createSegment(&g_pcbId, &g_pcb, PCB_KEY, sizeof(struct pcb_type) * numProc); 

	// Initialize shared seconds to 0
	*g_shs = 0;

	// Initialize shared nanoseconds to 0
	*g_shn = 0;

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
	pid_t thisPid;
	char *path = "./Slave";
	long long realEndTime;
	int i = 0;
	int j = 0;
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

	// Attempt to open file in append mode
	if ((g_output = fopen(filename, "a")) == NULL) {
		perror("Failed to open file in Master");
		cleanUp();
		return 1;
	}

	// Start of log entry message in file
	fputs("***LOG ENTRY BEGIN***\n", g_output);

	// Forks the initial child processes
	for (i = 0; i < spawns; i++) {
		thisPid = fork();

		// Child 
		if (thisPid == 0) {
			// Index in childPids array sent to child so can be msgd back when kills self
			char index[4];
			snprintf(index, 4, "%d", i);

			// Exec Slave from child process
			execl(path, index, NULL);

			// Should never reach here
			perror("Failed to execute Slave");
			exit(EXIT_FAILURE);
		}
		// Error
		else if (thisPid == -1) {
			perror("Failed to fork!");
			// Kill any previously forked children
			for (j = 0; j < i; j++) {
				kill(childPids[j], SIGABRT);
			}
			while (wait(NULL) > 0);
			cleanUp();
			return 1;
		} 
		// Parent
		else {
			childPids[i] = thisPid;
		}	
	}

	// The real time in nanoseconds that OSS should terminate if hasn't already finished
	realEndTime = nsSinceEpoch() + (waitReal * ((long long) NS_PER_S));

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
	
	// If all slave process didn't already exit
	if (waitpid(-1, NULL, WNOHANG) != -1) {
		for (i = 0; i < spawns; i++) {
			kill(childPids[i], SIGABRT);
		}
		while (wait(NULL) > 0);
		printf("OSS exiting...\n");
	}

	cleanUp();
	return 0;
}
