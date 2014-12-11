#include "nub.h"
#include "fuq.h"
#include "util.h"
#include "uv.h"

#include <stdlib.h>  /* malloc, free */


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
  void** item;

  thread = (nub_thread_t*) arg;
  queue = &thread->incoming_;

  for (;;) {
    while (!fuq_empty(queue)) {
      item = fuq_dequeue(queue);
      ((nub_thread_work_cb) item[0])(thread, item[1]);
      free(item);
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


void nub_thread_push(nub_thread_t* thread,
                     nub_thread_work_cb work_cb,
                     void* arg) {
  void** args;
  args = malloc(sizeof(*args) * 2);
  ASSERT(NULL != args);
  args[0] = (void*) work_cb;
  args[1] = arg;
  fuq_enqueue(&thread->incoming_, args);
  uv_cond_signal(&thread->cond_wait_);
}
