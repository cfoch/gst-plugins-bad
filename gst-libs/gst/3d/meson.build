gst3d_sources = [
  'gst3dcamera.c',
  'gst3dmaterialinfo.c',
  'gst3dmaterialmeta.c',
  'gst3dmodelmeta.c',
  'gst3dvertexconverter.c',
  'gst3dvertexformat.c',
  'gst3dvertexinfo.c',
  'gst3dvertexmeta.c'
]

gst3d_headers = [
  '3d.h',
  'gst3dcamera.h',
  'gst3dmaterialinfo.h',
  'gst3dmaterialmeta.h',
  'gst3dmodelmeta.h',
  'gst3dvertexconverter.h',
  'gst3dvertexformat.h',
  'gst3dvertexinfo.h',
  'gst3dvertexmeta.h'
]

install_headers(gst3d_headers, subdir : 'gstreamer-1.0/gst/3d')
graphene_dep = dependency('graphene-1.0', version : '>=1.4.0', required : false)


if graphene_dep.found() and gstgl_dep.found()
  orcsrc = 'gst3dvertexpack'
  if have_orcc
    orc_h = custom_target(orcsrc + '.h',
      input : orcsrc + '.orc',
      output : orcsrc + '.h',
      command : orcc_args + ['--header', '-o', '@OUTPUT@', '@INPUT@'])
    orc_c = custom_target(orcsrc + '.c',
      input : orcsrc + '.orc',
      output : orcsrc + '.c',
      command : orcc_args + ['--implementation', '-o', '@OUTPUT@', '@INPUT@'])
  else
    orc_h = configure_file(input : orcsrc + '-dist.h',
      output : orcsrc + '.h',
      configuration : configuration_data())
    orc_c = configure_file(input : orcsrc + '-dist.c',
      output : orcsrc + '.c',
      configuration : configuration_data())
  endif

  gst3d_args = []
  graphene_prefix = graphene_dep.get_pkgconfig_variable('prefix')
  gst3d_args += ['-DGRAPHENE_PREFIX="' + graphene_prefix + '"']
  gst3d_args += ['-DHAVE_GRAPHENE=1']


  gst3dlib = library('gst3d-' + api_version,
    gst3d_sources, orc_c, orc_h,
    c_args : gst_plugins_bad_args + gst3d_args + ['-DGST_USE_UNSTABLE_API'],
    include_directories : [configinc, libsinc],
    version : libversion,
    soversion : soversion,
    install : true,
    dependencies : [gstbase_dep, gstvideo_dep, gstgl_dep, graphene_dep, orc_dep]
  )

  gst3d_dep = declare_dependency(link_with : gst3dlib,
    include_directories : [libsinc],
    dependencies : [gstvideo_dep, gstgl_dep, graphene_dep, orc_dep])

endif
