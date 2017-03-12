/**
* slave.c
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary:
*/
#include "oss.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

// Global vars, need to be global for signal handling purposes
int *g_shs;
int *g_shn;
int *g_pcb;
int g_mschId;
int g_mossId;

int detachSegment(int *shp) {
	// Detach from segment
	if (shmdt(shp) == -1) {
		perror("Detatch shared memory failed in Slave");
		return 0;
	}

	return 1;
}

/**
* Detaches from shared mem
* *Must* be called before any exit of process
*/
void cleanUp() {
	int success;

	success = detachSegment(g_shs);

	success = detachSegment(g_shn);

	success = detachSegment(g_pcb);


	// Not all segments detatched successfully
	if (!success) {
		exit(EXIT_FAILURE);
	}
}

void createSegment(int **shp, key_t key, size_t size) {
	int id;

	// Allocate segment of shared memory
	if ((id = shmget(key, size, 0660)) < 0) {
		perror("Allocating shared memory failed in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((*shp = shmat(id, NULL, 0)) == (int*) -1) {
		perror("Attach shared memory failed in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}	
}

/**
* Attaches to the memory segments for seconds and nanoseconds
* created in OSS.
*/
void attachMemory(int numProc) {
	createSegment(&g_shs, SHS_KEY, sizeof(int));

	createSegment(&g_shn, SHN_KEY, sizeof(int));
	
	createSegment(&g_pcb, PCB_KEY, sizeof(struct pcb_type) * numProc);

	// Find message queue for sending messages to OSS
	if ((g_mschId = msgget(SCH_KEY, 0666)) < 0) {
		perror("Failed to get OSS message queue in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Find message queue used for critical section lock
	if ((g_mossId = msgget(OSS_KEY, 0666)) < 0) {
		perror("Failed to get lock message queue in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}	
}

/**
* Handles SIGABORT sent from OSS and SIGINT
* Prints appropriate message, cleans up, and exits
*/
void sigHandler(int sig) {
	sigset_t mask, omask;

	// Block other signals
	if ((sigfillset(&mask) == -1) || (sigprocmask(SIG_SETMASK, &mask, &omask) == -1)) {
		perror("Failed to block signals");
	}
	
	// Master sent abort signal
	if (sig == SIGABRT) {
		fprintf(stderr, "Child %d was aborted, exiting...\n", getpid());
		cleanUp();
		exit(EXIT_SUCCESS);
	}

	// Received interrupt signal
	if (sig == SIGINT) {
		fprintf(stderr, "Child %d was interupted, exiting...\n", getpid());
	}
	else {
		fprintf(stderr, "Child %d unknown signal %d, exiting...\n", getpid(), sig);
	}
	cleanUp();
	exit(EXIT_FAILURE);	
}

int main(int argc, char **argv) {
	int pcbIndex;
	int spawns;
	int quantum;
	struct sigaction sa;
	struct sch_msgbuf schBuf;
	struct oss_msgbuf ossBuf;

	pcbIndex = strtol(argv[0], NULL, 10);
	spawns = strtol(argv[1], NULL, 10);

	// Setup signal handlers

	sa.sa_handler = sigHandler;
  	sigemptyset(&sa.sa_mask);
  	sa.sa_flags = 0;
	
	if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
		exit(EXIT_FAILURE);
	}
	
	if (sigaction(SIGABRT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
		exit(EXIT_FAILURE);
	}

	// Attach to shared memory
	attachMemory(spawns);

	// Seed random, changing seed based on pid
	srand(time(NULL) ^ (getpid() << 16));

	

	cleanUp();
	exit(EXIT_SUCCESS);
}
