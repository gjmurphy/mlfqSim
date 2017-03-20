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

// The range of the burst needed by each generated child process
#define BURST_MAX 100000000
#define BURST_MIN 20000000

// The time in nanoseconds between each potential child spawn
#define SPAWN_RT 100000000

// Three quantums of the queues
#define QUANT0 200000
#define QUANT1 400000
#define QUANT2 800000

// Range from 0 to this, how much time the cpu will take scheduling
#define CPU_LOAD 1001

// Hard coded keys for shared mem segments
#define STM_KEY 4220
#define PCB_KEY 5300

// Hard coded keys for message queues
#define SCH_KEY 6520
#define OSS_KEY 7762

// Struct used for the shared array of pcbs in OSS
struct pcb_t {
	int id;
	int priority;
	int burst_needed;
	int last_burst;
	long wait_time;
	stime_t start_time;
};

// Struct for sending message to Slave when it has been scheduled 
struct sch_msgbuf {
	long mtype;
	int quantum;
};

// Struct for Slave sending message back to OSS when it is finished
struct oss_msgbuf {
	long mtype;
	int index;
	short interrupt;
	short finished;
};

#endif
