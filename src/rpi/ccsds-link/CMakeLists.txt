cmake_minimum_required(VERSION 3.16)


project(_root
	LANGUAGES C CXX
)


add_subdirectory(../../../shared/sx126x/sx126x libs/sx126x)
add_subdirectory(../../../shared/ccsds/ccsds-uslp-cpp libs/ccsds-uslp-cpp)
add_subdirectory(./server-radio server-radio)
add_subdirectory(./server-uslp server-uslp)
add_subdirectory(./server-tun server-tun)
