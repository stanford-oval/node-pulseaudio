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
    'conditions': [
      ['OS=="linux"', {
        'defines': [
        ],
        'cflags': [
          '-Wall',
          '<!@(pkg-config --cflags libpulse)'
        ],
        'ldflags': [
          '<!@(pkg-config  --libs-only-L --libs-only-other libpulse)'
        ],
        'libraries': [
          '<!@(pkg-config  --libs-only-l --libs-only-other libpulse)'
        ]
      }]
    ]
  }]
}
