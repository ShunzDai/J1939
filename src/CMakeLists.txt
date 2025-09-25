file(GLOB_RECURSE SOURCES_J1939 LIST_DIRECTORIES false
  j1939.c
)

add_library(j1939 STATIC ${SOURCES_J1939})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

elseif(ESP_PLATFORM)
  target_link_libraries(j1939 PRIVATE idf::driver)
endif()

target_include_directories(j1939 PUBLIC
  .
)
