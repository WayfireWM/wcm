project('wcm', 'c', 'cpp', version : '0.3.1', default_options : 'cpp_std=c++17')

add_global_arguments('-DWAYFIRE_CONFIG_FILE="' + get_option('wayfire_config_file_path') + '"', language : 'cpp')
add_global_arguments('-DWF_SHELL_CONFIG_FILE="' + get_option('wf_shell_config_file_path') + '"', language : 'cpp')

wayfire = dependency('wayfire')
metadata_dir = wayfire.get_variable(pkgconfig: 'metadatadir')

share_dir = join_paths(get_option('prefix'), 'share')
wayfire_share_dir = join_paths(share_dir, 'wayfire')
icon_dir = join_paths(wayfire_share_dir, 'icons')

add_global_arguments('-DMETADATADIR="' + metadata_dir + '"', language : 'cpp')
add_global_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

dependency('pkg-config', required : true)
evdev = dependency('libevdev')

icon_dir = join_paths(wayfire_share_dir, 'icons')
add_project_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

subdir('icons')
subdir('proto')
subdir('src')

install_data('wayfire-config-manager.desktop', install_dir: join_paths(share_dir, 'applications'))
