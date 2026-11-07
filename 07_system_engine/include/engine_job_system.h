#ifndef PHOENIX_ENGINE_JOB_SYSTEM
#define PHOENIX_ENGINE_JOB_SYSTEM

#include "engine_defines.h"

/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/

typedef void (*JobDispatchRoutine)(void* user_data);

typedef struct {
    JobDispatchRoutine execute_task;
    void*              user_data;
} engine_job_packet;

typedef enum {
    JOB_THREAD_UI = 0,
    JOB_THREAD_FRONTEND,
    JOB_THREAD_BACKEND,
    /* The three above ARE the frame chain (system_frame_jobs.c). Never queue long work on them: every frame waits behind it.
       Background work (bakes, heavy mesh operators, file io) goes to the workers, through submit_worker_job(). */
    JOB_THREAD_WORKER_0,
    JOB_THREAD_WORKER_1,
    JOB_THREAD_COUNT
} job_thread_id;

#define JOB_WORKER_FIRST  JOB_THREAD_WORKER_0
#define JOB_WORKER_COUNT  2

#define JOB_QUEUE_CAPACITY 1024   /* must be a power of two */

typedef struct {
    engine_job_packet slots[JOB_QUEUE_CAPACITY];
    uint32_t          head;       /* next pop  */
    uint32_t          tail;       /* next push */
    CRITICAL_SECTION  lock;
    HANDLE            wake;       /* counting semaphore: 1 release per queued job */
} job_queue;

typedef struct {
    HANDLE         handle;
    job_queue      queue;
    volatile LONG  running;
    volatile LONG  busy;        /* 1 while a job is executing: submit_worker_job counts it as load */
    const char*    name;
} job_thread;

bool start_job_system(void);
void shutdown_job_system(void);
bool submit_job(job_thread_id target, JobDispatchRoutine fn, void* user_data);

/* Runs fn on the LEAST LOADED worker (queued jobs + the one running). Workers run below normal priority so they never fight the frame.
   fn must not touch editor data, Vulkan, or the object database. Give it its own copy of what it needs (see system_task.h). */
bool submit_worker_job(JobDispatchRoutine fn, void* user_data);

#endif

