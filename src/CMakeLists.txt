add_executable(udp_fw udp_fw.cpp udp_socket.h)

install(TARGETS udp_fw RUNTIME DESTINATION bin)