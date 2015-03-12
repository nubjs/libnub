#include "nub.h"
#include "fuq.h"
#include "util.h"
#include "uv.h"

#include <stdlib.h>  /* malloc, free */


static void nub__free_handle_cb(uv_handle_t* handle) {
  free(handle);
}


static void nub__async_prepare_cb(uv_prepare_t* handle) {
  nub_loop_t* loop;
  nub_thread_t* thread;
  nub_work_t* work;

  loop = (nub_loop_t*) handle->data;

  while (!fuq_empty(&loop->work_queue_)) {
    work = (nub_work_t*) fuq_dequeue(&loop->work_queue_);
    thread = (nub_thread_t*) work->thread;

    if (NUB_LOOP_QUEUE_LOCK == work->work_type) {
      uv_sem_post(&thread->thread_lock_sem_);
      uv_sem_wait(&loop->loop_lock_sem_);
    } else if (NUB_LOOP_QUEUE_WORK == work->work_type) {
      work->cb(thread, work, work->arg);
      /* TODO(trevnorris): Still need to implement returning status. */
    } else {
      UNREACHABLE();
    }
  }
}


static void nub__thread_dispose(uv_async_t* handle) {
  fuq_queue_t* queue;
  nub_loop_t* loop;
  nub_thread_t* thread;

  loop = (nub_loop_t*) handle->data;

  if (!loop->disposed_)
    return;

  queue = &loop->thread_dispose_queue_;
  while (!fuq_empty(queue)) {
    thread = (nub_thread_t*) fuq_dequeue(queue);
    ASSERT(NULL != thread);
    nub_thread_join(thread);
    if (NULL != thread->disposed_cb_)
      thread->disposed_cb_(thread);
  }
}


void nub_loop_init(nub_loop_t* loop) {
  uv_async_t* async_handle;
  int er;

  er = uv_loop_init(&loop->uvloop);
  ASSERT(0 == er);

  er = uv_prepare_init(&loop->uvloop, &loop->queue_processor_);
  ASSERT(0 == er);
  loop->queue_processor_.data = loop;
  uv_unref((uv_handle_t*) &loop->queue_processor_);

  er = uv_mutex_init(&loop->queue_processor_lock_);
  ASSERT(0 == er);

  fuq_init(&loop->blocking_queue_);

  er = uv_sem_init(&loop->loop_lock_sem_, 0);
  ASSERT(0 == er);

  fuq_init(&loop->thread_dispose_queue_);

  er = uv_mutex_init(&loop->thread_dispose_lock_);
  ASSERT(0 == er);

  fuq_init(&loop->work_queue_);

  er = uv_mutex_init(&loop->work_lock_);
  ASSERT(0 == er);

  async_handle = (uv_async_t*) malloc(sizeof(*async_handle));
  CHECK_NE(NULL, async_handle);
  er = uv_async_init(&loop->uvloop, async_handle, nub__thread_dispose);
  ASSERT(0 == er);
  async_handle->data = loop;
  loop->work_ping_ = async_handle;
  uv_unref((uv_handle_t*) loop->work_ping_);

  loop->ref_ = 0;
  loop->disposed_ = 0;

  er = uv_prepare_start(&loop->queue_processor_, nub__async_prepare_cb);
  ASSERT(0 == er);
}


int nub_loop_run(nub_loop_t* loop, uv_run_mode mode) {
  return uv_run(&loop->uvloop, mode);
}


void nub_loop_dispose(nub_loop_t* loop) {
  ASSERT(0 == uv_loop_alive(&loop->uvloop));
  ASSERT(1 == fuq_empty(&loop->blocking_queue_));
  ASSERT(0 == loop->ref_);
  ASSERT(NULL != loop->work_ping_);
  ASSERT(0 == uv_has_ref((uv_handle_t*) loop->work_ping_));
  ASSERT(1 == fuq_empty(&loop->thread_dispose_queue_));

  uv_close((uv_handle_t*) loop->work_ping_, nub__free_handle_cb);
  uv_close((uv_handle_t*) &loop->queue_processor_, NULL);
  ASSERT(0 == uv_is_active((uv_handle_t*) loop->work_ping_));

  fuq_dispose(&loop->thread_dispose_queue_);
  uv_mutex_destroy(&loop->thread_dispose_lock_);
  uv_sem_destroy(&loop->loop_lock_sem_);
  fuq_dispose(&loop->blocking_queue_);
  uv_mutex_destroy(&loop->queue_processor_lock_);
  fuq_dispose(&loop->work_queue_);
  uv_mutex_destroy(&loop->work_lock_);

  CHECK_EQ(0, uv_run(&loop->uvloop, UV_RUN_NOWAIT));
  CHECK_NE(UV_EBUSY, uv_loop_close(&loop->uvloop));
}


/* Should be run from spawned thread. */
int nub_loop_lock(nub_thread_t* thread) {
  fuq_queue_t* queue;
  uv_mutex_t* mutex;
  int er;
  int is_empty;

  ASSERT(NULL != thread);

  queue = &thread->nubloop->work_queue_;
  mutex = &thread->nubloop->work_lock_;

  if (NUB_LOOP_QUEUE_DISPOSE != thread->work.work_type)
    thread->work.work_type = NUB_LOOP_QUEUE_LOCK;

  uv_mutex_lock(mutex);
  is_empty = fuq_empty(queue);
  fuq_enqueue(queue, &thread->work);
  uv_mutex_unlock(mutex);

  if (is_empty)
    /* Send signal to event loop thread that work needs to be done. */
    er = uv_async_send(thread->async_signal_);
  else
    er = 0;

  /* Pause thread until uv_async_cb has run. */
  uv_sem_wait(&thread->thread_lock_sem_);

  return er;
}


void nub_loop_unlock(nub_thread_t* thread) {
  uv_sem_post(&thread->nubloop->loop_lock_sem_);
}


void nub_loop_enqueue(nub_thread_t* thread,
                      nub_work_t* work,
                      nub_complete_cb cb) {
  nub_loop_t* loop;

  ASSERT(NULL != thread);
  ASSERT(NULL == work->thread);

  loop = thread->nubloop;

  work->thread = thread;
  work->complete_cb = cb;
  work->work_type = NUB_LOOP_QUEUE_WORK;

  uv_mutex_lock(&loop->work_lock_);
  fuq_enqueue(&loop->work_queue_, work);
  uv_mutex_unlock(&loop->work_lock_);
  uv_async_send(thread->nubloop->work_ping_);
}
