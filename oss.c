/**
* oss.c
* Author: Gabriel Murphy
* Date: Mon Mar 20 2017
* Summary: Simulates multi level feedback queues for scheduling processes
* to share a single thread. OSS sets up the shared memory, controls the
* simulated system clock, randomly generates processes who then wait
* to be dispatched from the mlfq. OSS also handles the return of dispatched
* processes, prints the status of the system each cycle, and handles the
* end of the of the simulation.
*/
#include "oss.h"
#include "queue.h"
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
#include <sys/wait.h>

#define DFLT_FILEN "test.out"

// Shared memory vars, need to be global for signal handling purposes
stime_t *g_stime;
int g_stimeId;

pcb_t *g_pcb;
int g_pcbId;

int g_mschId;
int g_mossId;

FILE *g_output;

/**
* Detatches pointer from shared mem and marks the segment for removal
* Returns false if either op fails, true if both successful
*/
bool cleanSegment(void *pointer, int id) {
	return (shmdt(pointer) != -1) && (shmctl(id, IPC_RMID, NULL) != -1);
}

/**
* Marks a message queue for removal
* Returns false if error, true if successful
*/
bool cleanMsgQueue(int id) {
	return msgctl(id, IPC_RMID, NULL) != -1;
}

/**
* Removes all shared mem segments and message queues
* *Must* be called before any exit of process
*/
void cleanUp() {
	bool success = true;
	fclose(g_output);

	if (!cleanSegment(g_stime, g_stimeId) || !cleanSegment(g_pcb, g_pcbId)) {
		fprintf(stderr, 
			"OSS failed to remove shared mem segments: %s\n", strerror(errno));
		success = false;
	}

	if (!cleanMsgQueue(g_mschId) || !cleanMsgQueue(g_mossId)) {
		fprintf(stderr, 
			"OSS failed to remove shared message queues: %s\n", strerror(errno));
		success = false;
	}

	if (!success) {
		fprintf(stderr, "OSS failed to clean up\n");
		exit(EXIT_FAILURE);
	}
}

