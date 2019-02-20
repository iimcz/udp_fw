
#include <signal.h>

#include "udp_socket.h"

constexpr auto STATS_COUNT = 1000;


namespace {
    udp_socket_t *global_sock_ptr = nullptr;

    static struct sigaction act;

    void sigHandler(int sig, siginfo_t */*siginfo*/, void */*context*/) {
        if (sig == SIGPIPE) {
            // Sigpipe needs to be ignored, otherwise application may get killed randomly
            return;
        }
        if (global_sock_ptr) {
            global_sock_ptr->stop();
        }
        act.sa_handler = SIG_DFL;
        act.sa_flags &= ~SA_SIGINFO;
        sigaction(SIGINT, &act, 0);
    }

}


int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage " << argv[0] << " <listen port> <target address> <target port>\n";
        std::cerr << "\n\t" << argv[0] << " listens on a port and forwards every packet to a specified address.\n";

        return 1;
    }

    try {
        std::cout << "Creating a receive socket\n";
        udp_socket_t receive_socket;
        std::cout << "Enabling broadcast\n";
        receive_socket.enable_broadcast();
        std::cout << "Binding receive socket\n";
        receive_socket.bind(std::stoul(argv[1]));

        std::cout << "Creating sending socket\n";
        udp_socket_t send_socket;
        std::cout << "Binding send socket\n";
        send_socket.bind(0);
        std::cout << "Connecting send socket\n";
        send_socket.connect(argv[2], argv[3]);

        // Install signal handler

        global_sock_ptr = &receive_socket;
        ::memset(&act, '\0', sizeof(act));
        act.sa_sigaction = &sigHandler;
        act.sa_flags = SA_SIGINFO;
        ::sigaction(SIGINT, &act, 0);
        ::sigaction(SIGPIPE, &act, 0);

        // And wait for data
        size_t count = 0;
        size_t size = 0;
        while(receive_socket.active()) {
            const auto& data = receive_socket.wait_for_data();
            if (data.empty()) {
                std::cout << "X";
            } else {
                //std::cout << "Received data, forwarding\n";
                send_socket.send(data);
                std::cout << ".";
                if (++count % 80 == 0) {
                    std::cout << "\n";
                }
                size += data.size();
                if (count >= STATS_COUNT) {
                    std::cout << "\nSend " << count << " packets, " << size << "B\n";
                    count = 0;
                    size = 0;
                }
            }
        }


        std::cout << "Quitting\n";
    } catch (std::runtime_error &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }


}