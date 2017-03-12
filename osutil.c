/**
* osutil.c
* 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: Implementation of ostil.h 
*          Contains various utilities for OS projects
*/
#include "osutil.h"
#include <time.h>
#include <stdio.h>

/**
* Returns the time in nanoseconds since epoch
*/
long long nsSinceEpoch() {
	struct timespec timeSpec;

	clock_gettime(CLOCK_REALTIME, &timeSpec);

	// timespec has two fields that must be combined to get full nanoseconds
	return ((long long) timeSpec.tv_sec * BILLION + (long long) timeSpec.tv_nsec);
}

/**
* Returns the seconds part of timespec
*/
long getSS() {	
	struct timespec timeSpec;

	clock_gettime(CLOCK_REALTIME, &timeSpec);

	return timeSpec.tv_sec;
}

/**
* Returns the nanoseconds part of timespec
*/
long getNN() {	
	struct timespec timeSpec;

	clock_gettime(CLOCK_REALTIME, &timeSpec);

	return timeSpec.tv_nsec;
}
