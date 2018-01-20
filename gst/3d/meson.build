gst3ds_sources = [
  'gst3d.c',
  'gst3dvertexconvert.c',
  'gst3dvertexsrc.c'
]

# graphene_dep = dependency('graphene-1.0', version : '>=1.4.0', required : false)

if gst3d_dep.found() and graphene_dep.found()
  # args3d = []
  # graphene_prefix = graphene_dep.get_pkgconfig_variable('prefix')
  # args3d += ['-DGRAPHENE_PREFIX="' + graphene_prefix + '"']
  # args3d += ['-DHAVE_GRAPHENE=1']

  gst3d = library('gst3d',
    gst3ds_sources,
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
    # c_args : gst_plugins_bad_args + args3d + ['-DGST_USE_UNSTABLE_API'],
    link_args : noseh_link_args,
    include_directories : [configinc, libsinc],
    dependencies : [gstbase_dep, gstvideo_dep, gstgl_dep, gst3d_dep, orc_dep],
    install : true,
    install_dir : plugins_install_dir
  )
endif
