project('wcm', 'c', 'cpp', version : '0.11.0', default_options : 'cpp_std=c++17')

fs = import('fs')

add_global_arguments('-DWAYFIRE_CONFIG_FILE="' + get_option('wayfire_config_file_path') + '"', language : 'cpp')
add_global_arguments('-DWF_SHELL_CONFIG_FILE="' + get_option('wf_shell_config_file_path') + '"', language : 'cpp')

wayfire = dependency('wayfire', version: '>=0.11.0')
wf_shell = dependency('wf-shell', required : get_option('wf_shell'))

wayfire_metadata_dir = wayfire.get_variable(pkgconfig: 'metadatadir')
wayfire_sysconf_dir = wayfire.get_variable(pkgconfig: 'sysconfdir')

share_dir = join_paths(get_option('prefix'), 'share')
wayfire_locale_dir = join_paths(share_dir, 'locale')
icon_dir = join_paths(share_dir, 'wcm', 'icons')

add_global_arguments('-DWAYFIRE_METADATADIR="' + wayfire_metadata_dir + '"', language : 'cpp')
add_global_arguments('-DWAYFIRE_LOCALEDIR="' + wayfire_locale_dir + '"', language : 'cpp')
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

install_data('wcm.desktop', install_dir: join_paths(share_dir, 'applications'))

mo_targets = []

po_files_raw = run_command('find', 'locale', '-name', '*.po', check : true).stdout().strip().split('\n')
po_files = files(po_files_raw)

foreach po_file : po_files
  language = fs.name(fs.parent(fs.parent(po_file)))
  mo_file = fs.stem(po_file) + '.mo'
  t = custom_target('mo-' + language,
    input : po_file,
    output : mo_file,
    command : ['msgfmt', '@INPUT@', '-o', '@OUTPUT@'],
    install : true,
    install_dir : join_paths(
      wayfire_locale_dir,
      language,
      'LC_MESSAGES'
    )
  )

  mo_targets += t
endforeach
