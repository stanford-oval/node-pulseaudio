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
      '<!@(pkg-config --cflags libpulse)'
    ],
    'xcode_settings': {
      'OTHER_CFLAGS': [
        '-Wall',
        '-Wno-deprecated-declarations',
        '<!@(pkg-config --cflags libpulse)'
      ]
    },
    'libraries': [
      '<!@(pkg-config  --libs libpulse)'
    ],
    'include_dirs': [
      "<!(node -e \"require('nan')\")",
    ]
  }]
}
