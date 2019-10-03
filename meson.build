project('wcm', 'cpp', version : '0.1')

add_global_arguments('-DCONFIG_FILE_PATH="' + get_option('config_file_path') + '"', language : 'cpp')

share_dir = join_paths(get_option('prefix'), 'share')
wayfire_share_dir = join_paths(share_dir, 'wayfire')
metadata_dir = join_paths(wayfire_share_dir, 'metadata')
icon_dir = join_paths(wayfire_share_dir, 'icons')
add_global_arguments('-DMETADATADIR="' + metadata_dir + '"', language : 'cpp')
add_global_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

dependency('pkg-config', required : true)

icon_dir = join_paths(wayfire_share_dir, 'icons')
add_project_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

subdir('metadata')
subdir('icons')
subdir('src')

install_data('wayfire-config-manager.desktop', install_dir: join_paths(share_dir, 'applications'))
