/**
* stime.c
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: helper functions for the stime struct used for simulated system time
*/
#include "stime.h"

/**
* Increments a given stime struct by a number of nanoseconds
*/
void incrementTime(stime_t *time, long long delta) {
	// Done this way to avoid overflowing nnsec, probably simpler way TODO
	if (delta < NS_PER_S) {
		time->nnsec += delta;

		if (time->nnsec >= NS_PER_S) {
			time->sec++;
			time->nnsec -= NS_PER_S;
		}
	}
	else {
		while (delta >= NS_PER_S) {
			time->sec++;
			delta -= NS_PER_S;
		}

		time->nnsec = delta;
	}	
}

/**
* Checks if a is greater than or equal to b
*/
int gte(stime_t *a, stime_t *b) {
	if ((a->sec > b->sec) || (a->sec == b->sec && a->nnsec >= b->nnsec)) {
		return 1;
	}

	return 0;
}

/**
* Combines the two parts of stime struct (sec and nnsec) to get full time in nanoseconds
*/
long long combined(stime_t *time) {
	return ((long long) time->sec * NS_PER_S + (long long) time->nnsec);
}