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
struct stime_t *g_stm;
struct pcb_t *g_pcb;
int g_mschId;
int g_mossId;

/**
* Detaches from shared mem
* *Must* be called before any exit of process
*/
void cleanUp() {
	int success = 1;

	// Detach from segment
	if (shmdt(g_stm) == -1) {
		perror("Detatch shared memory failed in Slave");
		success = 0;
	}

	// Detach from pcb array
	if (shmdt(g_pcb) == -1) {
		perror("Detatch shared memory failed in Slave");
		success = 0;
	}


	// Not all segments detatched successfully
	if (!success) {
		exit(EXIT_FAILURE);
	}
}

/**
* Attaches to the memory segments for seconds and nanoseconds
* created in OSS.
*/
void attachMemory(int numProc) {
	int pcbId;
	int id;

	// Allocate segment of shared memory
	if ((id = shmget(STM_KEY, sizeof(struct stime_t), 0660)) < 0) {
		perror("Allocating shared memory failed in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((g_stm = shmat(id, NULL, 0)) == (struct stime_t*) -1) {
		perror("Attach shared memory failed in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	// Find segment of shared memory for pcb array
	if ((pcbId = shmget(PCB_KEY, sizeof(struct pcb_t) * numProc, 0660)) < 0) {
		perror("Allocating shared memory failed in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach pcb pointer to segment for access
	if ((g_pcb = shmat(pcbId, NULL, 0)) == (struct pcb_t*) -1) {
		perror("Attach shared memory failed in Slave");
		cleanUp();
		exit(EXIT_FAILURE);
	}

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
	
	// OSS sent abort signal
	if (sig == SIGABRT) {
		fprintf(stderr, "Slave %d was aborted, exiting...\n", getpid());
		cleanUp();
		exit(EXIT_SUCCESS);
	}

	// Received interrupt signal
	if (sig == SIGINT) {
		fprintf(stderr, "Slave %d was interupted, exiting...\n", getpid());
	}
	else {
		fprintf(stderr, "Slave %d unknown signal %d, exiting...\n", getpid(), sig);
	}

	cleanUp();
	exit(EXIT_FAILURE);	
}

/**
* Main function
*/
int main(int argc, char **argv) {
	int pcbIndex;
	int spawns;
	int quantum;
	int interruptProb;
	int terminateProb;
	int msgId = getpid();
	struct sigaction sa;
	struct sch_msgbuf schBuf;
	struct oss_msgbuf ossBuf;
	struct pcb_t *pcb;

	pcbIndex = strtol(argv[0], NULL, 10);
	spawns = strtol(argv[1], NULL, 10);
	interruptProb = strtol(argv[2], NULL, 10);
	terminateProb = strtol(argv[3], NULL, 10);

	// Setup signal handlers

	sa.sa_handler = sigHandler;
  	sigemptyset(&sa.sa_mask);
  	sa.sa_flags = 0;
	
	if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: Slave cannot handle SIGINT");
		exit(EXIT_FAILURE);
	}
	
	if (sigaction(SIGABRT, &sa, NULL) == -1) {
        perror("Error: Slave cannot handle SIGABRT");
		exit(EXIT_FAILURE);
	}

	// Attach to shared memory
	attachMemory(spawns);

	// Reference to this prcoesses pcb in shared memory
	pcb = &g_pcb[pcbIndex];

	// Seed random, changing seed based on pid
	srand(time(NULL) ^ (getpid() << 16));

	while (1) {
		// Wait to be scheduled by OSS -- message type is our pid
		if (msgrcv(g_mschId, &schBuf, sizeof(struct sch_msgbuf), msgId, 0) == -1){
			perror("Slave failed to receive scheduler message");
			cleanUp();
			exit(EXIT_FAILURE);
		}

		ossBuf.mtype = 1;
		ossBuf.finished = 0;
		ossBuf.interrupt = 0;
		ossBuf.index = pcbIndex;

		quantum = schBuf.quantum;

		// Check for termination
		if ((rand() % terminateProb + 1) == terminateProb) {
			quantum = rand() % (quantum + 1);
			pcb->last_burst = quantum;
			ossBuf.finished = 1;
			break;
		}

		// Check for interrupt
		if ((rand() % interruptProb + 1) == interruptProb) {
			quantum = rand() % (quantum + 1);
			ossBuf.interrupt = 1;
		}

		pcb->last_burst = quantum;

		// If not termination, pass lock back to message queue
		if (msgsnd(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), 0) == -1) {
			perror("Slave failed to send oss messege");
			cleanUp();
			exit(EXIT_FAILURE);
		}
	}
	
	// Send final message back to OSS -- no blocked waiting, just send and gtfo
	if (msgsnd(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), IPC_NOWAIT) == -1) {
		perror("Slave failed to send oss messege");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	cleanUp();

	exit(EXIT_SUCCESS);
}
