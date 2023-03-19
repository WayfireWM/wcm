xml = dependency('libxml-2.0', required: true)
gtkmm     = dependency('gtkmm-3.0', required: true)
wf_config = dependency('wf-config', version: '>=0.6.0', required: true)
xkbregistry = dependency('xkbregistry', required: true)

dep_list = [xml, gtkmm, wf_config, wf_protos, evdev, xkbcommon, xkbregistry]

sources = files('main.cpp', 'metadata.cpp', 'wcm.cpp', 'utils.cpp')

executable(meson.project_name(), sources,
                     install : true,
                     dependencies : dep_list)
