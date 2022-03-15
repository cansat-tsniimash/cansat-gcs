cmake_minimum_required(VERSION 3.16)


project(its-server-tun
	LANGUAGES C CXX
)

find_package(Threads REQUIRED)


add_executable(server-tun
	src/main.cpp
	src/tun_device.hpp
	src/tun_device.cpp
	src/zmq_server.hpp
	src/zmq_server.cpp
	
	libs/json.hpp
	libs/loguru.hpp
	libs/loguru.cpp
)

target_compile_definitions(server-tun PRIVATE LOGURU_WITH_STREAMS)
target_include_directories(server-tun PRIVATE libs)

set_target_properties(server-tun
PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

target_link_libraries(server-tun
PRIVATE
	zmq
)