/**
* Allocate and link to shared memory segments
*/
void setupMemory(int numProcess) {
	// Allocate segment of shared memory
	if ((g_stimeId = shmget(STM_KEY, sizeof(stime_t), IPC_CREAT | 0660)) < 0) {
		perror("Allocating shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	// Attach shm to segment for access
	if ((g_stime = shmat(g_stimeId, NULL, 0)) == (stime_t*) -1) {
		perror("Attach shared memory failed in OSS");
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	// Allocate segment of shared memory for pcb array
	if ((g_pcbId = shmget(PCB_KEY, sizeof(struct pcb_t) * numProcess, IPC_CREAT | 0660)) < 0) {
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
	g_stime->sec = 0;

	// Initialize shared nanoseconds to 0
	g_stime->nnsec = 0;

	int i;

	for (i = 0; i < numProcess; i++) {
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

/**
* Kills all child processes using their ids in the pcb
*/
void abortAll(int numProcess) {
	int i;

	for (i = 0; i < numProcess; i++) {
		if (g_pcb[i].exists) {
			kill(g_pcb[i].id, SIGABRT);
		}
	}

	while (wait(NULL) > 0);
}

/**
* Creates a new pcb entry and forks/execs new process
*/
void generateChild(int numProcess, int index, int intMin, int intMax, int termMin, int termMax) {
	struct pcb_t pcb;
	pid_t thisPid;
	int intProb, termProb;

	// Make sure previous child occupying this pcb spot has fully terminated
	if (g_pcb[index].id != -1) {
		waitpid(g_pcb[index].id, NULL, 0);
	}

	// Setup pcb values
	pcb.exists = true;
	pcb.waiting = false;

	pcb.startTime = *g_stime;
	
	pcb.priority = 0;
	pcb.lastBurst = 0;
	pcb.sysWaitTime.sec = 0;
	pcb.sysWaitTime.nnsec = 0;

	g_pcb[index] = pcb;

	intProb = (rand() % (intMax + 1 - intMin)) + intMin;
	termProb = (rand() % (termMax + 1 - termMin)) + termMin;

	// Fork and exec new process

	thisPid = fork();

	// Child does this
	if (thisPid == 0) {
		// index in the pcb array is sent to Process
		char indexArg[8];
		char procArg[8];
		char intProbArg[8];
		char termProbArg[8];
		snprintf(indexArg, 8, "%d", index);
		snprintf(procArg, 8, "%d", numProcess);
		snprintf(intProbArg, 8, "%d", intProb);
		snprintf(termProbArg, 8, "%d", termProb);

		// Exec Process from child
		execl("./Process", indexArg, procArg, intProbArg, termProbArg, NULL);

		// Should never reach here
		fprintf(stderr, "Failed to exec Process %d: %s", thisPid, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// Parent does this
	else {
		g_pcb[index].id = thisPid;
	}
}

/**
* Writes the given buffer to a file if linecount has not reached 1000
*/
void writeToLog(char *buff, int *lineCount, int maxLines) {
	if (*lineCount <= maxLines) {
		fputs(buff, g_output);
		fflush(g_output);
		(*lineCount)++;
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

int intPow(int base, unsigned int exp) {
	int i;
	int result = 1;

	for (i = 0; i < base; i++) {
		result *= base;
	}

	return result;
}

/**
* Reads the preference variables from a file and adds them to an array
*/
void readPreferences(int *prefs) {
	FILE *file;
	char *filename = "pref.dat";
	int count = 0;
	int i = 0;
	char buff[128];

	if ((file = fopen(filename, "r")) == NULL) {
		perror("Failed to open pref file in OSS");
		exit(EXIT_FAILURE);
	}

	while (fgets(buff, sizeof(buff), file) != NULL) {
		// Numbers are every other line of file (others are descriptions)
		if ((count % 2) != 0) {
			prefs[i] = strtol(buff, NULL, 10);
			i++;
		}

		count++;
	}

	fclose(file);
}

/**
* Clears terminal and prints the status of the system
*/
void printStatus(int pcbIndex, int numProcess, int numQueues, queue_t *queues) {
	int i;

	// Clear terminal (*nix systems only)
	printf("\033[2J");

	// Prints the status of each entry in the PCB
	
	printf("PCB:\n");

	for (i = 0; i < numProcess; i++) {
		printf("%2d |", i);
		if (g_pcb[i].exists) {
			printf("PID: %d ", g_pcb[i].id);
			printf("|Time in system: %2d.%-9d ", 
				g_stime->sec - g_pcb[i].startTime.sec, g_stime->nnsec - g_pcb[i].startTime.nnsec);
			printf("|Time waiting: %2d.%-9d ", g_pcb[i].sysWaitTime.sec, g_pcb[i].sysWaitTime.nnsec);
			if (g_pcb[i].waiting) {
				printf("*Waiting on I/O*");
			}
		}
		printf("\n");
	}

	// Print which processes are waiting on I/O

	printf("I/O Wait Queue:");

	for (i = 0; i < numProcess; i++) {
		if (g_pcb[i].exists && g_pcb[i].waiting) {
			printf(" |%d|", g_pcb[i].id);
		}
	}

	// Print the processes in each of the queues

	printf("\n");

	for (i = 0; i < numQueues; i++) {
		printf("Queue %d:", i);

		node_t *current = queues[i].head;

		while (current != NULL) {
			printf(" |%d|", g_pcb[current->index].id);

			current = current->next;
		}

		printf("\n");
	}

	printf("Simulated system time: %d.%d\n", g_stime->sec, g_stime->nnsec);

	printf("Process scheduled: ");

	if (pcbIndex != -1) {
		printf("|%d|", g_pcb[pcbIndex].id);
		if (g_pcb[pcbIndex].waiting) {
			printf(" *I/O*");
		}
		else if (!g_pcb[pcbIndex].exists) {
			printf(" *Finished*");
		}
		printf("\n");
	}
	else {
		printf("None\n");
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
			"-s [integer]: number of child processes to generate (max 18)\n"
			"-t [integer]: number of seconds OSS will wait\n"
			"-l [filename]: name of file where log will be written\n";
	int c = 0;

	// Read preferences from file
	int prefs[11];
	readPreferences(prefs);

	// preference variables
	char *filename = DFLT_FILEN;
	int numProcess = prefs[0];
	int numQueues = prefs[1];
	int waitReal = prefs[2];
	int waitSim = prefs[3];
	int generateRate = prefs[4];
	int quantumFactor = prefs[5];
	int workMax = prefs[6];
	int maxLines = prefs[7];
	int intMin = prefs[8];
	int intMax = prefs[9];
	int termMin = prefs[10];
	int termMax = prefs[11];
	int sleepAmount = prefs[12];
	int maxIoWait = prefs[13];

	// process variables
	int i = 0;
	int pcbIndex = -1;
	int workTime;
	
	// Used to track when to generate a new child
	stime_t generateTime = {0, 0};

	// Used to track statistics
	stime_t cpuIdleTime = {0, 0};
	stime_t totalWait = {0, 0};
	stime_t averageWait = {0, 0};
	stime_t totalTurn = {0, 0};
	stime_t averageTurn = {0, 0};

	// Priority queues -- where processes wait to be scheduled
	// Higher the queue, lower the priority
	queue_t *queues;

	// Quantum coresponding to each priority queue
	int *quantums;

	int lineCount = 1;
	int totalProcesses = 0;
	int totalFinished = 0;

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
			// Number of simultaneous processes 
			case 's':
				numProcess = strtol(optarg, NULL, 10);
				if (numProcess > 18) {
					fprintf(stderr, "Max number of simultaneous processes is 18\n");
					return 1;
				}
				break;
			// File to save log entries to
			case 'l':
				filename = strdup(optarg);
				break;
			// Max number of real-time seconds OSS runs for
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
	setupMemory(numProcess);

	// Allocate memory for array used for the priority queues	
	if ((queues = calloc(numQueues, sizeof(queue_t))) == NULL) {
		perror("Failed to allocate memory for priority queue array");
		cleanUp();
		return 1;
	}

	// Allocate memory for array used for the queue's quantum	
	if ((quantums = calloc(numQueues, sizeof(int))) == NULL) {
		perror("Failed to allocate memory for priority queue array");
		cleanUp();
		return 1;
	}

	// Initialize the priority queues and calculate quantums
	for (i = 0; i < numQueues; i++) {
		queues[i] = createQueue();
		// 2^i * quantumFactor
		quantums[i] = intPow(2, i) * quantumFactor;
	}

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
		// Generate new child processes if time reached
		if (gte(g_stime, &generateTime)) {
			int tempIndex = -1;

			// Find if there is a free pcb using the pcbVector
			for (i = 0; i < numProcess; i++) {
				if (!g_pcb[i].exists) {
					tempIndex = i;
					break;
				}
			}

			// Generate new child process if a free pcb was found
			if (tempIndex != -1) {
				generateChild(numProcess, tempIndex, intMin, intMax, termMin, termMax);

				totalProcesses++;

				// Add to queue based on its priority
				push(&queues[g_pcb[tempIndex].priority], tempIndex);
				
				snprintf(logBuff, 128, "** OSS: Generating process with PID %d"
					" and putting in queue %d at time %d.%d\n",
					g_pcb[tempIndex].id, g_pcb[tempIndex].priority, g_stime->sec, g_stime->nnsec);

				writeToLog(logBuff, &lineCount, maxLines);
			}

			// Time to generate next process
			generateTime = *g_stime;
			incrementTime(&generateTime, (rand() % generateRate) + 1);
		}

		// Print the current status of the system to terminal
		printStatus(pcbIndex, numProcess, numQueues, queues);

		pcbIndex = -1;

		// Check if there is a waiting process whose I/O has returned
		for (i = 0; i < numProcess; i++) {
			if (g_pcb[i].waiting && gte(g_stime, &g_pcb[i].ioFinishTime)) {
				g_pcb[i].waiting = false;
				pcbIndex = i;
				break;
			}
		}

		// If no waiting processes, find the next ready process to dispatch
		if (pcbIndex == -1) {
			// Check each queue in order of priority
			for (i = 0; i < numQueues; i++) {
				if (queues[i].size != 0) {
					pcbIndex = pop(&queues[i]);
					break;
				}
			}
		}

		workTime = (rand() % workMax) + 1;

		// Add cpu work time to simulated system time  
		incrementTime(g_stime, workTime);

		// Add cpu work time to the wait time of each active pcb
		for (i = 0; i < numProcess; i++) {
			if (g_pcb[i].exists && !g_pcb[i].waiting) {
				incrementTime(&g_pcb[i].sysWaitTime, workTime);
			}
		}

		// Schedule child process if one was found waiting
		if (pcbIndex != -1) {
			// Message type is set to child pid, child only waits for this type
			schBuf.mtype = g_pcb[pcbIndex].id;

			// Get quantum for the queue
			schBuf.quantum = quantums[g_pcb[pcbIndex].priority];

			snprintf(logBuff, 128, "OSS: Dispatching process with PID %d"
				                    " from queue %d at time %d.%d\n",
					g_pcb[pcbIndex].id, g_pcb[pcbIndex].priority, g_stime->sec, g_stime->nnsec);
			writeToLog(logBuff, &lineCount, maxLines);

			// Send message to child proccess (msg type is child pid) to schedule 
			if (msgsnd(g_mschId, &schBuf, sizeof(struct sch_msgbuf), 0) == -1) {
					perror("OSS failed to send sch messege");
					cleanUp();
					exit(EXIT_FAILURE);
			}

			snprintf(logBuff, 128, "  OSS: Total time this dispatch %d nanoseconds\n",
					workTime);
			writeToLog(logBuff, &lineCount, maxLines);

			// Wait for return message from child process
			if (msgrcv(g_mossId, &ossBuf, sizeof(struct oss_msgbuf), 1, 0) == -1){
					perror("OSS failed to receive return message");
					cleanUp();
					exit(EXIT_FAILURE);
			}

			snprintf(logBuff, 128, "    OSS: Receiving that process with PID %d"
					" ran for %d nanoseconds\n",
					g_pcb[pcbIndex].id, g_pcb[pcbIndex].lastBurst);
			writeToLog(logBuff, &lineCount, maxLines);

			// Increment system time to that of child's last burst
			incrementTime(g_stime, g_pcb[pcbIndex].lastBurst);

			// Increment the wait time of any active pcb except the one that just ran
			for (i = 0; i < numProcess; i++) {
				if (i != pcbIndex && g_pcb[i].exists && !g_pcb[i].waiting) {
					incrementTime(&g_pcb[i].sysWaitTime, g_pcb[pcbIndex].lastBurst);
				}
			}

			// If child proccess finished this burst
			if (ossBuf.finished) {
				snprintf(logBuff, 128, "XX OSS: Process with PID %d finished at time %d.%d\n",
					g_pcb[pcbIndex].id, g_stime->sec, g_stime->nnsec);
				writeToLog(logBuff, &lineCount, maxLines);

				// Subtract current time form start time and add it to total turnaround
				incrementTime(&totalTurn, combined(g_stime) - combined(&g_pcb[pcbIndex].startTime));
				
				// Add the pcb's wait time to total wait time
				incrementTime(&totalWait, combined(&g_pcb[pcbIndex].sysWaitTime));

				// Mark pcb as free
				g_pcb[pcbIndex].exists = false;
				g_pcb[pcbIndex].waiting = false;
				totalFinished++;
			}
			// Not finished so find which queue to go to
			else {
				// Child process was interrupted
				if (ossBuf.interrupt) {
					// After wait is handled, will be reset to priority 0 (highest priority)
					g_pcb[pcbIndex].priority = -1;

					snprintf(logBuff, 128, "    OSS: Process %d was interrupted, adding to wait queue\n",
							g_pcb[pcbIndex].id);
					writeToLog(logBuff, &lineCount, maxLines);

					// Mark as waiting for I/O
					g_pcb[pcbIndex].waiting = true;

					g_pcb[pcbIndex].ioFinishTime = *g_stime;
					incrementTime(&g_pcb[pcbIndex].ioFinishTime, (rand() % maxIoWait) + 1);
				}
				// Used all of quantum, adjust priority
				else {
					// Move down one level (unless already at lowest queue)
					g_pcb[pcbIndex].priority += (g_pcb[pcbIndex].priority < (numQueues - 1));

					// Add to back of proper queue
					push(&queues[g_pcb[pcbIndex].priority], pcbIndex);

					snprintf(logBuff, 128, "      OSS: Putting process with PID %d into queue %d\n",
						g_pcb[pcbIndex].id, g_pcb[pcbIndex].priority);
					writeToLog(logBuff, &lineCount, maxLines);
				}
			}
		}
		// No processes to be scheduled, CPU is idle
		else {
			// Larger increment needed to get to next process to generate
			incrementTime(g_stime, rand() % (workMax * 100));
			incrementTime(&cpuIdleTime, rand() % (workMax * 100));
		}

		// Check if OSS has generated the max number of children (total)
		if (totalProcesses >= 100) {
			printf("OSS has generated 100 total processes, exiting\n");
			break;
		}

		// Check for simulated system time reaching end point
		if (g_stime->sec >= waitSim) {
			printf("Simulated time ended %d.%d\n", g_stime->sec, g_stime->nnsec);
			break;
		}

		// Check for real system time reaching end point
		if (realTimeSinceEpoch() >= realEndTime) {
			printf("Real time ended\n");
			break;
		}

		// Allows printed status of system to be viewable to user
		usleep(sleepAmount);
	}	

	// Kill all active processes
	abortAll(numProcess);

	cleanUp();

	// Calculate average wait and turnaround time
	if (totalFinished != 0) {
		incrementTime(&averageWait, combined(&totalWait) / totalFinished);
		incrementTime(&averageTurn, combined(&totalTurn) / totalFinished);
	}

	printf("CPU Idle: %d.%d\n", cpuIdleTime.sec, cpuIdleTime.nnsec);
	printf("Average time waiting: %d.%d\n", averageWait.sec, averageWait.nnsec);
	printf("Average turnover: %d.%d\n", averageTurn.sec, averageTurn.nnsec);
	printf("OSS exiting...\n");

	return 0;
}
