project('ola_video_convert', 'cpp', default_options: ['cpp_std=c++17'])

libavformat = dependency('libavformat', version: '>=58.45.100')
libavcodec = dependency('libavcodec', version: '>=58.91.100')
libavutil = dependency('libavutil', version: '>=56.51.100')
libboost = dependency('boost', version: '>=1.71.0')

cpc = meson.get_compiler('cpp')
cpc.check_header('cxxopts.hpp', required: true)

executable('ola_video_convert', 'ola_video_convert.cpp', 'media.cpp',
           dependencies: [libavcodec, libavformat, libavutil, libboost])
