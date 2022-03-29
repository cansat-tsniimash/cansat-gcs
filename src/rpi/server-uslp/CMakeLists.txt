cmake_minimum_required(VERSION 3.16)


project(its-server-uslp
	LANGUAGES CXX
)


find_package(Boost COMPONENTS log program_options REQUIRED)
find_package(Threads REQUIRED)

add_executable(server-uslp
	src/bus_io.hpp
	src/bus_io.cpp
	src/bus_messages.hpp
	src/bus_messages.cpp
	src/log.hpp
	src/log.cpp
	src/event_handler.hpp
	src/stack.hpp
	src/stack.cpp
	src/dispatcher.hpp
	src/dispatcher.cpp
	src/main.cpp

	libs/json.hpp
)

set_target_properties(server-uslp
PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

target_compile_definitions(server-uslp PRIVATE LOGURU_WITH_STREAMS)

target_include_directories(server-uslp
PRIVATE
	libs
)

target_link_libraries(server-uslp
PRIVATE
	Threads::Threads
	Boost::log
#	Boost::program_options
	zmq
	ccsds::uslp
)
