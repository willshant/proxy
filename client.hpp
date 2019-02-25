#include <string>
#include<cstdio>
#include <string.h>
#include <vector>
#include<iostream>
#include <ctime>
#include <unordered_map>

using namespace std;

static size_t init_uid=0;

class Client {
public:
    size_t uid;
    string method;
    string url;
    string host;
    string header;
    vector<char> content;
    string port;
    string httpVersion;
    Client(vector<char> & request_content): content(request_content) {
        uid = init_uid++;
        char * pos = strstr(request_content.data(), " ");
        char temp[10];
        strncpy(temp, request_content.data(), pos - request_content.data());
        temp[pos - request_content.data()] = 0;
        method = temp;
        pos = strstr(request_content.data(), "\r\n");
        char temp_line[8192];
        strncpy(temp_line, request_content.data(), pos - request_content.data());
        temp_line[pos - request_content.data()] = 0;
        string line1(temp_line);
        size_t pos_1 = line1.find(" ");
        size_t pos_2 = line1.find(" ", pos_1 + 1);
        url = line1.substr(pos_1 + 1, pos_2 - pos_1 -1);
        size_t http = line1.find("HTTP");
        // report error for no version format
        httpVersion = line1.substr(http, string::npos);
        pos = strstr(request_content.data(), "Host: ");
        char * space = pos + 6;;
        char * enter = strstr(space, "\r\n");
        strncpy(temp_line, space, enter - space);
        temp_line[enter - space] = 0;
        host = temp_line;
        if (method == "CONNECT"){
            // port = "443";
            size_t pos = host.find(":");
            if (pos == string::npos){
                port = "443";
            }
            else {
                port = host.substr(pos + 1, string::npos);
                host = host.substr(0, pos);
            }
        }
        else {
            size_t pos = host.find(":");
            if (pos == string::npos){
                port = "http";
            }
            else {
                port = host.substr(pos + 1, string::npos);
                host = host.substr(0, pos);
            }
        }
    }
};


class Response{
public:
    string url;
    // string line1;
    bool if_cache;
    bool if_nocache;
    bool if_validate;
    time_t expiration_time;
    time_t age;
    string last_modified;
    string etag;
    vector<char> content;
    Response(vector<char> & response_content, string & url): 
        url(url), 
        if_cache(false), 
        if_nocache(false), 
        if_validate(false), 
        expiration_time(time(0)), 
        age(0), 
        content(response_content) {
        char * header_end = strstr(response_content.data(), "\r\n\r\n");
        char temp[8192];
        strncpy(temp, response_content.data(), header_end - response_content.data());
        temp[header_end - response_content.data()] = 0;
        string header(temp);
        size_t pos_cache = header.find("Cache-Control:");
        if (pos_cache != string::npos){
            if (header.find("no-store", pos_cache) != string::npos || 
                header.find("private", pos_cache) != string::npos){
                if_cache = false;
                return;
            }
            size_t pos_age;
            if ((pos_age = header.find("Age: ")) != string::npos) {
                pos_age += 5;
                size_t pos_end = header.find("\r\n", pos_age);
                time_t age = atoi(header.substr(pos_age, pos_end - pos_age).c_str());
                this->age = age;
            }
            if (header.find("no-cache", pos_cache) != string::npos ) {
                if_nocache = true;
            }
            else if(header.find("must-revalidate", pos_cache) != string::npos || 
                    header.find("proxy-revalidate", pos_cache) != string::npos ){
                if_validate = true;
            }
            if (size_t pos = header.find("s-maxage", pos_cache) != string::npos){
                size_t pos1 = header.find("=", pos);
                size_t pos2 = pos1 + 1;
                pos1++;
                while (isdigit(header[pos1])){
                    pos1++;
                }
                int age = atoi(header.substr(pos2, pos1 - pos2).c_str());
                time_t now = time(0);
                expiration_time = now + age - this->age;
            }
            else if (size_t pos = header.find("max-age", pos_cache) != string::npos){
                size_t pos1 = header.find("=", pos);
                size_t pos2 = pos1 + 1;
                pos1++;
                while (isdigit(header[pos1])){
                    pos1++;
                }
                time_t age = atoi(header.substr(pos2, pos1 - pos2).c_str());
                time_t now = time(0);
                expiration_time = now + age - this->age;
            }
            else if (size_t pos = header.find("Expires") != string::npos){
                size_t pos1 = header.find(" ", pos); // after Expires:
                size_t pos2 = header.find("\r\n", pos); // the end of the line
                pos1 = header.find(" ", pos1); // after week string
                string temp = header.substr(pos1 + 1, pos2 - pos1); // day month year time GMT
                size_t pos_ws = temp.find(" "); // whitespace after day
                vector<string> time;
                while (pos_ws != string::npos){
                    string tem = temp.substr(0, pos_ws);
                    time.push_back(tem);
                    temp = temp.substr(pos_ws + 1, string::npos);
                    pos_ws = temp.find(" ");
                }
                string toTrans = time[0] + " " + time[1] + " " + time[2] + " " + time[3];
                struct tm timest;
                strptime(toTrans.c_str(), "%d %b %Y %H:%M:%S", &timest);

                expiration_time = mktime(&timest);
                //scan_httpdate(header.substr(pos1 + 1, pos2 - pos1 - 1).c_str, &x);
            }
            
            size_t pos_etag;
            if ((pos_etag = header.find("Etag: ")) != string::npos) {
                pos_etag += 6;
                size_t pos_end = header.find("\r\n", pos_etag);
                etag = header.substr(pos_etag, pos_end - pos_etag);
            }
            size_t pos_lastmod;
            if ((pos_lastmod = header.find("Last-Modified: ")) != string::npos) {
                pos_lastmod += 15;
                size_t pos_end = header.find("\r\n", pos_lastmod);
                last_modified = header.substr(pos_lastmod, pos_end - pos_lastmod);
            }

            if_cache = true;
        }
    }
};
