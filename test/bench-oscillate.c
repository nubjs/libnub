#include "nub.h"
#include "run-tests.h"
#include "helper.h"
#include "uv.h"

#define ITER 1e6L

static int iter;


static void thread_call(nub_thread_t* thread, nub_work_t* work, void* arg) {
  if (0 >= --iter)
    return nub_thread_dispose(thread, NULL);

  /* Block event loop long enough to queue up running this function again. */
  nub_loop_lock(thread);
  nub_thread_enqueue(thread, (nub_work_t*) arg);
  nub_loop_unlock(thread);
}


BENCHMARK_IMPL(oscillate) {
  nub_loop_t loop;
  nub_thread_t thread;
  nub_work_t work;
  uint64_t time;

  iter = ITER;
  nub_work_init(&work, thread_call, &work);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);

  time = uv_hrtime();

  nub_thread_enqueue(&thread, &work);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  time = uv_hrtime() - time;
  fprintf(stderr, "oscillate: %Lf/sec\n", ITER / (time / 1e9));

  nub_loop_dispose(&loop);

  return 0;
}


BENCHMARK_IMPL(oscillate_multi) {
  nub_loop_t loop;
  nub_thread_t thread0;
  nub_thread_t thread1;
  nub_thread_t thread2;
  nub_thread_t thread3;
  nub_thread_t thread4;
  nub_thread_t thread5;
  nub_thread_t thread6;
  nub_thread_t thread7;
  nub_work_t work;
  uint64_t time;

  iter = ITER;
  nub_work_init(&work, thread_call, &work);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread0) == 0);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  ASSERT(nub_thread_create(&loop, &thread3) == 0);
  ASSERT(nub_thread_create(&loop, &thread4) == 0);
  ASSERT(nub_thread_create(&loop, &thread5) == 0);
  ASSERT(nub_thread_create(&loop, &thread6) == 0);
  ASSERT(nub_thread_create(&loop, &thread7) == 0);

  time = uv_hrtime();

  nub_thread_enqueue(&thread0, &work);
  nub_thread_enqueue(&thread1, &work);
  nub_thread_enqueue(&thread2, &work);
  nub_thread_enqueue(&thread3, &work);
  nub_thread_enqueue(&thread4, &work);
  nub_thread_enqueue(&thread5, &work);
  nub_thread_enqueue(&thread6, &work);
  nub_thread_enqueue(&thread7, &work);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  time = uv_hrtime() - time;
  fprintf(stderr, "oscillate_multi: %Lf/sec\n", ITER / (time / 1e9));

  nub_loop_dispose(&loop);
  iter = ITER;

  return 0;
}


void enqueue_noop(nub_thread_t* thread, nub_work_t* work, void* arg) {
  intptr_t d = (intptr_t) thread->data;
  d++;
  thread->data = (void*) d;
}


void enqueue_dispose(nub_thread_t* thread, nub_work_t* work, void* arg) {
  nub_thread_dispose(thread, NULL);
}


BENCHMARK_IMPL(enqueue_work) {
  nub_loop_t loop;
  nub_thread_t thread;
  nub_work_t work_noop;
  nub_work_t work_dispose;
  uint64_t time;

  iter = ITER;

  nub_work_init(&work_noop, enqueue_noop, &work_noop);
  nub_work_init(&work_dispose, enqueue_dispose, &work_dispose);
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);

  thread.data = 0x0;

  time = uv_hrtime();

  for (size_t i = 0; i < ITER; i++) {
    nub_thread_enqueue(&thread, &work_noop);
  }
  nub_thread_enqueue(&thread, &work_dispose);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(ITER == (intptr_t) thread.data);

  time = uv_hrtime() - time;
  fprintf(stderr, "enqueue_work:  %Lf/sec\n", ITER / (time / 1e9));

  nub_loop_dispose(&loop);
  iter = ITER;

  return 0;
}
