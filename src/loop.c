#include "nub.h"
#include "fuq.h"
#include "util.h"
#include "uv.h"

#include <stdlib.h>  /* malloc, free */


void nub__async_dispose(uv_handle_t* handle) {
  free(handle);
}


void nub__thread_dispose(uv_async_t* handle) {
  fuq_queue* queue;
  nub_loop_t* loop;
  nub_thread_t* thread;

  loop = (nub_loop_t*) handle->data;
  queue = &loop->thread_dispose_queue_;
  while (!fuq_empty(queue)) {
    thread = (nub_thread_t*) fuq_shift(queue);

    uv_cond_signal(&thread->cond_wait_);
    uv_thread_join(&thread->uvthread);
    uv_close((uv_handle_t*) thread->async_signal_, nub__async_dispose);
    uv_sem_destroy(&thread->blocker_sem_);
    uv_cond_destroy(&thread->cond_wait_);
    uv_mutex_destroy(&thread->cond_mutex_);
    --thread->nubloop->ref_;
    thread->nubloop = NULL;
  }
}


void nub_loop_init(nub_loop_t* loop) {
  uv_async_t* async_handle;
  int er;

  er = uv_loop_init(&loop->uvloop);
  ASSERT(0 == er);

  er = uv_sem_init(&loop->blocker_sem_, 0);
  ASSERT(0 == er);

  fuq_init(&loop->thread_dispose_queue_);

  async_handle = (uv_async_t*) malloc(sizeof(*async_handle));
  CHECK_NE(NULL, async_handle);
  er = uv_async_init(&loop->uvloop, async_handle, nub__thread_dispose);
  ASSERT(0 == er);
  async_handle->data = loop;
  loop->thread_dispose_ = async_handle;
  uv_unref((uv_handle_t*) loop->thread_dispose_);

  loop->ref_ = 0;
}


int nub_loop_run(nub_loop_t* loop, uv_run_mode mode) {
  return uv_run(&loop->uvloop, mode);
}


void nub_loop_dispose(nub_loop_t* loop) {
  ASSERT(0 == loop->ref_);
  ASSERT(0 == uv_loop_alive(&loop->uvloop));
  ASSERT(1 == fuq_empty(&loop->thread_dispose_queue_));
  ASSERT(NULL != loop->thread_dispose_);
  ASSERT(0 == uv_has_ref((uv_handle_t*) loop->thread_dispose_));

  uv_close((uv_handle_t*) loop->thread_dispose_, nub__async_dispose);
  ASSERT(0 == uv_is_active((uv_handle_t*) loop->thread_dispose_));

  fuq_dispose(&loop->thread_dispose_queue_);
  uv_sem_destroy(&loop->blocker_sem_);
  CHECK_EQ(0, uv_run(&loop->uvloop, UV_RUN_NOWAIT));
  CHECK_NE(UV_EBUSY, uv_loop_close(&loop->uvloop));
}


int nub_loop_block(nub_thread_t* thread) {
  int er;
  /* Send signal to event loop thread that work needs to be done. */
  er = uv_async_send(thread->async_signal_);
  /* Pause thread until uv_async_cb has run. */
  uv_sem_wait(&thread->blocker_sem_);
  return er;
}


void nub_loop_resume(nub_loop_t* loop) {
  uv_sem_post(&loop->blocker_sem_);
}
