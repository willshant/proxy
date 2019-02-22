#include "server.hpp"
#include "client.hpp" 

int main(int argc, char *argv[]) {
    Server server;
    
    if (server.init_server() == -1) {
        cerr << "Error: cannot build server" << endl;
        return -1;
    }

    // cout << "Waiting for connection on port " << port << endl;
    int i = 0;
    while (1) {
        struct sockaddr_storage socket_addr;
        socklen_t socket_addr_len = sizeof(socket_addr);
        int client_connection_fd;
        cout << "ith loop:" << i++ << endl;
        client_connection_fd = accept(server.socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
        cout << client_connection_fd << endl;
        //cout << "test" << endl;
        // socket_fd will keep 
        if (client_connection_fd == -1) {
            cerr << "Error: cannot accept connection on socket" << endl;
            return -1;
        } //if

        string buf = recvClientAll(client_connection_fd);
        cout << "request: " << endl;
        cout << buf << endl;
        Client client(buf);
//        cout << "Server received: \n" << client.body << endl << client.host << endl
//            << client.line1 << endl << client.method << endl << client.uid << endl;

        ProxySocket ps(client.host);
        ps.init_socket();
        sendAll(ps.socket_fd, client.body, client.body.size());

        // select
        // struct timeval tv;
        // fd_set readfds, master;

        // tv.tv_sec = 2;
        // tv.tv_usec = 500000;

        // FD_ZERO(&master);
        // FD_SET(ps.socket_fd, &master);
        // vector<char> buffer(1024, 0);
        // int index = 0;
        // int i = 1;
        // for (;;) {
        //     readfds = master;
        //     if (select(ps.socket_fd + 1, &readfds, NULL, NULL, &tv) == -1) {
        //         cout << "select" << endl;
        //         exit(4);
        //     }
        //     if (FD_ISSET(ps.socket_fd, &readfds)) {
        //         int byte_read = recv(ps.socket_fd, &buffer.data()[index] , (1024 * i) - index, 0);
        //         cout << "byte read: " << byte_read << endl;
        //         if (byte_read == 0) {
        //             break;
        //         }
        //         index += byte_read;
        //         if (index == 1024 * i) {
        //             buffer.resize(1024 * (++i), 0);
        //         } 
        //     }
        // }
        // vector<char> response(buffer);
        vector<char> response = recvAll(ps.socket_fd);
        cout << "response: " << endl;
        cout << response.data() << endl;
        cout << response.size() << endl;
        sendAll(client_connection_fd, response, response.size());
        freeaddrinfo(ps.host_info_list);
        close(ps.socket_fd);

        
    }
    freeaddrinfo(server.host_info_list);
    close(server.socket_fd);

    return 0;
}

