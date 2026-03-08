// shared.h
// Created by: Matthew Kik
// Shared definitions for oss and worker

#ifndef SHARED_H
#define SHARED_H

#include <sys/types.h>

#define SHM_KEY  859285
#define MSG_KEY  859286
#define CLOCK_SIZE (2 * sizeof(int))  // seconds + nanoseconds
#define MAX_PROCESSES 20
#define NANO_PER_SEC 1000000000
#define QUARTER_SEC_NS 250000000      // 250ms in nanoseconds
#define REPLY_OFFSET 1000000          // offset so reply mtype != send mtype

// Process Control Block
struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int endingTimeSeconds;
    int endingTimeNano;
    int messagesSent;
};

// Message buffer for message queue
struct mymsg {
    long mtype;       // oss→child: pid, child→oss: pid + REPLY_OFFSET
    int mtext;        // 1 = still running, 0 = terminating
};

#endif
