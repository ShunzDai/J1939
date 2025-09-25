file(GLOB_RECURSE SOURCES LIST_DIRECTORIES false *.h *.cpp *.c)

add_executable(test ${SOURCES})

target_link_libraries(test PUBLIC j1939)
