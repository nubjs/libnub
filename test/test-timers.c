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
  uint64_t* shared_cntr;
  void* data;
  uv_timer_cb t_cb;
} timer_work;


/* Runs from the spawned thread. */
static void create_timer_cb(nub_thread_t* thread, void* arg) {
  timer_work* t_work = (timer_work*) arg;

  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &t_work->uvtimer) == 0);
  ASSERT(uv_timer_start(&t_work->uvtimer,
                        t_work->t_cb,
                        t_work->timeout,
                        t_work->repeat) == 0);

  nub_loop_resume(thread);
}


/* Runs from the spawned thread. */
static void create_timer_dispose_cb(nub_thread_t* thread, void* arg) {
  timer_work* t_work = (timer_work*) arg;

  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &t_work->uvtimer) == 0);
  ASSERT(uv_timer_start(&t_work->uvtimer,
                        t_work->t_cb,
                        t_work->timeout,
                        t_work->repeat) == 0);

  nub_loop_resume(thread);
  nub_thread_dispose(thread, NULL);
}


/*** Test creating single timer from a single thread ***/

/* Runs from the main thread. */
static void single_timer_join_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;

  timer->cntr += 1;
  uv_close((uv_handle_t*) timer, NULL);
  nub_thread_join((nub_thread_t*) timer->data);
}


/* Runs from the main thread. */
static void single_timer_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;

  timer->cntr += 1;
  uv_close((uv_handle_t*) timer, NULL);
}


