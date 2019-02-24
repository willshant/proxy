#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include "client.hpp"
#include <thread>

unordered_map<string, Response> cache;

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


class ClientSocket{
public:
    int socket_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    string hostname;
    const char *port;

    ClientSocket(string & host, string & port): hostname(host), port(port.c_str()){}

    int init_socket(){
        int status;
        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family   = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        // cout << "host name = "  << hostname.c_str() << endl;
        // cout << "port" << port << endl;
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

vector<char> recvHeader(int fd){
    cout << "in receivr header" << endl;
    vector<char> res;
    int flag = 0;
    char buffer[1];

    while(true){
        memset(buffer, 0, 1);
        // cout << "before recv" << endl;
        if (recv(fd, buffer, 1, 0) == 0){
            cout << "receive return 0" << endl;
            res.resize(0, 0);
            return res;
        }
        // cout << "received char: " << buffer << endl;
        res.push_back(buffer[0]);

        if (buffer[0] == '\r'){
            if(flag == 0 || flag == 2){
                flag++;
            }
            else {
                flag = 1;
            }
        }
        else if(buffer[0] == '\n'){
            if (flag == 1 || flag == 3){
                flag++;
            }
            else {
                flag = 0;
            }
        }
        else {
            flag = 0;
        }
        if (flag == 4){
            break;
        }
    }
    res.push_back('\0');
    return res;
}

vector<char> parseHeader(int fd, vector<char> & client_request_header) {
    cout << "in parse header" << endl;
    vector<char> body;
    char * pos1 = strstr(client_request_header.data(), "Content-Length:");
    char * pos2 = strstr(client_request_header.data(), "Transfer-Encoding:");
    // cout << "in parseheader\n" << pos1 << endl;
    if (pos2 != NULL){
        cout << "transfer encoding loop" << endl;
        client_request_header.pop_back();
        return recvHeader(fd);
    }
    else if (pos1 != NULL){
        char * length = strstr(pos1, "\r\n");
        char arr[10];
        strncpy(arr, pos1+16, length-pos1-16);
        arr[length-pos1-16] = 0;
        int num = atoi(arr);
        vector<char> buf(num+1, 0);
        if(num != 0){
            recv(fd, buf.data(), num, MSG_WAITALL);
            cout << "actual body " << buf.data() << endl;
            client_request_header.pop_back();
        }
        return buf;
    }
    // if (pos1 != NULL || pos1 != NULL) {
    //     cout << "content length or transfer encoding not null" << endl;
    //     return recvHeader(fd);
    // }
    cout << "no header" << endl;
    return body;
}


vector<char> recvAll(int fd) {
    vector<char> buffer(4096, 0);
    int index = 0;
    int i = 1;
    while (true) {
        int byte_read = recv(fd, &buffer.data()[index] , (4096 * i) - index, 0);
        cout << "byte read: " << byte_read << endl;
        if (byte_read == 0) {
            break;
        }
        index += byte_read;
        if (index == 4096 * i) {
            buffer.resize(4096 * (++i), 0);
        } 
    }
    return buffer;
}

void sendAll(int fd, vector<char> & target, int size){
    int sum = 0;
    while(sum != size){
//        cout << "enter" << endl;
        int i = send(fd, target.data() + sum, size-sum, 0);
        if (i == -1) {
            cout << "error in send" << endl;
            break;
        }
        cout << "in send all: " << endl;
        cout << i << endl;
        sum += i;
    }
//    cout << "exit" << endl;
}

void sendConnect(int fd, const char * buffer, int size){
    int sum = 0;
    while(sum != size){
        int i = send(fd, buffer, size - sum, 0);
        
        if (i == -1) {
            cout << "error in send" << endl;
            break;
        }
        cout << "in send all: " << endl;
        cout << i << endl;
        sum += i;
    }
    cout << "exit send all" << endl;
}

void send200(int fd, Client & client){
    string temp;
    temp = client.httpVersion + " 200 Connection Established\r\n\r\n";
    cout << "send 200 message:" << temp << endl;
    sendConnect(fd, temp.c_str(), temp.size());
}

void MethodCon(int client_fd, int server_fd, Client & client){
    // return 200 ok
    send200(client_fd, client);
    fd_set rfds, sub;
    FD_ZERO(&rfds);
    FD_SET(client_fd, &rfds);
    FD_SET(server_fd, &rfds);
    int fd_max = max(client_fd, server_fd);
    // cout << "client_fd: " << client_fd << endl;
    // cout << "server_fd: " << server_fd << endl;
    char buffer[4096];
    int exitFlag = 0;
    while(true){
        sub = rfds;
        // cout << "in select" << endl;
        int status = select(fd_max + 1, &sub, NULL, NULL, NULL);
        if (status == -1){
            cerr << "select error" << endl;
        }
        else {
            for (int i = 0; i <= fd_max; ++i){
                if (FD_ISSET(i, &sub) && (i == client_fd || i == server_fd)){
                    // cout << "received fd in select, fd = " << i << endl;
                    // memset(buffer, 0, 4096);
                    // status = recv(i, buffer, 4096, 0);
                    // int exitflag = 1;
                    // vector<char> message = recvHeader(i, exitflag);
                    vector<char> message(1024, 0);
                    status = recv(i, message.data(), 1024, 0);
                    // cout << "byte received: " << status << endl;
                    
                    // cout << "buffer received: " << message.data() << endl;
                    // cout << "receive length: " << message.size() << endl;
                    if (status == 0 || status == -1){
                        cout << "exit flag is set to 1" << endl;
                        exitFlag = 1;
                        break;
                    }
                    message.resize(status);
                    // int size = sizeof(buffer);
                    if (i == client_fd){
                        // cout << "forward message from client to server, fd to = " << server_fd << endl;
                        // sendAll(server_fd, message, message.size());
                        // cout << "before send" << endl;
                        send(server_fd, message.data(), status, 0);
                        // cout << "after send" << endl;
                    }
                    else if (i == server_fd) {
                        // cout << "forward message from server to client, fd to = " << client_fd << endl;
                        // sendAll(client_fd, message, message.size());
                        // cout << "before send" << endl;
                        send(client_fd, message.data(), status, 0);
                        // cout << "after send" << endl;
                    }
                }
            }
            if (exitFlag == 1){
                break;
                cout << "exit flag == 1" << endl;
            }
        }
    }
    cout << "exit select" << endl;
}

void MethodPost(int server_fd, int client_fd, Client & client){
    sendAll(server_fd, client.content, client.content.size());
    vector<char> response = recvHeader(server_fd);
    // if(response.size() == 0){
    //     cout << "strange behavior of new accept" << endl;
    //     return;
    // }
    vector<char> response_body = parseHeader(server_fd, response);
    if (response_body.size() != 0) {
        response.insert(response.end(), response_body.begin(), response_body.end());
    }
    sendAll(client_fd, response, response.size());
}

void MethodGet(int client_fd, int server_fd, Client & client) {
    sendAll(server_fd, client.content, client.content.size());
    int exitflag = 1;
    vector<char> response = recvHeader(server_fd);
    cout << response.data() << endl;
    if(exitflag == 0){
        cout << "strange behavior of new accept" << endl;
    }
    vector<char> response_body = parseHeader(server_fd, response);
    cout << response_body.size() << endl;
    if (response_body.size() != 0) {
        response.insert(response.end(), response_body.begin(), response_body.end());
    }
    //vector<char> response = recvAll(server_fd);
    cout << "response: " << endl;
    cout << response.data() << endl;
    // Cache the response
    Response response_class(response, client.url);
    cout << "url: " << response_class.url << endl << "if cache: " << response_class.if_cache << endl
        << "expiration_time: " << response_class.expiration_time << endl << "if validate: " << response_class.if_validate
        << endl << response_class.content.data() << endl;
        // test time
    if (response_class.if_cache) {
        cache.insert(make_pair(response_class.url, response_class));
    }
    
    

    sendAll(client_fd, response, response.size());
}


void handleRequest(int client_connection_fd) {
    vector<char> client_request = recvHeader(client_connection_fd);
    if(client_request.size() == 0){
        cout << "strange behavior of new accept" << endl;
        cout << "thread ends: " << client_connection_fd << endl;
        return;
    }
    cout << "client request header: " << endl;
    cout << client_request.data() << endl;

    vector<char> client_request_body = parseHeader(client_connection_fd, client_request);
    cout << "body size: " << client_request_body.size() << endl;
    if (client_request_body.size() != 0) {
        cout << "to insert body" << endl;
        // client_request.pop_back();
        client_request.insert(client_request.end(), client_request_body.begin(), client_request_body.end());
    }
    cout << "client full request: " << endl;
    cout << client_request.data() << endl;

    Client client(client_request); // pass request header + body
    cout << "port:\n" << client.port << endl;
    cout << "method:\n" << client.method << "host:\n" << client.host << "url:\n" << client.url << "body:\n" << client.content.data() << endl;
    cout << "HTTP" << client.httpVersion << endl;
    ClientSocket client_socket(client.host, client.port);
    client_socket.init_socket();
    // deal with this shit
    if (client.method == "CONNECT"){
        MethodCon(client_connection_fd, client_socket.socket_fd, client);
    }
    else if (client.method == "POST"){
        MethodPost(client_socket.socket_fd, client_connection_fd, client);
    }
    else if (client.method == "GET") {
        MethodGet(client_connection_fd, client_socket.socket_fd, client);
    } else {
        cout << "received unresolvable http method: " << client.method << endl;
    }
    
    cout << "before free" << endl;
    freeaddrinfo(client_socket.host_info_list);
    cout << "after first free" << endl;
    close(client_socket.socket_fd);
    cout << "after second free" << endl;
    cout << "thread ends: " << client_connection_fd << endl;
}
