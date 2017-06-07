Multi level feedback queue scheduling simulation
Author: Gabriel Murphy
Date: Mon Mar 20 2017

This is a modified version of a project I did for an Operating Systems class 
at my university. It is meant to simulate an OS scheduling processes using 
a basic mlfq scheme.

OSS has a set number of queues (0 to n-1, where n is definable in prefs) each with
its own time quantum equal to 2^i * factor (factor is set in prefs).
OSS forks child processes at random intervals which are initially placed in the
top queue (q0). 
Each cycle, OSS checks the queues from top (q0) to bottom (qn-1), dispatching
the first process it finds. When a process is dispatched, it randomly decides
whether it should be interrupted by I/O, terminate, or just use the full quantum
it was granted. The decision is based on random probabilities chosen when child was
generated, thus each process has different characteristics: e.g. one might have high
probability of I/O interrupts, whereas another has low probability of termination, etc.
After this, the process indicates to OSS its decision and cedes control. 
If the process used all of its quantum (no I/O or termination), OSS simply moves it
down to the next queue (down to n-1), thus the next time it is dispatched it will
be given a longer quantum. If a process was interrupted, its priority will be reset
to the top queue (q0) and it will generate a random time for when that processes I/O
has been handled (in which case it will be dispatched). If the process has
terminated OSS will simply log its data for statistics and then clean up after
the process.
Moving processes who were interrupted up in priority and processses w/o interrupts
down facilitates I/O-bound processes to stay at the top of the queues meaning
they will be more responsive, while non-I/O bound processes (think batch processing)
will stay in the lower queues and take advantage of their significantly 
longer quantums.
Note: No "againg" scheme has been implemented so starvation of batch processes can
and does occur in this simulation.
Concurrency is maintained using a message queue in shared memory.

The ranges the probabilities, and many others properties, are contained in a text 
file called "pref.dat" which of course can be adjusted w/o having to re-compile.
Each preference in "pref.dat" includes a description of what it sets/does.

During the simulation, the status of the system is printed to the terminal:

 
A log file is also created during the simulation and writes each time an event
occurs (new process, process dispatched from which queue, etc.) 
along with with timestamps (simulated system time).

Requirements: A *nix system that supports ipcs. A C compiler and Make. 
The Make file uses gcc.
Note: W10's "Bash on Ubuntu on Windows" does not currently support ipcs.

Instructions: run cmd 'make' or 'make all' to compile both executables
of the project.
Then run cmd './OSS' to run the project with the default preferences.
Running the cmd 'make clean' will remove all object files, the two
executables, and the log file (if one was generated).

OSS accepts command line arguments to adjust some properties of the sim:
			-h: displays help message
			-s [integer]: number of simultaneous proccesses (max 18)
			-t [integer]: number of (real) seconds OSS will run
			-l [filename]: name of file where log will be written