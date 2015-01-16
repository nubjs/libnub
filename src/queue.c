#include "nub.h"


void nub_work_init(nub_work_t* work, nub_work_cb cb, void* arg) {
  work->cb = cb;
  work->arg = arg;
}
