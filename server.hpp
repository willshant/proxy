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

vector<char> CreateRequest(unordered_map<string, Response>::const_iterator it, Client & client, bool EtagOrMod){
    string toAdd;
    if (EtagOrMod == true){
        toAdd = "If-None-Match: " + it->second.etag + "\r\n";
    }
    else {
        toAdd = "If-Modified-Since: " + it->second.last_modified + "\r\n";
    }
    vector<char> rev = client.content;
    char * pos = strstr(rev.data(), "\r\n\r\n");
    vector<char>::iterator it1 = rev.begin() + (pos + 2 - rev.data());
    vector<char> temp(toAdd.begin(), toAdd.end());
    rev.insert(it1, temp.begin(), temp.end());
    cout << endl << "new validate header: " << rev.data() << endl << endl;
    return rev;
}

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
        if (recv(fd, buffer, 1, 0) == 0){
            cout << "receive return 0" << endl;
            res.resize(0, 0);
            return res;
        }
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
    cout << "no header" << endl;
    return body;
}

void sendAll(int fd, vector<char> & target, int size){
    int sum = 0;
    while(sum != size){
        int i = send(fd, target.data() + sum, size-sum, 0);
        if (i == -1) {
            cout << "error in send" << endl;
            break;
        }
        cout << "in send all: " << endl;
        cout << i << endl;
        sum += i;
    }
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
                    vector<char> message(1024, 0);
                    status = recv(i, message.data(), 1024, 0);
                    if (status == 0 || status == -1){
                        cout << "exit flag is set to 1" << endl;
                        exitFlag = 1;
                        break;
                    }
                    message.resize(status);
                    if (i == client_fd){
                        send(server_fd, message.data(), status, 0);
                    }
                    else if (i == server_fd) {
                        send(client_fd, message.data(), status, 0);
                    }
                }
            }
            if (exitFlag == 1){
                break;
                cout << "exit flag == 1" << endl;
                logfile.close_tunnel(client);
            }
        }
    }
    cout << "exit select" << endl;
}

void MethodPost(int server_fd, int client_fd, Client & client){
    sendAll(server_fd, client.content, client.content.size());
    vector<char> response = recvHeader(server_fd);
    vector<char> response_body = parseHeader(server_fd, response);
    if (response_body.size() != 0) {
        response.insert(response.end(), response_body.begin(), response_body.end());
    }
    sendAll(client_fd, response, response.size());
}

void MethodGet(int client_fd, int server_fd, Client & client) {
    unordered_map<string, Response>::iterator it = cache.find(client.url);
    if (it != cache.end()) {
        cout << "whether cached" << endl;
        if (it->second.if_nocache || it->second.expiration_time < time(0)) {
            if (it->second.if_nocache){
                // print validate
                logfile.validate(client);
            }
            else if (it->second.expiration_time < time(0)){
                // print expired
                logfile.expired(client, it->second);
            }
            cout << "need revalidation" << endl;
            if (!it->second.etag.empty()) {
                cout << "add etag" << endl;
                vector<char> newReq = CreateRequest(it, client, true);
                sendAll(server_fd, newReq, newReq.size());
            }
            else if (!it->second.last_modified.empty()) {
                cout << "add last mod" << endl;
                vector<char> newReq = CreateRequest(it, client, false);
                sendAll(server_fd, newReq, newReq.size());
            }
            else {
                // send original request to server
                sendAll(server_fd, client.content, client.content.size());
            }
            logfile.re_request(client);
        }
        else {
            // send response and return
            logfile.valid(client);
            sendAll(client_fd, it->second.content, it->second.content.size());
            return;
        }
    }
    else {
        // send original request to server
        logfile.not_in_cache(client);
        sendAll(server_fd, client.content, client.content.size());
    }

    // sendAll(server_fd, client.content, client.content.size());
    vector<char> response = recvHeader(server_fd);
    cout << response.data() << endl;
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
    cout << "url: " << response_class.url << endl << "status: " << response_class.status << "if cache: " << response_class.if_cache << endl
        << "expiration_time: " << response_class.expiration_time << endl << "if validate: " << response_class.if_validate
        << endl << "age: " << response_class.age << endl << "last_modified: " << response_class.last_modified << endl
        << "etag: " << response_class.etag << endl << response_class.content.data() << endl;
    logfile.receive_response(client, response_class);
    
    // unordered_map<string, Response>::iterator it_incache = cache.find(client.url);
    if (response_class.status == "304" && it != cache.end()){
        cout << "304 received, return cached content" << endl;
        cout << it->first << endl;
        sendAll(client_fd, it->second.content, it->second.content.size());   
    } else if (response_class.status == "200") {
        if (response_class.if_cache) {
            if (it != cache.end()) {
                cout << "removed stale cache" << endl;
                cache.erase(response_class.url);
            }
            cout << "saved response in cache" << endl;
            cache.insert(make_pair(response_class.url, response_class));
            if (response_class.if_nocache) {
                logfile.need_revalidate(client);
            } else {
                logfile.expire_cache(client, response_class);
            }
        } else {
            logfile.not_cacheable(client);
        }
        sendAll(client_fd, response, response.size());
    } else {
        sendAll(client_fd, response, response.size());
    }
    logfile.responding(client, response_class);
}


void handleRequest(int client_connection_fd, string ipaddr) {
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

    Client client(client_request, ipaddr); // pass request header + body
    
    logfile.request_from_client(client);
    
    cout << "port: " << client.port << endl;
    cout << "method: " << client.method << endl << "host: " << client.host << endl << "url: " << client.url << endl << "body: " << client.content.data() << endl;
    cout << "HTTP: " << client.httpVersion << endl;
    cout << "Cache Control fields\n";
    cout << "no-store: " << client.no_store << endl << "only_if_cached: " << client.only_if_cached << endl;
    cout << "no_cache: " << client.no_cache << endl << "if_max_stale: " << client.if_max_stale << endl << "if_max_stale_has_value: " << client.if_max_stale_has_value << endl;
    cout << "max-stale: " << client.max_stale << endl << "if_max_age: " << client.if_max_age << endl << "if_min_fresh: " << client.if_min_fresh << endl;
    cout << "min-fresh: " << client.min_fresh << endl;
    ClientSocket client_socket(client.host, client.port);
    
    if (client_socket.init_socket() == -1) {
        logfile.err_unresolvable_method(client);
        return;
    }
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
