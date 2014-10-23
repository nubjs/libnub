#include "run-tests.h"
#include "uv.h"

int main(int argc, char **argv) {
  argv = uv_setup_args(argc, argv);

  run_test_timer_huge_timeout();
  run_test_timer_huge_repeat();
  run_test_timer_run_once();
  run_test_timer_run_once_multi();

  return 0;
}
