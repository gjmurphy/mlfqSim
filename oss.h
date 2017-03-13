/**
* oss.h
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: Contains definitions shared between OSS and Slave
* and also the structs of the message buffers used in the two
* message queues
*/
#ifndef OSS_H
#define OSS_H

// Default values of preferences
#define DFLT_SPAWNS 1
#define DFLT_RWAIT 2
#define DFLT_SWAIT 2
#define DFLT_FILEN "test.out"

// Hard coded keys for shared mem segments
#define STM_KEY 4220
#define PCB_KEY 5300

// Hard coded keys for message queues
#define SCH_KEY 6520
#define OSS_KEY 7762

// Number of nanoseconds in second
#define NS_PER_S 1000000

struct stime_t {
	int sec;
	int nnsec;
};

struct pcb_t {
	int id;
	int priority;
	struct stime_t time_created;
	struct stime_t time_ended;
	int time_waiting;
	int time_cpu;

	int burst_needed;
	int last_burst;
};

// Struct for sending message to Slave when it has been scheduled 
struct sch_msgbuf {
	long mtype;
	int quantum;
};

// Struct for sending messages between Slave's to handle mutual exclusion
struct oss_msgbuf {
	long mtype;
	int index;
	short interrupt;
	short finished;
};

#endif
