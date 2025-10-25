cmake_minimum_required(VERSION 3.10)

project(HTTPServer)

set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_CXX_STANDARD 17)


#asio
add_compile_definitions(ASIO_STANDALONE ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/asio/asio/include)


#Simple-Web-Server
set(USE_STANDALONE_ASIO ON)
set(BUILD_TESTING OFF)

#Проверяем обратим ли патч
execute_process(
  COMMAND git apply --reverse --check --directory=libs/Simple-Web-Server libs/patches/fix_simple_web_server_cmake.patch
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE patch_already_applied
)

if(patch_already_applied EQUAL 0)
  #Если патч успешно обратим, то патч уже применён - не делаем повторное применение
  message(STATUS "Patch already applied, skipping")
else()
  execute_process(
    COMMAND git apply --directory=libs/Simple-Web-Server libs/patches/fix_simple_web_server_cmake.patch
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE patch_result
  )
  if(NOT patch_result EQUAL 0)
    message(WARNING "Failed to apply patch: ${patch_result}")
  endif()
endif()

add_subdirectory(libs/Simple-Web-Server)


#json
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/json/include)


#spdlog
add_subdirectory(libs/spdlog)


add_executable(HTTPServer src/main.cpp src/FileSaver.cpp)
target_include_directories(HTTPServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/Simple-Web-Server)
target_link_libraries(HTTPServer PRIVATE simple-web-server)
target_link_libraries(HTTPServer PRIVATE spdlog::spdlog_header_only)
