add_library(Network STATIC
    Socket.cpp
    Socket.hpp
)

target_include_directories(Network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# If on Windows, link against Winsock
if(WIN32)
    target_link_libraries(Network PUBLIC ws2_32)
endif()
