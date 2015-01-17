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

  loop = (nub_loop_t*) handle->data;
  while (!fuq_empty(&loop->async_queue_)) {
    thread = (nub_thread_t*) fuq_dequeue(&loop->async_queue_);
    uv_sem_post(&thread->blocker_sem_);
    uv_sem_wait(&loop->blocker_sem_);
  }
}


static void nub__thread_dispose(uv_async_t* handle) {
  fuq_queue_t* queue;
  nub_loop_t* loop;
  nub_thread_t* thread;

  loop = (nub_loop_t*) handle->data;
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

  er = uv_prepare_init(&loop->uvloop, &loop->async_runner_);
  ASSERT(0 == er);
  loop->async_runner_.data = loop;
  uv_unref((uv_handle_t*) &loop->async_runner_);

  er = uv_mutex_init(&loop->async_mutex_);
  ASSERT(0 == er);

  fuq_init(&loop->async_queue_);

  er = uv_sem_init(&loop->blocker_sem_, 0);
  ASSERT(0 == er);

  fuq_init(&loop->thread_dispose_queue_);

  er = uv_mutex_init(&loop->thread_dispose_lock_);
  ASSERT(0 == er);

  async_handle = (uv_async_t*) malloc(sizeof(*async_handle));
  CHECK_NE(NULL, async_handle);
  er = uv_async_init(&loop->uvloop, async_handle, nub__thread_dispose);
  ASSERT(0 == er);
  async_handle->data = loop;
  loop->thread_dispose_ = async_handle;
  uv_unref((uv_handle_t*) loop->thread_dispose_);

  loop->ref_ = 0;

  er = uv_prepare_start(&loop->async_runner_, nub__async_prepare_cb);
  ASSERT(0 == er);
}


int nub_loop_run(nub_loop_t* loop, uv_run_mode mode) {
  return uv_run(&loop->uvloop, mode);
}


void nub_loop_dispose(nub_loop_t* loop) {
  ASSERT(0 == uv_loop_alive(&loop->uvloop));
  ASSERT(1 == fuq_empty(&loop->async_queue_));
  ASSERT(0 == loop->ref_);
  ASSERT(NULL != loop->thread_dispose_);
  ASSERT(0 == uv_has_ref((uv_handle_t*) loop->thread_dispose_));
  ASSERT(1 == fuq_empty(&loop->thread_dispose_queue_));

  uv_close((uv_handle_t*) loop->thread_dispose_, nub__free_handle_cb);
  uv_close((uv_handle_t*) &loop->async_runner_, NULL);
  ASSERT(0 == uv_is_active((uv_handle_t*) loop->thread_dispose_));

  fuq_dispose(&loop->thread_dispose_queue_);
  uv_mutex_destroy(&loop->thread_dispose_lock_);
  uv_sem_destroy(&loop->blocker_sem_);
  fuq_dispose(&loop->async_queue_);
  uv_mutex_destroy(&loop->async_mutex_);

  CHECK_EQ(0, uv_run(&loop->uvloop, UV_RUN_NOWAIT));
  CHECK_NE(UV_EBUSY, uv_loop_close(&loop->uvloop));
}


/* Should be run from spawned thread. */
int nub_loop_lock(nub_thread_t* thread) {
  uv_mutex_t* mutex = &thread->nubloop->async_mutex_;
  fuq_queue_t* queue = &thread->nubloop->async_queue_;
  int er;
  int is_empty;


  uv_mutex_lock(mutex);
  is_empty = fuq_empty(queue);
  fuq_enqueue(queue, thread);
  uv_mutex_unlock(mutex);

  if (is_empty)
    /* Send signal to event loop thread that work needs to be done. */
    er = uv_async_send(thread->async_signal_);
  else
    er = 0;

  /* Pause thread until uv_async_cb has run. */
  uv_sem_wait(&thread->blocker_sem_);

  return er;
}


void nub_loop_unlock(nub_thread_t* thread) {
  uv_sem_post(&thread->nubloop->blocker_sem_);
}
