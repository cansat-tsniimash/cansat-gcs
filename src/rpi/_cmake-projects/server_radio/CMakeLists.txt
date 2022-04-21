cmake_minimum_required(VERSION 3.16)


project(_root
	LANGUAGES C CXX
)


add_subdirectory(../../../../shared/sx126x/sx126x libs/sx126x)
add_subdirectory(../../server-radio server-radio)
