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

#include "stime.h"

// Default values of preferences
#define DFLT_SPAWNS 18
#define DFLT_RWAIT 20
#define DFLT_SWAIT 20
#define DFLT_FILEN "test.out"

#define BURST_MAX 100000000
#define BURST_MIN 50000000

#define SPAWN_RT 1000000

#define QUANT0 20000
#define QUANT1 40000
#define QUANT2 80000

// Hard coded keys for shared mem segments
#define STM_KEY 4220
#define PCB_KEY 5300

// Hard coded keys for message queues
#define SCH_KEY 6520
#define OSS_KEY 7762

struct pcb_t {
	int id;
	int priority;
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
