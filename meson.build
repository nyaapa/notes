project(
	'notes', 
	'cpp',
	version: '0.1.0',
	default_options : ['cpp_std=c++2a'],
	meson_version: '>= 0.40.0',
)

add_global_arguments(['-Wall', '-Wextra'], language: 'cpp')

inc = include_directories('include')

subdir('src')

subdir('t')