# TODO this copypaste from src/meson.build is terrible..

gtest = dependency('gtest', main : true, required : false)
gmock = dependency('gmock', required : false)

lcurl = dependency('libcurl', version : '>= 7.61.0')
lsqlite = dependency('sqlite3', version : '>= 3.25.0')

ragel = find_program('ragel')
ragel_gen = generator(ragel, output: '@BASENAME@.cpp',
  arguments: ['-I@SOURCE_DIR@', '-o', '@OUTPUT@', '@INPUT@'])

ragel_src = ragel_gen.process('../src/lexer.rl')

src_dir = include_directories('../src')

e = executable(
	'gtest', 
	'test.cpp',
	ragel_src,
	dependencies : [gtest, gmock, lcurl, lsqlite],
	include_directories : [inc, src_dir])
test('gtest', e)