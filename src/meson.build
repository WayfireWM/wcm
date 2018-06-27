xml_dep = dependency('libxml-2.0', required : true)
gtk_dep = dependency('gtk+-3.0', required : true)
wf_dep = dependency('wf-config', required : true)

dep_list = [xml_dep, gtk_dep, wf_dep]

sources = files('main.cpp', 'metadata.cpp', 'pages.cpp')

executable(meson.project_name(), sources,
                     install : true,
                     dependencies : dep_list)
