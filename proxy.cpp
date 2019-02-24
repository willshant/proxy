#include "server.hpp"

int main() {
    Server server;
    
    if (server.init_server() == -1) {
        cerr << "Error: cannot build server" << endl;
        return -1;
    }

    int i = 0;
    while (1) {
        struct sockaddr_storage socket_addr;
        socklen_t socket_addr_len = sizeof(socket_addr);
        int client_connection_fd;
        cout << "ith loop:" << i++ << endl;
        client_connection_fd = accept(server.socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
        cout << "client fd: " << client_connection_fd << endl;
        // socket_fd will keep 
        if (client_connection_fd == -1) {
            cerr << "Error: cannot accept connection on socket" << endl;
            return -1;
        } //if
        // multithreading starts
        thread(handleRequest, client_connection_fd).detach();
        
      
    }
    freeaddrinfo(server.host_info_list);
    close(server.socket_fd);

    return 0;
}
