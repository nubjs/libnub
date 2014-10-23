#include "nub.h"
#include "fuq.h"
#include "util.h"
#include "uv.h"

#include <stdlib.h>  /* malloc */


void nub__work_signal_cb(uv_async_t* handle) {
  nub_thread_t* thread;
  nub_loop_t* loop;

  thread = (nub_thread_t*) handle->data;
  loop = thread->nubloop;
  /* Resume the calling thread. */
  uv_sem_post(&thread->blocker_sem_);
  /* Block the event loop thread. */
  uv_sem_wait(&loop->blocker_sem_);
}


void nub__thread_entry_cb(void* arg) {
  nub_thread_t* thread;
  void* item;

  thread = (nub_thread_t*) arg;

  for (;;) {
    item = fuq_shift(&thread->incoming_);

    while (NULL != item) {
      (*thread->work_cb_)(thread, item);
      item = fuq_shift(&thread->incoming_);
    }

    if (0 < thread->disposed)
      break;

    uv_cond_wait(&thread->cond_wait_, &thread->cond_mutex_);
  }

  ASSERT(NULL == fuq_shift(&thread->incoming_));
  fuq_dispose(&thread->incoming_);
}


int nub_thread_create(nub_loop_t* loop,
                      nub_thread_t* thread,
                      nub_thread_work_cb work_cb) {
  uv_async_t* async_handle;
  int er;

  async_handle = (uv_async_t*) malloc(sizeof(*async_handle));
  CHECK_NE(NULL, async_handle);
  er = uv_async_init(&loop->uvloop, async_handle, nub__work_signal_cb);
  ASSERT(0 == er);
  async_handle->data = thread;
  thread->async_signal_ = async_handle;

  ASSERT(uv_loop_alive(&loop->uvloop));

  er = uv_sem_init(&thread->blocker_sem_, 0);
  ASSERT(0 == er);

  er = uv_cond_init(&thread->cond_wait_);
  ASSERT(0 == er);

  er = uv_mutex_init(&thread->cond_mutex_);
  ASSERT(0 == er);

  fuq_init(&thread->incoming_);
  thread->disposed = 0;
  thread->nubloop = loop;
  thread->work_cb_ = work_cb;
  ++loop->ref_;

  return uv_thread_create(&thread->uvthread, nub__thread_entry_cb, thread);
}


void nub_thread_dispose(nub_thread_t* thread) {
  thread->disposed = 1;
  fuq_push(&thread->nubloop->thread_dispose_queue_, thread);
  uv_async_send(thread->nubloop->thread_dispose_);
}


void nub_thread_push(nub_thread_t* thread, void* arg) {
  fuq_push(&thread->incoming_, arg);
  uv_cond_signal(&thread->cond_wait_);
}
