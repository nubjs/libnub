#include "nub.h"
#include "run-tests.h"
#include "helper.h"
#include "uv.h"

/* uv_timer_t must come first. */
typedef struct {
  uv_timer_t uvtimer;
  nub_work_t work;
  uint64_t timeout;
  uint64_t repeat;
  uint64_t cntr;
  void* data;
} timer_work;


/*** test timer huge timeout ***/

/* Run from main thread. */
static void timer_timeout_thread_disposed_cb(nub_thread_t* thread) {
  void** stor = (void**) thread->data;
  timer_work* tiny_timer = (timer_work*) stor[0];
  timer_work* huge_timer1 = (timer_work*) stor[1];
  timer_work* huge_timer2 = (timer_work*) stor[2];

  uv_close((uv_handle_t*) tiny_timer, NULL);
  uv_close((uv_handle_t*) huge_timer1, NULL);
  uv_close((uv_handle_t*) huge_timer2, NULL);

  thread->data = (void*) 0xf00;
}


/* Run from spawned thread. */
static void timer_timeout_close_cb(nub_thread_t* thread, void* arg) {
  void** stor = (void**) arg;
  timer_work* tiny_timer = (timer_work*) stor[0];
  timer_work* huge_timer1 = (timer_work*) stor[1];
  timer_work* huge_timer2 = (timer_work*) stor[2];

  ASSERT((void*) 0xba4 == tiny_timer->uvtimer.data);
  ASSERT((void*) 0xba4 == huge_timer1->uvtimer.data);
  ASSERT((void*) 0xba4 == huge_timer2->uvtimer.data);

  thread->data = arg;
  nub_thread_dispose(thread, timer_timeout_thread_disposed_cb);
}


/* Run from main thread. */
static void tiny_timer_cb(uv_timer_t* handle) {
}


/* Run from spawned thread. */
static void huge_timer_work_cb(nub_thread_t* thread, void* arg) {
  timer_work* work = (timer_work*) arg;
  work->uvtimer.data = (void*) 0xba4;

  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &work->uvtimer) == 0);
  ASSERT(uv_timer_start(&work->uvtimer,
                        tiny_timer_cb,
                        work->timeout,
                        work->repeat) == 0);

  nub_loop_resume(thread);
}


TEST_IMPL(timer_huge_timeout) {
  nub_loop_t loop;
  nub_thread_t timer_thread;
  timer_work tiny_timer = { .timeout = 1, .repeat = 0 };
  timer_work huge_timer1 = { .timeout = 0xffffffffffffLL, .repeat = 0 };
  timer_work huge_timer2 = { .timeout = (uint64_t) - 1, .repeat = 0 };
  void* stor[3] = { &tiny_timer, &huge_timer1, &huge_timer2 };
  nub_work_t closer = nub_work_init(timer_timeout_close_cb, stor);

  tiny_timer.work = nub_work_init(huge_timer_work_cb, &tiny_timer);
  huge_timer1.work = nub_work_init(huge_timer_work_cb, &huge_timer1);
  huge_timer2.work = nub_work_init(huge_timer_work_cb, &huge_timer2);

  tiny_timer.uvtimer.data = NULL;
  huge_timer1.uvtimer.data = NULL;
  huge_timer2.uvtimer.data = NULL;

  nub_loop_init(&loop);

  /* Spawn the thread that will create the timers. */
  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  /* Push work to the spawned thread. */
  nub_thread_enqueue(&timer_thread, &tiny_timer.work);
  nub_thread_enqueue(&timer_thread, &huge_timer1.work);
  nub_thread_enqueue(&timer_thread, &huge_timer2.work);
  nub_thread_enqueue(&timer_thread, &closer);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  ASSERT(0xf00 == (int) timer_thread.data);

  /* Make valgrind happy. */
  nub_loop_dispose(&loop);

  return 0;
}


/*** test timer huge repeat ***/

/* Run from the main thread. */
static void huge_repeat_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;

  timer->cntr += 1;

  if (((uint64_t) - 1) == timer->repeat)
    return uv_close((uv_handle_t*) timer, NULL);

  if (10 != timer->cntr)
    return;

  uv_close((uv_handle_t*) handle, NULL);
}


/* Run from the spawned thread. */
static void huge_repeat_work_cb(nub_thread_t* thread, void* arg) {
  timer_work* work = (timer_work*) arg;

  /* Event loop critical section. */
  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &work->uvtimer) == 0);
  ASSERT(uv_timer_start(&work->uvtimer,
                        huge_repeat_cb,
                        work->timeout,
                        work->repeat) == 0);

  nub_loop_resume(thread);
}


