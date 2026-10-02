add_library(morgojson 
    JSONObject.cpp
    Lexer.cpp
    Parser.cpp)

add_library(MorgoJSON::morgojson ALIAS morgojson)

target_include_directories(morgojson
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
