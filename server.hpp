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

// unordered_map<string, Response> cache;
Cache cache(100);

using namespace std;

vector<char> CreateRequest(Response & it, Client & client, bool EtagOrMod){
    string toAdd;
    if (EtagOrMod == true){
        toAdd = "If-None-Match: " + it.etag + "\r\n";
    }
    else {
        toAdd = "If-Modified-Since: " + it.last_modified + "\r\n";
    }
    vector<char> rev = client.content;
    char * pos = strstr(rev.data(), "\r\n\r\n");
    vector<char>::iterator it1 = rev.begin() + (pos + 2 - rev.data());
    vector<char> temp(toAdd.begin(), toAdd.end());
    rev.insert(it1, temp.begin(), temp.end());
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
            return -1;
        } //if

        socket_fd = socket(host_info_list->ai_family, 
                    host_info_list->ai_socktype, 
                    host_info_list->ai_protocol); // TCP / UDP, default 0 to auto determine protocol based on socktype, 
        if (socket_fd == -1) {
            return -1;
        } //if

        int yes = 1;
        // reuse port when rerun the server within a minute
        status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        // bind socket to port we've set in getaddrinfo, and bind to host IP if AI_PASSIVE
        status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1) {
            return -1;
        } //if

        status = listen(socket_fd, 100);
        if (status == -1) {
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
            return -1;
        } //if

        socket_fd = socket(host_info_list->ai_family, 
                    host_info_list->ai_socktype, 
                    host_info_list->ai_protocol);
        if (socket_fd == -1) {
            return -1;
        } //if
                
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
    vector<char> res;
    int flag = 0;
    char buffer[1];

    while(true){
        memset(buffer, 0, 1);
        if (recv(fd, buffer, 1, 0) == 0){
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
    vector<char> body;
    char * pos1 = strstr(client_request_header.data(), "Content-Length:");
    char * pos2 = strstr(client_request_header.data(), "Transfer-Encoding:");
    if (pos2 != NULL){
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
            client_request_header.pop_back();
        }
        return buf;
    }
    return body;
}

void sendAll(int fd, vector<char> & target, int size){
    int sum = 0;
    while(sum != size){
        int i = send(fd, target.data() + sum, size-sum, 0);
        if (i == -1) {
            break;
        }
        sum += i;
    }
}

void sendConnect(int fd, const char * buffer, int size){
    int sum = 0;
    while(sum != size){
        int i = send(fd, buffer, size - sum, 0);
        
        if (i == -1) {
            break;
        }
        sum += i;
    }
}

void send200(int fd, Client & client){
    string temp;
    temp = client.httpVersion + " 200 Connection Established\r\n\r\n";
    logfile.responding_code(client, temp.substr(0, temp.length() - 4));
    sendConnect(fd, temp.c_str(), temp.size());
}

void send504(int fd, Client & client){
    string temp;
    temp = client.httpVersion + " 504 Gateway Timeout\r\n\r\n";
    logfile.responding_code(client, temp.substr(0, temp.length() - 4));
    sendConnect(fd, temp.c_str(), temp.size());
}

void sendRevalidation(int server_fd, Client & client, Response & it) {
    logfile.validate(client);
    if (!it.etag.empty()) {
        vector<char> newReq = CreateRequest(it, client, true);
        sendAll(server_fd, newReq, newReq.size());
    }
    else if (!it.last_modified.empty()) {
        vector<char> newReq = CreateRequest(it, client, false);
        sendAll(server_fd, newReq, newReq.size());
    }
    else {
        // send original request to server
        sendAll(server_fd, client.content, client.content.size());
    }
    logfile.re_request(client);
}

void sendCache(int client_fd, Client & client, Response & it, bool expired) {
    if (expired) {
        logfile.expired(client, it);
    } else {
        logfile.valid(client);
    }
    sendAll(client_fd, it.content, it.content.size());
    logfile.responding(client, it);
} // need return after call

void sendOriginServer(int server_fd, Client & client) {
    sendAll(server_fd, client.content, client.content.size());
    logfile.not_in_cache(client);
    logfile.re_request(client);
}

void MethodCon(int client_fd, int server_fd, Client & client){
    // return 200 ok
    send200(client_fd, client);
    fd_set rfds, sub;
    FD_ZERO(&rfds);
    FD_SET(client_fd, &rfds);
    FD_SET(server_fd, &rfds);
    int fd_max = max(client_fd, server_fd);
    char buffer[4096];
    int exitFlag = 0;
    while(true){
        sub = rfds;
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
                logfile.close_tunnel(client);
                break;
            }
        }
    }
}

