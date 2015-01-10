#include "nub.h"
#include "fuq.h"
#include "util.h"
#include "uv.h"

#include <stdlib.h>  /* malloc, free */


static void nub__free_handle_cb(uv_handle_t* handle) {
  free(handle);
}


static void nub__work_signal_cb(uv_async_t* handle) {
  nub_loop_t* loop;
  nub_thread_t* thread;

  loop = ((nub_thread_t*) handle->data)->nubloop;
  while (!fuq_empty(&loop->async_queue_)) {
    thread = (nub_thread_t*) fuq_dequeue(&loop->async_queue_);
    uv_sem_post(&thread->blocker_sem_);
    uv_sem_wait(&loop->blocker_sem_);
  }
}


static void nub__thread_entry_cb(void* arg) {
  nub_thread_t* thread;
  fuq_queue_t* queue;
  nub_work_t* item;

  thread = (nub_thread_t*) arg;
  queue = &thread->incoming_;

  for (;;) {
    while (!fuq_empty(queue)) {
      item = (nub_work_t*) fuq_dequeue(queue);
      (item->cb)(thread, item->arg);
    }
    if (0 < thread->disposed)
      break;
    uv_cond_wait(&thread->cond_wait_, &thread->cond_mutex_);
  }

  ASSERT(1 == fuq_empty(queue));
  fuq_dispose(&thread->incoming_);
}


int nub_thread_create(nub_loop_t* loop, nub_thread_t* thread) {
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
  thread->disposed_cb_ = NULL;
  ++loop->ref_;

  return uv_thread_create(&thread->uvthread, nub__thread_entry_cb, thread);
}


void nub_thread_dispose(nub_thread_t* thread, nub_thread_disposed_cb cb) {
  thread->disposed = 1;
  thread->disposed_cb_ = cb;
  fuq_enqueue(&thread->nubloop->thread_dispose_queue_, thread);
  uv_async_send(thread->nubloop->thread_dispose_);
}


void nub_thread_join(nub_thread_t* thread) {
  thread->disposed = 1;
  uv_cond_signal(&thread->cond_wait_);
  uv_thread_join(&thread->uvthread);
  uv_close((uv_handle_t*) thread->async_signal_, nub__free_handle_cb);
  uv_sem_destroy(&thread->blocker_sem_);
  uv_cond_destroy(&thread->cond_wait_);
  uv_mutex_destroy(&thread->cond_mutex_);
  --thread->nubloop->ref_;
  thread->nubloop = NULL;
}


void nub_thread_enqueue(nub_thread_t* thread, nub_work_t* work) {
  fuq_enqueue(&thread->incoming_, (void*) work);
  uv_cond_signal(&thread->cond_wait_);
}
