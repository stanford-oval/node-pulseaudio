{
  'targets': [{
    'target_name': 'pulse',
    'defines': [
      'DIE_ON_EXCEPTION'
    ],
    'sources': [
      'src/context.cc',
      'src/stream.cc',
      'src/uv-mainloop.cc',
      'src/addon.cc'
    ],
    'cflags': [
      '-Wall',
      '-Wno-deprecated-declarations',
      '<!@(pkg-config --cflags-only-other libpulse)'
    ],
    'ldflags': [
      '<!@(pkg-config  --libs-only-L --libs-only-other libpulse)'
    ],
    'libraries': [
      '<!@(pkg-config  --libs-only-l --libs-only-other libpulse)'
    ],
    'include_dirs': [
      "<!(node -e \"require('nan')\")",
      '<!@(pkg-config --cflags-only-I libpulse)'
    ]
  }]
}
