project('wcm', 'c', 'cpp', version : '0.5.0', default_options : 'cpp_std=c++17')

add_global_arguments('-DWAYFIRE_CONFIG_FILE="' + get_option('wayfire_config_file_path') + '"', language : 'cpp')
add_global_arguments('-DWF_SHELL_CONFIG_FILE="' + get_option('wf_shell_config_file_path') + '"', language : 'cpp')

wayfire = dependency('wayfire')
wf_shell = dependency('wf-shell', required : get_option('wf_shell'))

wayfire_metadata_dir = wayfire.get_variable(pkgconfig: 'metadatadir')
wayfire_sysconf_dir = wayfire.get_variable(pkgconfig: 'sysconfdir')

share_dir = join_paths(get_option('prefix'), 'share')
icon_dir = join_paths(share_dir, 'wayfire', 'icons')

add_global_arguments('-DWAYFIRE_METADATADIR="' + wayfire_metadata_dir + '"', language : 'cpp')
add_global_arguments('-DWAYFIRE_SYSCONFDIR="' + wayfire_sysconf_dir + '"', language : 'cpp')
if wf_shell.found()
  wf_shell_metadata_dir = wf_shell.get_variable(pkgconfig: 'metadatadir')
  wf_shell_sysconf_dir = wf_shell.get_variable(pkgconfig: 'sysconfdir')
  add_project_arguments('-DHAVE_WFSHELL=1', language : 'cpp')
  add_global_arguments('-DWFSHELL_METADATADIR="' + wf_shell_metadata_dir + '"', language : 'cpp')
  add_global_arguments('-DWFSHELL_SYSCONFDIR="' + wf_shell_sysconf_dir + '"', language : 'cpp')
endif
add_global_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

dependency('pkg-config', required : true)
evdev = dependency('libevdev')

add_project_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

subdir('icons')
subdir('proto')
subdir('src')

install_data('wayfire-config-manager.desktop', install_dir: join_paths(share_dir, 'applications'))
