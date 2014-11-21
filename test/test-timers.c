#include "nub.h"
#include "run-tests.h"
#include "helper.h"
#include "uv.h"

typedef struct {
  uv_timer_t timer;
  uint64_t timeout;
  uint64_t repeat;
  uint64_t cntr;
} timer_work;


static void tiny_timer_cb(uv_timer_t* handle) {
  ASSERT(NULL != handle->data);

  void** timers = (void**) handle->data;
  if (uv_has_ref((uv_handle_t*) timers[0]) == 0 ||
      uv_has_ref((uv_handle_t*) timers[1]) == 0) {
    ASSERT(uv_timer_start(handle, tiny_timer_cb, 1, 0) == 0);
    return;
  }

  uv_close((uv_handle_t*) handle, NULL);
  uv_close((uv_handle_t*) timers[0], NULL);
  uv_close((uv_handle_t*) timers[1], NULL);
}


static void tiny_timer_work_cb(nub_thread_t* thread, void* arg) {
  timer_work* work = (timer_work*) arg;

  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &work->timer) == 0);
  ASSERT(uv_timer_start(&work->timer,
                        tiny_timer_cb,
                        work->timeout,
                        work->repeat) == 0);

  nub_loop_resume(thread);
  if ((uint64_t) - 1 == work->timeout)
    nub_thread_dispose(thread);
}


TEST_IMPL(timer_huge_timeout) {
  nub_loop_t loop;
  nub_thread_t timer_thread;
  timer_work tiny_timer = { .timeout = 1, .repeat = 0 };
  timer_work huge_timer1 = { .timeout = 0xffffffffffffLL, .repeat = 0 };
  timer_work huge_timer2 = { .timeout = (uint64_t) - 1, .repeat = 0 };
  void* timers[2] = { &huge_timer1, &huge_timer2 };

  tiny_timer.timer.data = timers;

  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  /* Push work to the spawned thread. */
  nub_thread_push(&timer_thread, tiny_timer_work_cb, (void*) &tiny_timer);
  nub_thread_push(&timer_thread, tiny_timer_work_cb, (void*) &huge_timer1);
  nub_thread_push(&timer_thread, tiny_timer_work_cb, (void*) &huge_timer2);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  /* Make valgrind happy. */
  nub_loop_dispose(&loop);

  return 0;
}


/* Run from the main thread. */
static void huge_repeat_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;

  if (10 != ++timer->cntr)
    return;

  ASSERT(NULL != handle->data);
  uv_close((uv_handle_t*) handle, NULL);
  uv_close((uv_handle_t*) handle->data, NULL);
}


/* Run from the spawned thread. */
static void huge_repeat_work_cb(nub_thread_t* thread, void* arg) {
  timer_work* work = (timer_work*) arg;

  /* Event loop critical section. */
  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &work->timer) == 0);
  ASSERT(uv_timer_start(&work->timer,
                        huge_repeat_cb,
                        work->timeout,
                        work->repeat) == 0);

  nub_loop_resume(thread);
  if ((uint64_t) - 1 == work->repeat)
    nub_thread_dispose(thread);
}


TEST_IMPL(timer_huge_repeat) {
  nub_loop_t loop;
  nub_thread_t timer_thread;
  timer_work tiny_timer = { .timeout = 2, .repeat = 2, .cntr = 0 };
  timer_work huge_timer = { .timeout = 1, .repeat = (uint64_t) - 1, .cntr = 0 };

  tiny_timer.timer.data = &huge_timer;
  huge_timer.timer.data = NULL;

  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  /* Push work to the spawned thread. */
  nub_thread_push(&timer_thread, huge_repeat_work_cb, &tiny_timer);
  nub_thread_push(&timer_thread, huge_repeat_work_cb, &huge_timer);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(tiny_timer.cntr == 10);
  ASSERT(huge_timer.cntr == 1);

  /* Make valgrind happy. */
  nub_loop_dispose(&loop);

  return 0;
}


/* Ran from the main thread */
static void timer_run_once_timer_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;
  timer->cntr++;
  uv_close((uv_handle_t*) handle, NULL);
}


/* Ran from the spawned thread. */
static void run_once_work_cb(nub_thread_t* thread, void* arg) {
  timer_work* work = (timer_work*) arg;
  uv_timer_t* timer_handle = (uv_timer_t*) work;

  /* Event loop critical section */
  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, timer_handle) == 0);
  ASSERT(uv_timer_start(timer_handle, timer_run_once_timer_cb, 0, 0) == 0);

  nub_loop_resume(thread);
  nub_thread_dispose(thread);
}


TEST_IMPL(timer_run_once) {
  nub_loop_t loop;
  nub_thread_t timer_thread;
  timer_work timer;

  timer.cntr = 0;
  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  /* Push work to the spawned thread. */
  nub_thread_push(&timer_thread, run_once_work_cb, &timer);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(timer.cntr == 1);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  nub_thread_push(&timer_thread, run_once_work_cb, &timer);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(timer.cntr == 2);

  /* Make valgrind happy. */
  nub_loop_dispose(&loop);

  return 0;
}


TEST_IMPL(timer_run_once_multi) {
  nub_loop_t loop;
  nub_thread_t timer_thread0;
  nub_thread_t timer_thread1;
  timer_work timer0;
  timer_work timer1;

  timer0.cntr = 0;
  timer1.cntr = 0;
  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread0) == 0);
  ASSERT(nub_thread_create(&loop, &timer_thread1) == 0);

  nub_thread_push(&timer_thread0, run_once_work_cb, &timer0);
  nub_thread_push(&timer_thread1, run_once_work_cb, &timer1);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(timer0.cntr == 1);
  ASSERT(timer1.cntr == 1);

  nub_loop_dispose(&loop);

  return 0;
}
