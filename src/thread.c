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
  while (!fuq_empty(&loop->blocking_queue_)) {
    thread = (nub_thread_t*) fuq_dequeue(&loop->blocking_queue_);
    uv_sem_post(&thread->thread_lock_sem_);
    uv_sem_wait(&loop->loop_lock_sem_);
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
      (item->cb)(thread, item, item->arg);
    }
    if (0 < thread->disposed)
      break;
    uv_sem_wait(&thread->sem_wait_);
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

  er = uv_sem_init(&thread->thread_lock_sem_, 0);
  ASSERT(0 == er);

  er = uv_sem_init(&thread->sem_wait_, 1);
  ASSERT(0 == er);

  fuq_init(&thread->incoming_);
  thread->disposed = 0;
  thread->nubloop = loop;
  thread->disposed_cb_ = NULL;
  thread->work.thread = thread;
  thread->work.work_type = NUB_LOOP_QUEUE_NONE;
  ++loop->ref_;

  return uv_thread_create(&thread->uvthread, nub__thread_entry_cb, thread);
}


void nub_thread_dispose(nub_thread_t* thread, nub_thread_disposed_cb cb) {
  thread->disposed = 1;
  thread->disposed_cb_ = cb;
  thread->nubloop->disposed_ = 1;
  uv_mutex_lock(&thread->nubloop->thread_dispose_lock_);
  fuq_enqueue(&thread->nubloop->thread_dispose_queue_, thread);
  uv_mutex_unlock(&thread->nubloop->thread_dispose_lock_);
  uv_async_send(thread->nubloop->work_ping_);
}


void nub_thread_join(nub_thread_t* thread) {
  ASSERT(NULL != thread);
  thread->disposed = 1;
  uv_sem_post(&thread->sem_wait_);
  uv_thread_join(&thread->uvthread);
  uv_close((uv_handle_t*) thread->async_signal_, nub__free_handle_cb);
  uv_sem_destroy(&thread->thread_lock_sem_);
  uv_sem_destroy(&thread->sem_wait_);
  --thread->nubloop->ref_;
  thread->nubloop = NULL;
}


void nub_thread_enqueue(nub_thread_t* thread, nub_work_t* work) {
  fuq_enqueue(&thread->incoming_, (void*) work);
  uv_sem_post(&thread->sem_wait_);
}
