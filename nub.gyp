{
  'includes': [
    'common.gypi',
  ],

  'targets': [
    {
      'target_name': 'libnub',
      'type': 'static_library',
      'dependencies': [ 'deps/uv/uv.gyp:libuv' ],
      'include_dirs': [
        'include',
        'src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
      },
      'sources': [
        'include/nub.h',
        'src/loop.c',
        'src/thread.c',
        'src/fuq.h',
        'src/util.h',
      ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [
            '-g',
            '--std=gnu89',
            '-pedantic',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter',
          ],
        }]
      ],
    },

    {
      'target_name': 'run-nub-tests',
      'type': 'executable',
      'dependencies': [
        'deps/uv/uv.gyp:libuv',
        'libnub',
      ],
      'sources': [
        'test/helper.h',
        'test/run-tests.c',
        'test/run-tests.h',
        'test/test-timers.c',
      ],
    },

    {
      'target_name': 'run-nub-benchmarks',
      'type': 'executable',
      'dependencies': [
        'deps/uv/uv.gyp:libuv',
        'libnub',
      ],
      'sources': [
        'test/helper.h',
        'test/run-benchmarks.c',
        'test/run-benchmarks.h',
        'test/bench-oscillate.c',
      ],
    },
  ],
}
