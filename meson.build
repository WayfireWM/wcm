project('wcm', 'cpp', version : '0.1')

dependency('pkg-config', required : true)
xml_dep = dependency('libxml-2.0', required : true)
gtk_dep = dependency('gtk+-3.0', required : true)
wf_dep = dependency('wf-config', required : true)

dep_list = [xml_dep, gtk_dep, wf_dep]

executable(meson.project_name(), 'wayfire-config-manager.cpp',
                     install : true,
                     dependencies : dep_list)
