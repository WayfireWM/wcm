project('wcm', 'cpp', version : '0.1')

add_global_arguments('-DCONFIG_FILE_PATH="' + get_option('config_file_path') + '"', language : 'cpp')

share_dir = join_paths(get_option('prefix'), 'share', 'wayfire')
metadata_dir = join_paths(share_dir, 'metadata')
icon_dir = join_paths(share_dir, 'icons')
add_global_arguments('-DMETADATADIR="' + metadata_dir + '"', language : 'cpp')
add_global_arguments('-DICONDIR="' + icon_dir + '"', language : 'cpp')

dependency('pkg-config', required : true)

subdir('metadata')
subdir('icons')
subdir('src')
