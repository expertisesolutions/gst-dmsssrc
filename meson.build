project('gstdmss', ['c', 'cpp'], version: '0.1', license: 'LGPL')

add_global_arguments('-DHAVE_CONFIG_H=1', language: ['c', 'cpp'])

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('VERSION', meson.project_version())
config_h.set_quoted('GST_LICENSE', 'LGPL')
config_h.set_quoted('GST_PACKAGE_NAME', meson.project_name())
config_h.set_quoted('GST_PACKAGE_ORIGIN', 'Unknown')

configure_file(
  output: 'config.h',
  configuration: config_h,
)

gst_dep = dependency('gstreamer-1.0')
gstapp_dep = dependency('gstreamer-app-1.0')
gstnet_dep = dependency('gstreamer-net-1.0')

dependencies = [
  gst_dep,
  gstapp_dep,
  gstnet_dep,
]

sources = files(
  'src/gstdmssdemux.c',
  'src/gstdmssprotocol.c',
  'src/gstdmsssrc.c',
  'src/plugin.c',
)

include_dirs = [
  '.',
  './src',
]

gst_dmss = library(meson.project_name(),
  sources, 
  include_directories: include_dirs,
  dependencies: dependencies,
  install: true,
  install_dir: plugins_install_dir,
)

