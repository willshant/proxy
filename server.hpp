#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>

using namespace std;

// class 
class Server {
public:
    int socket_fd;
    struct addrinfo *host_info_list;
    int init_server() {
        int status;
        //
        struct addrinfo host_info;
        //
        const char *hostname = NULL;
        const char *port     = "12345";

        memset(&host_info, 0, sizeof(host_info));

        host_info.ai_family   = AF_UNSPEC; // does not care IP version
        host_info.ai_socktype = SOCK_STREAM; // TCP reliable
        host_info.ai_flags    = AI_PASSIVE; // auto fill in local host address

        status = getaddrinfo(hostname, port, &host_info, &host_info_list);
        if (status != 0) {
        cerr << "Error: cannot get address info for host" << endl;
        cerr << "  (" << hostname << "," << port << ")" << endl;
        return -1;
        } //if

        socket_fd = socket(host_info_list->ai_family, 
                    host_info_list->ai_socktype, 
                    host_info_list->ai_protocol); // TCP / UDP, default 0 to auto determine protocol based on socktype, 
        if (socket_fd == -1) {
        cerr << "Error: cannot create socket" << endl;
        cerr << "  (" << hostname << "," << port << ")" << endl;
        return -1;
        } //if

        int yes = 1;
        // reuse port when rerun the server within a minute
        status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        // bind socket to port we've set in getaddrinfo, and bind to host IP if AI_PASSIVE
        status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1) {
        cerr << "Error: cannot bind socket" << endl;
        cerr << "  (" << hostname << "," << port << ")" << endl;
        return -1;
        } //if

        status = listen(socket_fd, 100);
        if (status == -1) {
        cerr << "Error: cannot listen on socket" << endl; 
        cerr << "  (" << hostname << "," << port << ")" << endl;
        return -1;
        } //if

        return socket_fd;
    }
};

class ProxySocket{
public:
    int socket_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    string hostname;
    const char *port;

    ProxySocket(string & host): hostname(host), port("http"){}

    int init_socket(){
        int status;
        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family   = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        cout << "host name = "  << hostname.c_str() << endl;
        cout << "port" << port << endl;
        status = getaddrinfo(hostname.c_str(), port, &host_info, &host_info_list);
        if (status != 0) {
            cerr << "Error: cannot get address info for host" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        } //if

        socket_fd = socket(host_info_list->ai_family, 
                    host_info_list->ai_socktype, 
                    host_info_list->ai_protocol);
        if (socket_fd == -1) {
            cerr << "Error: cannot create socket" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        } //if
        
        cout << "Connecting to " << hostname << " on port " << port << "..." << endl;
        
        while(true){
            status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
            if (status != -1) {
                break;
            } //if
        }

        return socket_fd;
    }
    
};


string recvClientAll(int fd) {
    char buffer[4096];
    string res;
    while(true) {
        memset(buffer, 0, 4096);
        recv(fd, buffer, 4096, 0);
        string temp(buffer);
        res += temp;
        if (size_t pos = res.find("\r\n\r\n") != string::npos) {
            break;
        }
    }
    return res;
}



vector<char> recvAll(int fd) {
//    cout << "enter" << endl;
    
    vector<char> buffer(1024, 0);
    int index = 0;
    int i = 1;
    while (true) {
        int byte_read = recv(fd, &buffer.data()[index] , 1024 - index, 0);
        if (byte_read == 0) {
            break;
        }
        index += byte_read;
        if (index == 1024 * i) {
            buffer.resize(1024 * (++i), 0);
        } 
    }
    return buffer;
}

void sendAll(int fd, vector<char> & target, int size){
    int sum = 0;
    while(sum != size){
//        cout << "enter" << endl;
        int i = send(fd, target.data() + sum, size-sum, 0);
        cout << i << endl;
        sum += i;
    }
//    cout << "exit" << endl;
}