#include "engine_globals.h"

enum { task_max = 32 };

/* touched by the UI thread ONLY (start, finish, cancel, describe). The worker only touches the atomics inside its task. */
static task* s_tasks[task_max];

static void task_finish(void* p);

/* worker thread */
static void task_run(void* p)
{
    task* t = (task*)p;
    if (t->cancel) {
        InterlockedExchange(&t->state, TASK_CANCELED);
    } else {
        InterlockedExchange(&t->state, TASK_RUNNING);
        t->work(t);
        InterlockedExchange(&t->state, t->cancel ? TASK_CANCELED : TASK_DONE);
    }

    /* hand the task back to the UI thread: that is where apply and the cleanup run */
    for (int tries = 0; tries < 200; tries++) {
        if (submit_job(JOB_THREAD_UI, task_finish, t)) return;
        Sleep(5);
    }
    phoenix_error("TASK", "UI queue stayed full, task '%s' could not be finished.", t->name);
}

/* UI thread */
static void task_finish(void* p)
{
    task* t = (task*)p;
    if (t->state == TASK_DONE && !t->cancel && t->apply) t->apply(t);

    if (t->data) {
        if (t->free_data) t->free_data(t->data);
        else              free(t->data);
    }
    s_tasks[t->slot] = NULL;
    free(t);
}

task* task_start(const char* name, task_work_fn work, task_apply_fn apply, task_free_fn free_data,
                 void* data, uint32_t object_id, uint32_t revision)
{
    if (!work) return NULL;

    int slot = -1;
    for (int i = 0; i < task_max; i++) if (!s_tasks[i]) { slot = i; break; }
    if (slot < 0) { phoenix_error("TASK", "Too many background tasks."); return NULL; }

    task* t = (task*)calloc(1, sizeof(task));
    if (!t) return NULL;
    snprintf(t->name, sizeof(t->name), "%s", name ? name : "Task");
    t->work = work;  t->apply = apply;  t->free_data = free_data;
    t->data = data;  t->object_id = object_id;  t->revision = revision;
    t->state = TASK_QUEUED;
    t->slot  = slot;

    s_tasks[slot] = t;
    if (!submit_worker_job(task_run, t)) {
        s_tasks[slot] = NULL;
        free(t);
        phoenix_error("TASK", "Worker queues are full.");
        return NULL;
    }
    return t;       /* valid until its apply step has run: do not keep it past the next frame without checking task_active_count() */
}

void task_cancel(task* t)
{
    if (t) InterlockedExchange(&t->cancel, 1);
}

void task_cancel_object(uint32_t object_id)
{
    for (int i = 0; i < task_max; i++)
        if (s_tasks[i] && s_tasks[i]->object_id == object_id) InterlockedExchange(&s_tasks[i]->cancel, 1);
}

bool task_should_cancel(const task* t)
{
    return t->cancel != 0;
}

void task_set_progress(task* t, int percent)
{
    InterlockedExchange(&t->progress, percent < 0 ? 0 : (percent > 100 ? 100 : percent));
}

int task_active_count(void)
{
    int n = 0;
    for (int i = 0; i < task_max; i++) if (s_tasks[i]) n++;
    return n;
}

bool task_describe(int index, char* out, size_t out_size)
{
    int seen = 0;
    for (int i = 0; i < task_max; i++) {
        if (!s_tasks[i]) continue;
        if (seen++ != index) continue;
        const task* t = s_tasks[i];
        if (t->state == TASK_QUEUED) snprintf(out, out_size, "%s  queued", t->name);
        else                         snprintf(out, out_size, "%s  %d%%", t->name, (int)t->progress);
        return true;
    }
    return false;
}