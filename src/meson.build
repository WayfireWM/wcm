xml = dependency('libxml-2.0', required : true)
gtk = dependency('gtk+-3.0', required : true)
wf_config = dependency('wf-config', version: '>=0.4.0', required : true)

dep_list = [xml, gtk, wf_config, wf_protos, evdev]

sources = files('main.cpp', 'metadata.cpp', 'pages.cpp')

executable(meson.project_name(), sources,
                     install : true,
                     dependencies : dep_list)
