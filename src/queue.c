#include "nub.h"


void nub_work_init(nub_work_t* work, nub_work_cb cb, void* arg) {
  work->cb = cb;
  work->arg = arg;
  /* Only used for enqueued work for the event loop thread. */
  work->thread = NULL;
  work->complete_cb = NULL;
}
