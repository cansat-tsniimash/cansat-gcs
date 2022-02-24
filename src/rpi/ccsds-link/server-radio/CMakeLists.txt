cmake_minimum_required(VERSION 3.16)


project(its-server-radio
	LANGUAGES C
)


add_subdirectory(libs/log.c)
add_subdirectory(libs/jsmin)


add_executable(server-radio
	src/main.c
	src/server.h
	src/server.c
	src/server-zmq.h
	src/server-zmq.c
	src/server-config.h
	src/server-config.c

	libs/sx126x/inc/sx126x_board_rpi.h
	libs/sx126x/src/sx126x_board_rpi.c
)

target_include_directories(server-radio PRIVATE libs/sx126x/inc/)

target_link_libraries(server-radio
PRIVATE
	sx126x::sx126x
	logc
	jsmin
	gpiod
	zmq
)

