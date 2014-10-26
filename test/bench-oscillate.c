#include "nub.h"
#include "run-tests.h"
#include "helper.h"
#include "uv.h"

#define ITER 1e6L

static int iter = ITER;


static void thread_call(nub_thread_t* thread, void* arg) {
  if (0 >= --iter)
    return nub_thread_dispose(thread);

  nub_loop_block(thread);
  nub_thread_push(thread, NULL);
  nub_loop_resume(thread->nubloop);
}


BENCHMARK_IMPL(oscillate) {
  nub_loop_t loop;
  nub_thread_t thread;
  uint64_t time;

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread, thread_call) == 0);

  time = uv_hrtime();

  nub_thread_push(&thread, NULL);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  time = uv_hrtime() - time;
  fprintf(stderr, "back n forths: %Lf/sec\n", ITER / (time / 1e9));

  nub_loop_dispose(&loop);
  iter = ITER;

  return 0;
}


BENCHMARK_IMPL(oscillate2) {
  nub_loop_t loop;
  nub_thread_t thread0;
  nub_thread_t thread1;
  nub_thread_t thread2;
  nub_thread_t thread3;
  nub_thread_t thread4;
  nub_thread_t thread5;
  nub_thread_t thread6;
  nub_thread_t thread7;
  uint64_t time;

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread0, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread1, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread2, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread3, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread4, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread5, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread6, thread_call) == 0);
  ASSERT(nub_thread_create(&loop, &thread7, thread_call) == 0);

  time = uv_hrtime();

  nub_thread_push(&thread0, NULL);
  nub_thread_push(&thread1, NULL);
  nub_thread_push(&thread2, NULL);
  nub_thread_push(&thread3, NULL);
  nub_thread_push(&thread4, NULL);
  nub_thread_push(&thread5, NULL);
  nub_thread_push(&thread6, NULL);
  nub_thread_push(&thread7, NULL);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  time = uv_hrtime() - time;
  fprintf(stderr, "back n forths 2: %Lf/sec\n", ITER / (time / 1e9));

  nub_loop_dispose(&loop);
  iter = ITER;

  return 0;
}
