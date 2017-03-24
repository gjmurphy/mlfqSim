/**
* oss.h
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: Contains definitions shared between OSS and Slave
* and also the structs of the message buffers used in the two
* message queues and the pcb 
*/
#ifndef OSS_H
#define OSS_H

#include "stime.h"

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
