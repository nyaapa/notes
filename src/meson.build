lcurl = dependency('libcurl', version : '>= 7.61.0')
lsqlite = dependency('sqlite3', version : '>= 3.25.0')

notes = executable(
    'notes', # executable name
    'main.cpp', # source files to be compiled
    include_directories : inc, # previously declared include directories in root :code:`meson.build`
    #link_with : 'lcurl', # linking executable with shared previously declared shared library :code:`libuuid`
    dependencies: [lcurl, lsqlite],
    install : true) # :code:`notes` executable be part of project installation