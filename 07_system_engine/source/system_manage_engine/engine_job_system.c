#include "engine_globals.h"


static job_thread g_threads[JOB_THREAD_COUNT];

bool queue_push(job_queue* q, engine_job_packet job)
{
    bool ok = false;
    EnterCriticalSection(&q->lock);
    if (q->tail - q->head < JOB_QUEUE_CAPACITY) {
        q->slots[q->tail & (JOB_QUEUE_CAPACITY - 1)] = job;
        q->tail++;
        ok = true;
    }
    LeaveCriticalSection(&q->lock);
    return ok;
}

bool queue_pop(job_queue* q, engine_job_packet* out)
{
    bool ok = false;
    EnterCriticalSection(&q->lock);
    if (q->head != q->tail) {
        *out = q->slots[q->head & (JOB_QUEUE_CAPACITY - 1)];
        q->head++;
        ok = true;
    }
    LeaveCriticalSection(&q->lock);
    return ok;
}

DWORD WINAPI job_thread_main(LPVOID param)
{
    job_thread* self = (job_thread*)param;
    phoenix_info("JOBS", "Thread '%s' started.", self->name);

    for (;;) {
        WaitForSingleObject(self->queue.wake, INFINITE);
        bool did_run = false;
        engine_job_packet job;
        while (queue_pop(&self->queue, &job)) {
            did_run = true;
            InterlockedExchange(&self->busy, 1);
            if (job.execute_task) {
                job.execute_task(job.user_data);
            }
            InterlockedExchange(&self->busy, 0);
        }
        (void)did_run;

        if (!InterlockedCompareExchange(&self->running, 0, 0)) {
            break;   /* woken for shutdown and nothing left to run */
        }
    }

    phoenix_info("JOBS", "Thread '%s' stopped.", self->name);
    return 0;
}

bool start_job_system(void)
{
    static const char* names[JOB_THREAD_COUNT] = { "UI", "Frontend", "Backend", "Worker 0", "Worker 1" };

    for (int i = 0; i < JOB_THREAD_COUNT; i++) {
        job_thread* t = &g_threads[i];
        memset(t, 0, sizeof(*t));
        t->name    = names[i];
        t->running = 1;
        InitializeCriticalSection(&t->queue.lock);
        t->queue.wake = CreateSemaphoreA(NULL, 0, JOB_QUEUE_CAPACITY, NULL);
        t->handle     = CreateThread(NULL, 0, job_thread_main, t, 0, NULL);
        if (!t->handle) {
            phoenix_fatal("JOBS", "Failed to create thread '%s'.", t->name);
            return false;
        }
        if (i >= JOB_WORKER_FIRST) SetThreadPriority(t->handle, THREAD_PRIORITY_BELOW_NORMAL);   /* background work must never fight the frame */
    }
    return true;
}

void shutdown_job_system(void)
{
    for (int i = 0; i < JOB_THREAD_COUNT; i++) {
        InterlockedExchange(&g_threads[i].running, 0);
        ReleaseSemaphore(g_threads[i].queue.wake, 1, NULL);   /* wake so it can exit */
    }
    for (int i = 0; i < JOB_THREAD_COUNT; i++) {
        WaitForSingleObject(g_threads[i].handle, INFINITE);
        CloseHandle(g_threads[i].handle);
        CloseHandle(g_threads[i].queue.wake);
        DeleteCriticalSection(&g_threads[i].queue.lock);
    }
}

bool submit_job(job_thread_id target, JobDispatchRoutine fn, void* user_data)
{
    if (target < 0 || target >= JOB_THREAD_COUNT) {
        phoenix_error("JOBS", "Invalid job thread index %d.", (int)target);
        return false;
    }
    job_thread* t = &g_threads[target];
    engine_job_packet job = { fn, user_data };
    if (!queue_push(&t->queue, job)) {
        phoenix_error("JOBS", "Queue for '%s' is full.", t->name);
        return false;
    }
    ReleaseSemaphore(t->queue.wake, 1, NULL);
    return true;
}


uint32_t worker_load(job_thread* t)
{
    uint32_t depth;
    EnterCriticalSection(&t->queue.lock);
    depth = t->queue.tail - t->queue.head;
    LeaveCriticalSection(&t->queue.lock);
    return depth + (t->busy ? 1u : 0u);
}

bool submit_worker_job(JobDispatchRoutine fn, void* user_data)
{
    int best = JOB_WORKER_FIRST;
    uint32_t best_load = 0xFFFFFFFFu;
    for (int i = JOB_WORKER_FIRST; i < JOB_WORKER_FIRST + JOB_WORKER_COUNT; i++) {
        const uint32_t load = worker_load(&g_threads[i]);
        if (load < best_load) { best_load = load; best = i; }
    }
    return submit_job((job_thread_id)best, fn, user_data);
}