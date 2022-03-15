cmake_minimum_required(VERSION 3.16)


project(its-server-uslp
	LANGUAGES CXX
)


add_executable(server-uslp
	src/bus_io.hpp
	src/bus_io.cpp
	src/bus_messages.hpp
	src/bus_messages.cpp
	src/log.hpp
	src/event_handler.hpp
	src/main.cpp

	libs/json.hpp
	libs/loguru.hpp
	libs/loguru.cpp
)

target_compile_definitions(server-uslp PRIVATE LOGURU_WITH_STREAMS)

target_include_directories(server-uslp
PRIVATE
	libs
)


target_link_libraries(server-uslp
PRIVATE
	ccsds::uslp
	zmq
)
