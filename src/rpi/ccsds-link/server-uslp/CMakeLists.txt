cmake_minimum_required(VERSION 3.16)


project(its-server-uslp
	LANGUAGES CXX
)


add_executable(server-uslp
	src/main.cpp
	src/json.hpp
	src/event_handler.hpp
)


target_link_libraries(server-uslp
PRIVATE
	ccsds::uslp
	zmq
)
