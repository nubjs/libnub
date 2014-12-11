#include "nub.h"
#include "run-tests.h"
#include "helper.h"
#include "uv.h"

#define ITER 1e6L

static int iter = ITER;


static void thread_call(nub_thread_t* thread, void* arg) {
  if (0 >= --iter)
    return nub_thread_dispose(thread, NULL);

  nub_loop_block(thread);
  nub_thread_push(thread, thread_call, NULL);
  nub_loop_resume(thread);
}


BENCHMARK_IMPL(oscillate) {
  nub_loop_t loop;
  nub_thread_t thread;
  uint64_t time;

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);

  time = uv_hrtime();

  nub_thread_push(&thread, thread_call, NULL);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  time = uv_hrtime() - time;
  fprintf(stderr, "back n forths:   %Lf/sec\n", ITER / (time / 1e9));

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
  ASSERT(nub_thread_create(&loop, &thread0) == 0);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  ASSERT(nub_thread_create(&loop, &thread3) == 0);
  ASSERT(nub_thread_create(&loop, &thread4) == 0);
  ASSERT(nub_thread_create(&loop, &thread5) == 0);
  ASSERT(nub_thread_create(&loop, &thread6) == 0);
  ASSERT(nub_thread_create(&loop, &thread7) == 0);

  time = uv_hrtime();

  nub_thread_push(&thread0, thread_call, NULL);
  nub_thread_push(&thread1, thread_call, NULL);
  nub_thread_push(&thread2, thread_call, NULL);
  nub_thread_push(&thread3, thread_call, NULL);
  nub_thread_push(&thread4, thread_call, NULL);
  nub_thread_push(&thread5, thread_call, NULL);
  nub_thread_push(&thread6, thread_call, NULL);
  nub_thread_push(&thread7, thread_call, NULL);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  time = uv_hrtime() - time;
  fprintf(stderr, "back n forths 2: %Lf/sec\n", ITER / (time / 1e9));

  nub_loop_dispose(&loop);
  iter = ITER;

  return 0;
}
