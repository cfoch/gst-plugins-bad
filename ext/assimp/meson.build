assimp_sources = [
  'gstassimp.c',
]

assimp_dep = dependency('assimp', required : false)
# gst3d_dep = dependency('gst3d', required : false)

if assimp_dep.found() and gst3d_dep.found()
  gstassimp = library('gstassimp',
    assimp_sources,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc, libsinc],
    dependencies : [assimp_dep, gstgl_dep, gstvideo_dep, gstbase_dep, gst3d_dep, gio_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
endif
