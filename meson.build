project('wcm', 'cpp', version : '0.1')

share_dir = join_paths(get_option('prefix'), 'share', 'wayfire')
metadata_dir = join_paths(share_dir, 'metadata')
icon_dir = join_paths(share_dir, 'icons')
add_global_arguments('-DMETADATADIR="' + metadata_dir + '"', language : 'cpp')
add_global_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

dependency('pkg-config', required : true)
xml_dep = dependency('libxml-2.0', required : true)
gtk_dep = dependency('gtk+-3.0', required : true)
wf_dep = dependency('wf-config', required : true)

dep_list = [xml_dep, gtk_dep, wf_dep]

executable(meson.project_name(), 'wayfire-config-manager.cpp',
                     install : true,
                     dependencies : dep_list)

subdir('metadata')
subdir('icons')
