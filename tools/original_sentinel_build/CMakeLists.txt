cmake_minimum_required(VERSION 3.20)
project(OriginalSentinelProviderChecks LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(SENTINEL_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# Private composition includes the real owner-pinned Honey factory. Production
# course admission remains separate until actual source77 native gameplay passes.
add_subdirectory("${SENTINEL_SOURCE}/tools/original_resource_build" resources)
target_sources(pikmin_pc PRIVATE
 "${SENTINEL_SOURCE}/pc_port/pc_p2_original_shijimi_state.cpp"
 "${SENTINEL_SOURCE}/pc_port/pc_p2_original_shijimi_honey.cpp"
 "${SENTINEL_SOURCE}/pc_port/pc_p2_original_shijimi_honey_native.cpp")
add_executable(p2_original_shijimi_honey_identity
 "${SENTINEL_SOURCE}/tools/test_p2_original_shijimi_honey.cpp"
 "${SENTINEL_SOURCE}/pc_port/pc_p2_original_shijimi_honey.cpp"
 "${SENTINEL_SOURCE}/pc_port/pc_p2_original_resource_contents.cpp")
target_include_directories(p2_original_shijimi_honey_identity PRIVATE "${SENTINEL_SOURCE}/pc_port")
target_compile_options(p2_original_shijimi_honey_identity PRIVATE -Wall -Wextra -Werror -UNDEBUG)
enable_testing()
add_test(NAME original_shijimi_honey_identity COMMAND p2_original_shijimi_honey_identity)
add_executable(p2_original_shijimi_state
 "${SENTINEL_SOURCE}/tools/test_p2_original_shijimi_state.cpp"
 "${SENTINEL_SOURCE}/pc_port/pc_p2_original_shijimi_state.cpp")
target_include_directories(p2_original_shijimi_state PRIVATE "${SENTINEL_SOURCE}/pc_port")
target_compile_options(p2_original_shijimi_state PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME original_shijimi_state COMMAND p2_original_shijimi_state)
