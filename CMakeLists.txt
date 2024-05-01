cmake_minimum_required(VERSION 3.20)
project(CrossPcInputRelay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(relay_protocol
    Protocol/MousePacket.cpp
    Protocol/Observability.cpp
)
target_include_directories(relay_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(input_relay main.cpp)
target_link_libraries(input_relay PRIVATE relay_protocol)

if(WIN32)
    target_sources(input_relay PRIVATE
        Protocol/BluetoothTransport.cpp
        input_injection/MouseInjector.cpp
    )
    target_link_libraries(input_relay PRIVATE Ws2_32 Bthprops User32)

    add_executable(raw_input_sample
        input_capture/main.cpp
        input_capture/MouseHook.cpp
    )
    target_include_directories(raw_input_sample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(raw_input_sample PRIVATE User32)

    add_executable(injection_sample
        input_injection/main.cpp
        input_injection/MouseInjector.cpp
        Protocol/MousePacket.cpp
    )
    target_include_directories(injection_sample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(injection_sample PRIVATE User32)
endif()

add_executable(protocol_self_test tests/protocol_self_test.cpp)
target_link_libraries(protocol_self_test PRIVATE relay_protocol)

enable_testing()
add_test(NAME protocol_self_test COMMAND protocol_self_test)
