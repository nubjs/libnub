#include "nub.h"


nub_work_t nub_work_init(nub_work_cb cb, void* arg) {
  nub_work_t work;
  work.cb = cb;
  work.arg = arg;
  return work;
}
