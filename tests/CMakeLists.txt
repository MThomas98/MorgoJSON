include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(morgojson_tests tests.cpp Lexer_tests.cpp)
target_link_libraries(morgojson_tests PRIVATE MorgoJSON::morgojson GTest::gtest_main)
# Lexer tests exercise internal headers that live in src/.
target_include_directories(morgojson_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(morgojson_tests PRIVATE MORGOJSON_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

include(GoogleTest)
gtest_discover_tests(morgojson_tests)