void MethodPost(int server_fd, int client_fd, Client & client){    
    logfile.re_request(client);
    sendAll(server_fd, client.content, client.content.size());
    vector<char> response = recvHeader(server_fd);
    vector<char> response_body = parseHeader(server_fd, response);
    Response response_class(response, client.url);
    logfile.receive_response(client, response_class);
    if (response_body.size() != 0) {
        response.insert(response.end(), response_body.begin(), response_body.end());
    }
    sendAll(client_fd, response, response.size());
    logfile.responding(client, response_class);
}

void MethodGet(int client_fd, int server_fd, Client & client) {
    Response *it = cache.find(client.url);
    
    if (it != NULL) {
    	it->age += time(0) - it->receive;
    }
    if (client.no_store == true || 
        it == NULL) {
        // send to original server
        sendOriginServer(server_fd, client);
    }
    else if (client.only_if_cached == true) {
        if (it == NULL) {
            // send 504
            send504(client_fd, client);
            return;
        }
        else {
            // send cache
            sendCache(server_fd, client, *it, false);
            return;
        }
    }
    else {
        if (it != NULL) {
            if (client.no_cache == true || 
                it->if_nocache == true) {
                // send validate
                sendRevalidation(server_fd, client, *it);
            }
            else if (client.if_max_stale == true) {
                if (client.if_max_stale_has_value == true) {
                    if (it->age + it->receive < it->expiration_time + 
                        client.max_stale) {
                        // send cache
                        bool expired = it->age + it->receive > it->expiration_time;
                        sendCache(server_fd, client, *it, expired);
                        return;
                    }
                    else {
                        // send validate
                        sendRevalidation(server_fd, client, *it);
                    }
                }
                else {
                    // send cache
                    sendCache(server_fd, client, *it, false);
                    return;
                }
            }
            else if (client.if_max_age == true) {
                if (client.max_age > it->age) {
                    // send cache
                    sendCache(server_fd, client, *it, false);
                    return;
                }
                else {
                    // send validate
                    logfile.expired(client, *it);
                    sendRevalidation(server_fd, client, *it);
                }
            }
            else if (client.if_min_fresh == true) {
                if (client.min_fresh < it->expiration_time - 
                    it->receive - it->age) {
                    // send cache
                    sendCache(server_fd, client, *it, false);
                    return;
                }
                else {
                    // send validate
                    sendRevalidation(server_fd, client, *it);
                }
            } else if (it->expiration_time < time(0)) {
                // send revalidation
                sendRevalidation(server_fd, client, *it);
            }
            else {
                // send cache and return
                sendCache(server_fd, client, *it, false);
                return;
            }
        }
        else {
            // send original request
            sendOriginServer(server_fd, client);
        }
    }

    // sendAll(server_fd, client.content, client.content.size());
    vector<char> response = recvHeader(server_fd);
    if (response.size() == 0) {
        // log it.
        logfile.err_receive_nothing(client);
        return;
    }
    vector<char> response_body = parseHeader(server_fd, response);
    if (response_body.size() != 0) {
        response.insert(response.end(), response_body.begin(), response_body.end());
    }
    // Cache the response
    Response response_class(response, client.url);
    
    logfile.receive_response(client, response_class);
    
    // unordered_map<string, Response>::iterator it_incache = cache.find(client.url);
    if (response_class.status == "304" && it != NULL){
        sendAll(client_fd, it->content, it->content.size());
    } else if (response_class.status == "200") {
        if (response_class.if_cache) {
            cache.insert(response_class.url, response_class);
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

    // cache.print(); // test whether the cache contains anything
}


void handleRequest(int client_connection_fd, string ipaddr) {
    vector<char> client_request = recvHeader(client_connection_fd);
    if(client_request.size() == 0){
        logfile.err_receive_nothing();
        return;
    }
    vector<char> client_request_body = parseHeader(client_connection_fd, client_request);
    if (client_request_body.size() != 0) {
        client_request.insert(client_request.end(), client_request_body.begin(), client_request_body.end());
    }

    Client client(client_request, ipaddr); // pass request header + body
    
    logfile.request_from_client(client);
    
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
    
    freeaddrinfo(client_socket.host_info_list);
    close(client_socket.socket_fd);
}
