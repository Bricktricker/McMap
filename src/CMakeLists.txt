file(GLOB src_files "*.cpp" "*.h" "*.hpp")

add_executable(McMap ${src_files})

target_link_libraries(McMap ZLIB::ZLIB)
target_link_libraries(McMap PNG::PNG)
target_link_libraries(McMap Threads::Threads)
