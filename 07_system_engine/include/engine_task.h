#ifndef PHOENIX_ENGINE_TASK_H
#define PHOENIX_ENGINE_TASK_H


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

/* ------------------------------------------------------------------------- */
/* Background tasks: bakes, heavy mesh operators, file io.                   */
/*                                                                           */
/*   UI thread:      task_start(...)  takes a SNAPSHOT of the data it needs     */
/*   worker thread:  work(t)          reads and writes ONLY t->data             */
/*   UI thread:      apply(t)         puts the result into the editor           */
/*                                                                           */
/* The editor's data belongs to the UI thread (every draw_* runs in job_ui_draw). A worker never touches it:      */
/* no g_terrain, no edit_mode_mesh, no scene objects, no Vulkan, and no pointer from object_db_get (it dies when the     */
/* block is set again). Copy what you need into t->data before task_start().                                     */
/*                                                                           */
/* apply() runs on the UI thread, between frames, so it may upload and set database blocks. Replaced GPU buffers  */
/* still go through em_gpu_retire (the previous frame may still be on the GPU).                                  */
/* ------------------------------------------------------------------------- */

typedef enum { TASK_QUEUED = 0, TASK_RUNNING, TASK_DONE, TASK_CANCELED } task_state;

typedef struct task task;
typedef void (*task_work_fn)(task* t);     /* worker thread. Poll task_should_cancel() in long loops, report with task_set_progress(). */
typedef void (*task_apply_fn)(task* t);    /* UI thread. Only called when the work finished and was not canceled. Check object_id / revision FIRST. */
typedef void (*task_free_fn)(void* data);  /* UI thread, always called once, after apply or cancel. NULL = plain free(). */

struct task {
    char          name[32];                /* shown in the status bar: "Bake curvature" */
    task_work_fn  work;
    task_apply_fn apply;
    task_free_fn  free_data;
    void*         data;                    /* the snapshot in, the result out. Owned by the task. */
    uint32_t      object_id;               /* apply() checks the object still exists */
    uint32_t      revision;                /* the owner's revision when the snapshot was taken (g_terrain.edit_revision...): apply() checks it is still current */
    volatile LONG state;                   /* task_state */
    volatile LONG cancel;
    volatile LONG progress;                /* 0..100, written by work */
    int           slot;
};

/* UI thread only. Returns NULL when 32 tasks are already running or the worker queues are full. */
task* task_start(const char* name, task_work_fn work, task_apply_fn apply, task_free_fn free_data,
                 void* data, uint32_t object_id, uint32_t revision);

void task_cancel(task* t);                 /* UI thread. The work stops at its next poll, apply is skipped, data is still freed. */
void task_cancel_object(uint32_t object_id);   /* UI thread. Call when a scene object is deleted. */

/* worker side */
bool task_should_cancel(const task* t);
void task_set_progress(task* t, int percent);

/* UI thread, for status bars: how many are running, and "Bake curvature  42%" for the i-th one. */
int  task_active_count(void);
bool task_describe(int index, char* out, size_t out_size);


#endif


