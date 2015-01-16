#ifndef LIBNUB_NUB_H_
#define LIBNUB_NUB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "uv.h"
#include "fuq.h"

#ifdef _WIN32
  /* Windows - set up dll import/export decorators. */
# if defined(NUB_BUILDING_SHARED)
    /* Building shared library. */
#   define NUB_EXTERN __declspec(dllexport)
# elif defined(NUB_USING_SHARED)
    /* Using shared library. */
#   define NUB_EXTERN __declspec(dllimport)
# else
    /* Building static library. */
#   define NUB_EXTERN /* nothing */
# endif
#elif __GNUC__ >= 4
# define NUB_EXTERN __attribute__((visibility("default")))
#else
# define NUB_EXTERN /* nothing */
#endif


typedef struct nub_loop_s nub_loop_t;
typedef struct nub_thread_s nub_thread_t;
typedef struct nub_work_s nub_work_t;

typedef void (*nub_loop_cb)(void* arg);
typedef void (*nub_thread_disposed_cb)(nub_thread_t* thread);
typedef void (*nub_work_cb)(nub_thread_t* thread, void* arg);


struct nub_loop_s {
  /* public */
  void* data;  /* User storage */

  /* read-only */
  uv_loop_t uvloop;

  /* private */
  uv_prepare_t async_runner_;   /* Runs async queue before idling */
  uv_mutex_t async_mutex_;      /* Threads will need to use a mutex to add
                                   items to the queue */
  fuq_queue_t async_queue_;     /* Holds entries from loop_block to run in the
                                   prepare cb */
  uv_sem_t blocker_sem_;        /* Block while thread operates on the eloop */
  volatile unsigned int ref_;   /* Nuber of threads attached to this loop */
  uv_async_t* thread_dispose_;  /* Used to dispose of threads */
  fuq_queue_t thread_dispose_queue_;
  uv_mutex_t thread_dispose_lock_;
};


struct nub_thread_s {
  /* public */
  void* data;

  /* read-only */
  nub_loop_t* nubloop;
  uv_thread_t uvthread;
  volatile int disposed;

  /* private */
  fuq_queue_t incoming_;
  uv_sem_t blocker_sem_;
  /* Must be separately allocated so the handle can be closed after the thread
   * is gone. Used in an internal uv_async_send() call to signal the event loop
   * a thread has work to do. */
  uv_async_t* async_signal_;
  uv_sem_t sem_wait_;
  volatile int processing_;
  nub_thread_disposed_cb disposed_cb_;
};


/* Will be used bi-directionally once the event loop accepts dispatch queues. */
struct nub_work_s {
  void* arg;
  nub_work_cb cb;
};


/**
 * Initialize the event loop.
 */
NUB_EXTERN void nub_loop_init(nub_loop_t* loop);


/**
 * Run the event loop.
 *
 * Return value is the same as uv_run();
 */
NUB_EXTERN int nub_loop_run(nub_loop_t* loop, uv_run_mode mode);


/**
 * Cleanup nub_loop_t resources. This does not cleanup resources attached to
 * the loop. Such as spawned threads. That should be done by the user previous
 * to running this function. Should only be run after uv_run() has returned
 * and the loop is no longer alive.
 */
NUB_EXTERN void nub_loop_dispose(nub_loop_t* loop);


/**
 * Blocks until the event loop has been halted and is ready for the worker
 * thread to perform event loop specific operations.
 *
 * Useful for cases where the return values of specific calls are needed
 * synchronously (e.g. creating a handle in libuv).
 *
 * The nub_thread_t passed must be the same as the calling thread.
 *
 * Return value is the same as uv_async_send().
 */
NUB_EXTERN int nub_loop_block(nub_thread_t* thread);


/**
 * Tell the event loop to resume execution as normal.
 *
 * Should be run at the end of any event loop critical section.
 */
NUB_EXTERN void nub_loop_resume(nub_thread_t* thread);


/**
 * Spawn a new thread and attach it to the passed event loop.
 *
 * The passed nub_thread_t should be previously uninitialized. Though the
 * "data" field on the nub_thread_t can be set.
 *
 * Return value is the same as uv_thread_create().
 */
NUB_EXTERN int nub_thread_create(nub_loop_t* loop, nub_thread_t* thread);


/**
 * Dispose of a thread. Call from the thread that needs to be disposed. Should
 * be the last call from that thread. This will make an internal call to end
 * the thread so it can be joined from the nub_loop_t thread. This action is
 * transparent to the thread that's being disposed.
 *
 * cb can be NULL.
 *
 * Make sure not to run until all resources on the spawned thread have been
 * cleaned up.
 */
NUB_EXTERN void nub_thread_dispose(nub_thread_t* thread,
                                   nub_thread_disposed_cb cb);


/**
 * Join a spawned thread and wait for it to be brought down. Must be run from
 * event loop thread.
 *
 * Since nub_thread_create() doesn't require the user to keep the thread alive,
 * nub_thread_join() differs from pthread_join() by simply telling the thread
 * to finish processing all work in its queue then to bring itself down.
 */
NUB_EXTERN void nub_thread_join(nub_thread_t* thread);


/**
 * Push data onto the processing queue. Should only be run from the nub_loop_t
 * thread. This will signal the spawned thread there is an item on the queue.
 */
NUB_EXTERN void nub_thread_enqueue(nub_thread_t* thread, nub_work_t* work);


/**
 * Create a unit of work to be dispached out to the thread's processing queue.
 *
 * In the future nub_work_t will also be used for event loop dispatch queues.
 */
NUB_EXTERN void nub_work_init(nub_work_t* work, nub_work_cb cb, void* arg);

#ifdef __cplusplus
}
#endif
#endif  /* LIBNUB_NUB_H_ */
