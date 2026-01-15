#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#include <ucontext.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <signal.h>

#include "api.h"

typedef struct SetUpConfig {
    enum policy_type policy;
    void *vm;
    int vm_size;
    int num_frames;
    int page_size;
    struct mm_stats *stats;
} SetUpConfig;

extern SetUpConfig setUpConfig;

typedef struct FIFOPhysicalFrame{
    int frame_number;
    int virtualPageNumber;
    bool isWritePerformed;
    struct FIFOPhysicalFrame *next;
} FIFOPhysicalFrame;

extern FIFOPhysicalFrame *phyFrameLLHead;

typedef struct ThirdChanceFrame {
    int frame_number;
    int virtualPageNumber;
    int R;
    int M;
    int pass_count;
    struct ThirdChanceFrame *next;
} ThirdChanceFrame;
extern ThirdChanceFrame *thirdChanceHead;

void segfault_handler(int sig, siginfo_t *sigInfo, void *ucontext);