{
  'target_defaults': {
    'default_configuration': 'Release',
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG' ],
        'cflags': [ '-g', '-O0', '-fwrapv' ],
      },
      'Release': {
        'cflags': [
          '-O3',
          '-fstrict-aliasing',
          '-fomit-frame-pointer',
          '-fdata-sections',
          '-ffunction-sections',
        ],
      },
    },

    'conditions': [
      ['OS in "freebsd linux openbsd solaris android"', {
        'cflags': [ '-Wall' ],
        'cflags_cc': [ '-fno-rtti', '-fno-exceptions' ],
        'target_conditions': [
          ['_type=="static_library"', {
            'standalone_static_library': 1,
          }],
        ],
      }],
    ],
  },
}
