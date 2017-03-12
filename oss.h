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

// Hard coded keys for shared mem segments
#define SHS_KEY 4220
#define SHN_KEY 4480
#define PCB_KEY 5300

// Hard coded keys for message queues
#define SCH_KEY 6520
#define OSS_KEY 7762

// Number of nanoseconds in second
#define NS_PER_S 1000000000

struct pcb_type {
	int priority;
	int time_created;
	int time_ended;
	int time_waiting;
	int cpu_time;
	int burst_time;
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
	int time_used;
};

#endif
