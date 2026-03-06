add_library(Core STATIC
    Logger.cpp
    Logger.hpp
)

target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
