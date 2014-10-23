#include "nub.h"
#include "run-tests.h"
#include "helper.h"
#include "uv.h"

#define ITER 1e6L

static int iter = ITER;


static void thread_call(nub_thread_t* thread, void* arg) {
  if (0 == --iter)
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

  return 0;
}
