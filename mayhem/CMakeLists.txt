
add_executable(fuzz_libnest fuzz_nest.cpp)

target_compile_options(fuzz_libnest PRIVATE -fsanitize=undefined,fuzzer)
target_link_options(fuzz_libnest PRIVATE -fsanitize=undefined,fuzzer)

target_link_libraries(fuzz_libnest libnest2d)