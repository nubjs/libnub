#include "run-benchmarks.h"
#include "uv.h"

int main(int argc, char **argv) {
  argv = uv_setup_args(argc, argv);

  run_bench_oscillate();
  run_bench_oscillate_multi();
  run_bench_enqueue_work();

  return 0;
}
