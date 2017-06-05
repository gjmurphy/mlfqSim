/**
* stime.h
* Author: Gabriel Murphy
* Date: Mon Mar 20 2017
* Summary: struct and helper functions used for simulated system time
*/
#ifndef S_TIME
#define S_TIME

// Number of nanoseconds in second
#define NS_PER_S 1000000000

typedef struct stime_t {
	int sec;
	int nnsec;
} stime_t;

void incrementTime(stime_t *, long long);

int gte(stime_t *, stime_t *);

long long combined(stime_t *);

#endif