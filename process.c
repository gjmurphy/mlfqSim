/**
* process.c
* Author: Gabriel Murphy
* Date: Mon Mar 20 2017
* Summary: Instance of Process is execed when OSS forks a new child.
* Process spin-locks on a shared message queue until that Process 
* is "dispatched" by OSS. When that happens, Process randomly
* decides (based on probabilities sent as command line args) whether
* it should be interrupted by I/O, terminate, or just use its full
* quantum. Process sends a message back to OSS at the end of each
* dispatch.
*/
#include "oss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

// Shared memory vars, need to be global for signal handling purposes
struct stime_t *g_stm;
struct pcb_t *g_pcb;
int g_mschId;
int g_mossId;

/**
* Detaches from shared mem
* *Must* be called before any exit of process
*/
void cleanUp() {
	bool success = true;
	char errorMsg[128];

	snprintf(errorMsg, 128, "Detach shared memory failed in Process %d", getpid());

	// Detach from segment
	if (shmdt(g_stm) == -1) {
		fprintf(stderr, "%s: %s\n", errorMsg, strerror(errno));
		success = false;
	}

	// Detach from pcb array
	if (shmdt(g_pcb) == -1) {
		fprintf(stderr, "%s: %s\n", errorMsg, strerror(errno));
		success = false;
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
void attachMemory(int numProcess) {
	int pcbId;
	int id;
	char errorMsg[128];

	snprintf(errorMsg, 128, "Attach shared memory failed in Process %d", getpid());

	// Allocate segment of shared memory
	if ((id = shmget(STM_KEY, sizeof(struct stime_t), 0660)) < 0) {
		fprintf(stderr, "%s 0: %s\n", errorMsg, strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((g_stm = shmat(id, NULL, 0)) == (struct stime_t*) -1) {
		fprintf(stderr, "%s 1: %s\n", errorMsg, strerror(errno));
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	// Find segment of shared memory for pcb array
	if ((pcbId = shmget(PCB_KEY, sizeof(pcb_t) * numProcess, 0660)) < 0) {
		fprintf(stderr, "%s 2: %s\n", errorMsg, strerror(errno));
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach pcb pointer to segment for access
	if ((g_pcb = shmat(pcbId, NULL, 0)) == (struct pcb_t*) -1) {
		fprintf(stderr, "%s 3: %s\n", errorMsg, strerror(errno));
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Find message queue used for critical section lock
	if ((g_mschId = msgget(SCH_KEY, 0666)) < 0) {
		fprintf(stderr, "%s 4: %s\n", errorMsg, strerror(errno));
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Find message queue for sending messages to OSS
	if ((g_mossId = msgget(OSS_KEY, 0666)) < 0) {
		fprintf(stderr, "%s 5: %s\n", errorMsg, strerror(errno));
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
		fprintf(stderr, "Failed to block signals in Process %d: %s\n", getpid(), strerror(errno));
	}
	
	// OSS sent abort signal
	if (sig == SIGABRT) {
		fprintf(stderr, "Process %d was aborted, exiting...\n", getpid());
	}
	// Received interrupt signal
	else if (sig == SIGINT) {
		fprintf(stderr, "Process %d was interupted, exiting...\n", getpid());
	}
	// Any other signal
	else {
		fprintf(stderr, "Process %d unknown signal %d, exiting...\n", getpid(), sig);
	}

	cleanUp();
	exit(EXIT_FAILURE);
}

/**
* Main function
*/
int main(int argc, char **argv) {
	int pcbIndex;
	int numProcess;
	int quantum;
	int interruptProb;
	int terminateProb;
	int msgId = getpid();
	struct sigaction sa;
	struct sch_msgbuf schBuf;
	struct oss_msgbuf ossBuf;
	struct pcb_t *pcb;

	// Get vars passed from OSS as command line args
	pcbIndex = strtol(argv[0], NULL, 10);
	numProcess = strtol(argv[1], NULL, 10);
	interruptProb = strtol(argv[2], NULL, 10);
	terminateProb = strtol(argv[3], NULL, 10);

	// Setup signal handlers

	sa.sa_handler = sigHandler;
  	sigemptyset(&sa.sa_mask);
  	sa.sa_flags = 0;
	
	if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: Process cannot handle SIGINT");
		exit(EXIT_FAILURE);
	}
	
	if (sigaction(SIGABRT, &sa, NULL) == -1) {
        perror("Error: Process cannot handle SIGABRT");
		exit(EXIT_FAILURE);
	}

	// Attach to shared memory
	attachMemory(numProcess);

	// Reference to this prcoesses pcb in shared memory
	pcb = &g_pcb[pcbIndex];

	// Seed random, changing seed based on pid
	srand(time(NULL) ^ (getpid() << 16));

	// Main loop
	while (1) {
		// Wait to be scheduled by OSS -- message type is our pid
		if (msgrcv(g_mschId, &schBuf, sizeof(struct sch_msgbuf), msgId, 0) == -1){
			fprintf(stderr, "Process %d failed to receive schedule message: %s", getpid(), strerror(errno));
			cleanUp();
			exit(EXIT_FAILURE);
		}

		// Set initial values of return message
		ossBuf.mtype = 1;
		ossBuf.finished = false;
		ossBuf.interrupt = false;
		ossBuf.index = pcbIndex;

		quantum = schBuf.quantum;

		// Check for termination using the termination probability
		// 1 in terminateProb chance of terminating this cycle
		if ((rand() % terminateProb) == (terminateProb - 1)) {
			// How much of this quantum to use before terminating
			quantum = rand() % (quantum + 1);
			pcb->lastBurst = quantum;
			ossBuf.finished = true;
			// Break main loop
			break;
		}

		// Check for interrupt using the interrupt probability
		if ((rand() % interruptProb) == (interruptProb - 1)) {
			// How much of this quantum to use before the interrupt occurs
			quantum = rand() % (quantum + 1);
			ossBuf.interrupt = true;
		}

		pcb->lastBurst = quantum;

		// If not termination, pass lock back to message queue
		if (msgsnd(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), 0) == -1) {
			fprintf(stderr, "Process %d failed to send OSS message: %s", getpid(), strerror(errno));
			cleanUp();
			exit(EXIT_FAILURE);
		}
	}
	
	// Send final message back to OSS -- no blocked waiting, just send and gtfo
	if (msgsnd(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), IPC_NOWAIT) == -1) {
		fprintf(stderr, "Process %d failed to send OSS term message: %s", getpid(), strerror(errno));
		cleanUp();
		exit(EXIT_FAILURE);
	}

	cleanUp();

	exit(EXIT_SUCCESS);
}
