#include "run-tests.h"
#include "uv.h"

int main(int argc, char **argv) {
  argv = uv_setup_args(argc, argv);

  run_test_single_timer_single_thread();
  run_test_multi_timer_single_thread();
  run_test_single_timer_multi_thread();
  run_test_multi_timer_multi_thread();

  return 0;
}
