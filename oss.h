/**
* oss.h
* Author: Gabriel Murphy
* Date: Mon Mar 20 2017
* Summary: Contains definitions shared between OSS and Process
* and also the structs of the message buffers used in the two
* message queues and the pcb 
*/
#ifndef OSS_H
#define OSS_H

#include "stime.h"

// C boolean type
typedef enum {false, true} bool;

// Hard coded keys for shared mem segments
#define STM_KEY 4220
#define PCB_KEY 5300

// Hard coded keys for message queues
#define SCH_KEY 6520
#define OSS_KEY 7762

// Struct used for the shared array of pcbs in OSS
typedef struct pcb_t {
	int id;
	bool exists;
	bool waiting;
	int priority;
	int lastBurst;
	stime_t sysWaitTime;
	stime_t ioFinishTime;
	stime_t startTime;
} pcb_t;

// Struct for sending message to Process when it has been scheduled 
struct sch_msgbuf {
	long mtype;
	int quantum;
};

// Struct for Process sending message back to OSS when it is finished
struct oss_msgbuf {
	long mtype;
	int index;
	bool interrupt;
	bool finished;
};

#endif
