#include "server.hpp"
#include "client.hpp" 

int main(int argc, char *argv[]) {
    Server server;
    
    if (server.init_server() == -1) {
        cerr << "Error: cannot build server" << endl;
        return -1;
    }

    // cout << "Waiting for connection on port " << port << endl;
    struct sockaddr_storage socket_addr;
    socklen_t socket_addr_len = sizeof(socket_addr);
    int client_connection_fd;
    int i = 0;
    while (1) {
        cout << "i:" << i++ << endl;
        client_connection_fd = accept(server.socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
//        cout << "test" << endl;
        // socket_fd will keep 
        if (client_connection_fd == -1) {
            cerr << "Error: cannot accept connection on socket" << endl;
            return -1;
        } //if

        string buf = recvClientAll(client_connection_fd);
        cout << buf << endl;
        Client client(buf);
//        cout << "Server received: \n" << client.body << endl << client.host << endl
//            << client.line1 << endl << client.method << endl << client.uid << endl;

        ProxySocket ps(client.host);
        ps.init_socket();
        sendAll(ps.socket_fd, client.body, client.body.size());
        vector<char> response = recvAll(ps.socket_fd);
        cout << response.data() << endl;
        sendAll(client_connection_fd, response, response.size());
        freeaddrinfo(ps.host_info_list);
        close(ps.socket_fd);

        
    }
    freeaddrinfo(server.host_info_list);
    close(server.socket_fd);

    return 0;
}