/* Run from spawned thread. */
static void timer_repeat_close_cb(nub_thread_t* thread, void* arg) {
  thread->data = (void*) 0xf00;
  nub_thread_dispose(thread, NULL);
}


TEST_IMPL(timer_huge_repeat) {
  nub_loop_t loop;
  nub_thread_t timer_thread;
  timer_work tiny_timer = { .timeout = 2, .repeat = 2, .cntr = 0 };
  timer_work huge_timer = { .timeout = 1, .repeat = (uint64_t) - 1, .cntr = 0 };
  nub_work_t closer = nub_work_init(timer_repeat_close_cb, NULL);

  tiny_timer.uvtimer.data = &huge_timer;
  tiny_timer.work = nub_work_init(huge_repeat_work_cb, &tiny_timer);
  tiny_timer.data = &timer_thread;

  huge_timer.uvtimer.data = NULL;
  huge_timer.work = nub_work_init(huge_repeat_work_cb, &huge_timer);
  huge_timer.data = &timer_thread;

  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  nub_thread_enqueue(&timer_thread, &tiny_timer.work);
  nub_thread_enqueue(&timer_thread, &huge_timer.work);
  nub_thread_enqueue(&timer_thread, &closer);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  ASSERT(10 == tiny_timer.cntr);
  ASSERT(1 == huge_timer.cntr);
  ASSERT((void*) 0xf00 == timer_thread.data);

  /* Make valgrind happy. */
  nub_loop_dispose(&loop);

  return 0;
}


/*** test single timer used multiple times ***/

/* Ran from the main thread */
static void timer_run_once_timer_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;
  timer->cntr++;
  uv_close((uv_handle_t*) handle, NULL);
}


/* Ran from the spawned thread. */
static void run_once_work_cb(nub_thread_t* thread, void* arg) {
  timer_work* timer = (timer_work*) arg;
  uv_timer_t* timer_handle = (uv_timer_t*) timer;

  /* Event loop critical section */
  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, timer_handle) == 0);
  ASSERT(uv_timer_start(timer_handle, timer_run_once_timer_cb, 0, 0) == 0);

  nub_loop_resume(thread);

  nub_thread_dispose(thread, NULL);
}


TEST_IMPL(timer_run_once) {
  nub_loop_t loop;
  nub_thread_t timer_thread;
  timer_work timer;

  timer.cntr = 0;
  timer.work = nub_work_init(run_once_work_cb, &timer);
  timer.uvtimer.data = &timer_thread;

  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);

  nub_thread_enqueue(&timer_thread, &timer.work);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(timer.cntr == 1);

  ASSERT(nub_thread_create(&loop, &timer_thread) == 0);
  timer.uvtimer.data = &timer_thread;

  nub_thread_enqueue(&timer_thread, &timer.work);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(timer.cntr == 2);

  /* Make valgrind happy. */
  nub_loop_dispose(&loop);

  return 0;
}


/*** test multiple timers run from multiple threads ***/

TEST_IMPL(timer_run_once_multi) {
  nub_loop_t loop;
  nub_thread_t timer_thread0;
  nub_thread_t timer_thread1;
  timer_work timer0;
  timer_work timer1;

  timer0.cntr = 0;
  timer0.work = nub_work_init(run_once_work_cb, &timer0);
  timer0.uvtimer.data = &timer_thread0;

  timer1.cntr = 0;
  timer1.work = nub_work_init(run_once_work_cb, &timer1);
  timer1.uvtimer.data = &timer_thread1;

  nub_loop_init(&loop);

  ASSERT(nub_thread_create(&loop, &timer_thread0) == 0);
  ASSERT(nub_thread_create(&loop, &timer_thread1) == 0);

  nub_thread_enqueue(&timer_thread0, &timer0.work);
  nub_thread_enqueue(&timer_thread1, &timer1.work);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  ASSERT(timer0.cntr == 1);
  ASSERT(timer1.cntr == 1);

  ASSERT(nub_thread_create(&loop, &timer_thread0) == 0);
  ASSERT(nub_thread_create(&loop, &timer_thread1) == 0);

  nub_thread_enqueue(&timer_thread0, &timer0.work);
  nub_thread_enqueue(&timer_thread1, &timer1.work);

  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);

  ASSERT(timer0.cntr == 2);
  ASSERT(timer1.cntr == 2);

  nub_loop_dispose(&loop);

  return 0;
}
