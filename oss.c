/**
* oss.c
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: 
*/
#include "oss.h"
#include "queue.h"
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
stime_t *g_stm;
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
void setupMemory(int spawns) {
	int i;

	// Allocate segment of shared memory
	if ((g_stmId = shmget(STM_KEY, sizeof(stime_t), IPC_CREAT | 0660)) < 0) {
		perror("Allocating shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((g_stm = shmat(g_stmId, NULL, 0)) == (stime_t*) -1) {
		perror("Attach shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	// Allocate segment of shared memory for pcb array
	if ((g_pcbId = shmget(PCB_KEY, sizeof(struct pcb_t) * spawns, IPC_CREAT | 0660)) < 0) {
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
	g_stm->sec = 0;

	// Initialize shared nanoseconds to 0
	g_stm->nnsec = 0;

	for (i = 0; i < spawns; i++) {
		g_pcb[i].id = -1;
	}

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

void abortAll(int spawns) {
	int i;

	for (i = 0; i < spawns; i++) {
		if (g_pcb[i].id != -1) {
			kill(g_pcb[i].id, SIGABRT);
		}
	}

	while (wait(NULL) > 0);
}

void generateChild(int spawns, int index) {
	if (g_pcb[index].id != -1) {
		waitpid(g_pcb[index].id, NULL, 0);
	}

	struct pcb_t pcb;
	pid_t thisPid;

	pcb.priority = 0;

	pcb.burst_needed = (rand() % (BURST_MAX - BURST_MIN)) + BURST_MIN;
	pcb.last_burst = 0;

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
		pcb.id = thisPid;
	}

	g_pcb[index] = pcb;
}

void writeToLog(char *buff, int *lineCount) {
	if (*lineCount <= 1000) {
		fputs(buff, g_output);
		fflush(g_output);
		*lineCount++;
	}
}

/**
* Returns the time in nanoseconds since epoch
*/
long long realTimeSinceEpoch() {
	struct timespec timeSpec;

	clock_gettime(CLOCK_REALTIME, &timeSpec);

	// timespec has two fields that must be combined to get full nanoseconds
	return ((long long) timeSpec.tv_sec * 1000000000L + (long long) timeSpec.tv_nsec);
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
			"-l [filename]: name of file where log will be written\n";
	int c = 0;
	
	// preference variables
	int spawns = DFLT_SPAWNS;
	int waitReal = DFLT_RWAIT;
	int waitSim = DFLT_SWAIT;
	char *filename = DFLT_FILEN;

	// process variables
	int i = 0;
	
	stime_t spawnTime = {0, 0};
	stime_t cpuIdleTime = {0, 0};
	stime_t totalWait = {0, 0};
	stime_t averageWait = {0, 0};

	queue_t queues[3];

	for (i = 0; i < 3; i++) {
		queues[i] = createQueue();
	}

	int quantums[3] = {QUANT0, QUANT1, QUANT2};
	int lineCount = 1;
	int totalProcesses = 0;

	short *pcbVector;

	char logBuff[128];

	long long realEndTime;

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

	// Allocate memory for array used to denote which pcb is free	
	if ((pcbVector = calloc(1, sizeof(short) * spawns)) == NULL) {
		perror("Failed to allocate memory for pcbVector array");
		cleanUp();
		return 1;
	}

	memset(pcbVector, 0, spawns);

	// Delete old log file if it exists
	remove(filename);

	// Attempt to open file in append mode
	if ((g_output = fopen(filename, "a")) == NULL) {
		perror("Failed to open file in OSS");
		cleanUp();
		return 1;
	}

	// The real time in nanoseconds that OSS should terminate if hasn't already finished
	realEndTime = realTimeSinceEpoch() + (waitReal * ((long long) NS_PER_S));

	srand(time(NULL));

	// Main loop
	while (1) {
		int pcbIndex = -1;
		int workTime = rand() % 1001;

		// Spawn new child processes if time
		if (gte(g_stm, &spawnTime)) {
			int tempIndex = -1;

			for (i = 0; i < spawns; i++) {
				if (!pcbVector[i]) {
					tempIndex = i;
				}
			}

			if (tempIndex != -1) {
				generateChild(spawns, tempIndex);
				pcbVector[tempIndex] = 1;
				totalProcesses++;
				push(&queues[0], tempIndex);
				
				snprintf(logBuff, 128, "OSS: Generating process with PID %d" 
					" and putting in queue %d at time %d.%d\n",
					g_pcb[tempIndex].id, 0, g_stm->sec, g_stm->nnsec);

				printf("%s", logBuff);

				writeToLog(logBuff, &lineCount);
			}

			// Generate new spawn time
			spawnTime = *g_stm;
			incrementTime(&spawnTime, rand() % SPAWN_RT);
		}

		// Iterate system time  
		incrementTime(g_stm, workTime);		

		// Find next process to schedule
		if (queues[0].size != 0) {
			pcbIndex = pop(&queues[0]);
		}
		else if (queues[1].size != 0) {
			pcbIndex = pop(&queues[1]);	
		}
		else if (queues[2].size != 0) {
			pcbIndex = pop(&queues[2]);	
		}

		// Schedule child process if one was found waiting
		if (pcbIndex != -1) {
			schBuf.mtype = g_pcb[pcbIndex].id;
			schBuf.quantum = quantums[g_pcb[pcbIndex].priority];

			snprintf(logBuff, 128, "OSS: Dispatching process with PID %d at time %d.%d\n",
					g_pcb[pcbIndex].id, g_stm->sec, g_stm->nnsec);
			writeToLog(logBuff, &lineCount);

			if (msgsnd(g_mschId, &schBuf, sizeof(struct sch_msgbuf), 0) == -1) {
					perror("OSS failed to send sch messege");
					cleanUp();
					exit(EXIT_FAILURE);
			}

			snprintf(logBuff, 128, "OSS: Total time this dispatch %d nanoseconds\n",
					workTime);
			writeToLog(logBuff, &lineCount);

			if (msgrcv(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), 1, 0) == -1){
					perror("OSS failed to receive return message");
					cleanUp();
					exit(EXIT_FAILURE);
			}

			snprintf(logBuff, 128, "OSS: Receiving that process with PID %d"
					" ran for %d nanoseconds\n",
					g_pcb[pcbIndex].id, g_pcb[pcbIndex].last_burst);
			writeToLog(logBuff, &lineCount);

			incrementTime(g_stm, g_pcb[pcbIndex].last_burst);

			if (ossBuf.finished) {
				snprintf(logBuff, 128, "OSS: Process with PID %d finished at time %d.%d\n",
					g_pcb[pcbIndex].id, g_stm->sec, g_stm->nnsec);
				printf("%s", logBuff);
				writeToLog(logBuff, &lineCount);
				pcbVector[pcbIndex] = 0;
			}
			else {
				int priority = g_pcb[pcbIndex].priority;

				if (ossBuf.interrupt) {
					priority = 0;

					snprintf(logBuff, 128, "OSS: Not using its entire quantum\n");
					writeToLog(logBuff, &lineCount);
				}
				else if (priority < 2) {
					priority++;
				}

				snprintf(logBuff, 128, "OSS: Putting process with PID %d into queue %d\n",
					g_pcb[pcbIndex].id, priority);
				writeToLog(logBuff, &lineCount);

				push(&queues[priority], pcbIndex);
				g_pcb[pcbIndex].priority = priority;
			}
		}
		else {
			incrementTime(&cpuIdleTime, workTime);
		}

		// Increment wait times
		for (i = 0; i < spawns; i++) {
			if (i != pcbIndex && pcbVector[i]) {
				incrementTime(&totalWait, workTime);

				if (pcbIndex != -1) {
					incrementTime(&totalWait, g_pcb[pcbIndex].last_burst);	
				}
			}
		}

		// Check if OSS has spawned the max number of children
		if (totalProcesses >= 100) {
			printf("OSS has spawned 100 children, exiting\n");
			break;
		}

		// Check for simulated system time reaching end point
		if (g_stm->sec >= waitSim) {
			printf("Simulated time ended %d.%d\n", g_stm->sec, g_stm->nnsec);
			break;
		}

		// Check for real system time reaching end point
		if (realTimeSinceEpoch() >= realEndTime) {
			printf("Real time ended\n");
			break;
		}
	}	

	abortAll(spawns);

	cleanUp();

	incrementTime(&averageWait, combined(&totalWait) / totalProcesses);

	printf("Idle: %d.%d\n", cpuIdleTime.sec, cpuIdleTime.nnsec);
	printf("Average Wait: %d.%d\n", averageWait.sec, averageWait.nnsec);
	printf("OSS exiting...\n");

	return 0;
}