TEST_IMPL(single_timer_single_thread) {
  nub_loop_t loop;
  nub_thread_t thread;
  timer_work timer;

  timer.timeout = 1;
  timer.repeat = 0;
  timer.cntr = 0;
  timer.data = &thread;
  timer.t_cb = single_timer_join_cb;
  nub_work_init(&timer.work, create_timer_cb, &timer);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(1 == timer.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(2 == timer.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(3 == timer.cntr);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(4 == timer.cntr);
  nub_loop_dispose(&loop);

  /* Repeat tests, but using nub_thread_dispose() instead. */
  timer.t_cb = single_timer_cb;
  nub_work_init(&timer.work, create_timer_dispose_cb, &timer);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(5 == timer.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(6 == timer.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(7 == timer.cntr);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(8 == timer.cntr);
  nub_loop_dispose(&loop);

  return 0;
}


/*** Test creating multiple timers from a single thread ***/

/* Runs from the main thread. */
static void multi_timer_cb(uv_timer_t* handle) {
  static int cntr = 0;
  timer_work* timer = (timer_work*) handle;

  cntr += 1;
  timer->cntr += 1;
  uv_close((uv_handle_t*) timer, NULL);

  /* If cntr is even, then both timers have run so join the thread. */
  if (0 == cntr % 2)
    nub_thread_join((nub_thread_t*) timer->data);
}


/* Runs from the spawned thread. */
static void thread_multi_timer_dispose_cb(nub_thread_t* thread, void* arg) {
  static int cntr = 0;
  timer_work* t_work = (timer_work*) arg;

  cntr += 1;

  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &t_work->uvtimer) == 0);
  ASSERT(uv_timer_start(&t_work->uvtimer,
                        single_timer_cb,
                        t_work->timeout,
                        t_work->repeat) == 0);

  nub_loop_resume(thread);

  if (0 == cntr % 2)
    nub_thread_dispose(thread, NULL);
}


TEST_IMPL(multi_timer_single_thread) {
  nub_loop_t loop;
  nub_thread_t thread;
  timer_work timer1;
  timer_work timer2;

  timer1.timeout = 1;
  timer1.repeat = 0;
  timer1.cntr = 0;
  timer1.data = &thread;
  timer1.t_cb = multi_timer_cb;
  nub_work_init(&timer1.work, create_timer_cb, &timer1);

  timer2.timeout = 1;
  timer2.repeat = 0;
  timer2.cntr = 0;
  timer2.data = &thread;
  timer2.t_cb = multi_timer_cb;
  nub_work_init(&timer2.work, create_timer_cb, &timer2);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(1 == timer1.cntr);
  ASSERT(1 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(2 == timer1.cntr);
  ASSERT(2 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(3 == timer1.cntr);
  ASSERT(3 == timer2.cntr);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(4 == timer1.cntr);
  ASSERT(4 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Repeat tests, but use nub_thread_dispose() instead. */
  nub_work_init(&timer1.work, thread_multi_timer_dispose_cb, &timer1);
  nub_work_init(&timer2.work, thread_multi_timer_dispose_cb, &timer2);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(5 == timer1.cntr);
  ASSERT(5 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(6 == timer1.cntr);
  ASSERT(6 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(7 == timer1.cntr);
  ASSERT(7 == timer2.cntr);
  ASSERT(nub_thread_create(&loop, &thread) == 0);
  nub_thread_enqueue(&thread, &timer1.work);
  nub_thread_enqueue(&thread, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(8 == timer1.cntr);
  ASSERT(8 == timer2.cntr);
  nub_loop_dispose(&loop);

  return 0;
}


/*** Test creating single timer from mutiple threads ***/

TEST_IMPL(single_timer_multi_thread) {
  nub_loop_t loop;
  nub_thread_t thread1;
  nub_thread_t thread2;
  timer_work timer1;
  timer_work timer2;

  timer1.timeout = 1;
  timer1.repeat = 0;
  timer1.cntr = 0;
  timer1.data = &thread1;
  timer1.t_cb = single_timer_join_cb;
  nub_work_init(&timer1.work, create_timer_cb, &timer1);

  timer2.timeout = 1;
  timer2.repeat = 0;
  timer2.cntr = 0;
  timer2.data = &thread2;
  timer2.t_cb = single_timer_join_cb;
  nub_work_init(&timer2.work, create_timer_cb, &timer2);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(1 == timer1.cntr);
  ASSERT(1 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(2 == timer1.cntr);
  ASSERT(2 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(3 == timer1.cntr);
  ASSERT(3 == timer2.cntr);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(4 == timer1.cntr);
  ASSERT(4 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Repeat tests, but using nub_thread_dispose() instead. */
  timer1.t_cb = single_timer_cb;
  nub_work_init(&timer1.work, create_timer_dispose_cb, &timer1);
  timer2.t_cb = single_timer_cb;
  nub_work_init(&timer2.work, create_timer_dispose_cb, &timer2);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(5 == timer1.cntr);
  ASSERT(5 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(6 == timer1.cntr);
  ASSERT(6 == timer2.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(7 == timer1.cntr);
  ASSERT(7 == timer2.cntr);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread2, &timer2.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(8 == timer1.cntr);
  ASSERT(8 == timer2.cntr);
  nub_loop_dispose(&loop);

  return 0;
}


/*** Test creating multiple timers from multiple threads ***/

static void multi_timer_multi_thread_cb(uv_timer_t* handle) {
  timer_work* timer = (timer_work*) handle;

  *timer->shared_cntr += 1;
  timer->cntr += 1;
  uv_close((uv_handle_t*) timer, NULL);

  /* If shared_cntr is even then both timers have run so join the thread. */
  if (0 == *timer->shared_cntr % 2)
    nub_thread_join((nub_thread_t*) timer->data);
}


static void multi_all_dispose_cb(nub_thread_t* thread, void* arg) {
  timer_work* t_work = (timer_work*) arg;

  *t_work->shared_cntr += 1;

  nub_loop_block(thread);

  ASSERT(uv_timer_init(&thread->nubloop->uvloop, &t_work->uvtimer) == 0);
  ASSERT(uv_timer_start(&t_work->uvtimer,
                        single_timer_cb,
                        t_work->timeout,
                        t_work->repeat) == 0);

  nub_loop_resume(thread);

  if (0 == *t_work->shared_cntr % 2)
    nub_thread_dispose(thread, NULL);
}


TEST_IMPL(multi_timer_multi_thread) {
  nub_loop_t loop;
  nub_thread_t thread1;
  nub_thread_t thread2;
  timer_work timer1;
  timer_work timer2;
  timer_work timer3;
  timer_work timer4;
  uint64_t cntr1_ptr;
  uint64_t cntr2_ptr;

  cntr1_ptr = 0;
  cntr2_ptr = 0;

  timer1.timeout = 1;
  timer1.repeat = 0;
  timer1.cntr = 0;
  timer1.shared_cntr = &cntr1_ptr;
  timer1.data = &thread1;
  timer1.t_cb = multi_timer_multi_thread_cb;
  nub_work_init(&timer1.work, create_timer_cb, &timer1);

  timer2.timeout = 1;
  timer2.repeat = 0;
  timer2.cntr = 0;
  timer2.shared_cntr = &cntr1_ptr;
  timer2.data = &thread1;
  timer2.t_cb = multi_timer_multi_thread_cb;
  nub_work_init(&timer2.work, create_timer_cb, &timer2);

  timer3.timeout = 1;
  timer3.repeat = 0;
  timer3.cntr = 0;
  timer3.shared_cntr = &cntr2_ptr;
  timer3.data = &thread2;
  timer3.t_cb = multi_timer_multi_thread_cb;
  nub_work_init(&timer3.work, create_timer_cb, &timer3);

  timer4.timeout = 1;
  timer4.repeat = 0;
  timer4.cntr = 0;
  timer4.shared_cntr = &cntr2_ptr;
  timer4.data = &thread2;
  timer4.t_cb = multi_timer_multi_thread_cb;
  nub_work_init(&timer4.work, create_timer_cb, &timer4);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(1 == timer1.cntr);
  ASSERT(1 == timer2.cntr);
  ASSERT(1 == timer3.cntr);
  ASSERT(1 == timer4.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(2 == timer1.cntr);
  ASSERT(2 == timer2.cntr);
  ASSERT(2 == timer3.cntr);
  ASSERT(2 == timer4.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(3 == timer1.cntr);
  ASSERT(3 == timer2.cntr);
  ASSERT(3 == timer3.cntr);
  ASSERT(3 == timer4.cntr);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(4 == timer1.cntr);
  ASSERT(4 == timer2.cntr);
  ASSERT(4 == timer3.cntr);
  ASSERT(4 == timer4.cntr);
  nub_loop_dispose(&loop);

  /* Repeat tests, but use nub_thread_dispose() instead. */
  nub_work_init(&timer1.work, multi_all_dispose_cb, &timer1);
  nub_work_init(&timer2.work, multi_all_dispose_cb, &timer2);
  nub_work_init(&timer3.work, multi_all_dispose_cb, &timer3);
  nub_work_init(&timer4.work, multi_all_dispose_cb, &timer4);

  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(5 == timer1.cntr);
  ASSERT(5 == timer2.cntr);
  ASSERT(5 == timer3.cntr);
  ASSERT(5 == timer4.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same resources. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(6 == timer1.cntr);
  ASSERT(6 == timer2.cntr);
  ASSERT(6 == timer3.cntr);
  ASSERT(6 == timer4.cntr);
  nub_loop_dispose(&loop);

  /* Re-run using the same loop. */
  nub_loop_init(&loop);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(7 == timer1.cntr);
  ASSERT(7 == timer2.cntr);
  ASSERT(7 == timer3.cntr);
  ASSERT(7 == timer4.cntr);
  ASSERT(nub_thread_create(&loop, &thread1) == 0);
  ASSERT(nub_thread_create(&loop, &thread2) == 0);
  nub_thread_enqueue(&thread1, &timer1.work);
  nub_thread_enqueue(&thread1, &timer2.work);
  nub_thread_enqueue(&thread2, &timer3.work);
  nub_thread_enqueue(&thread2, &timer4.work);
  ASSERT(nub_loop_run(&loop, UV_RUN_DEFAULT) == 0);
  ASSERT(8 == timer1.cntr);
  ASSERT(8 == timer2.cntr);
  ASSERT(8 == timer3.cntr);
  ASSERT(8 == timer4.cntr);
  nub_loop_dispose(&loop);

  return 0;
}
