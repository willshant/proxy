#include <string>
#include<cstdio>
#include <string.h>
#include <vector>
#include<iostream>
#include <ctime>

using namespace std;

static size_t init_uid=0;

class Client {
public:
    size_t uid;
    string method;
    string url;
    string host;
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
        cout << "line1" << line1 << endl;
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
    time_t expiration_time;
    bool if_validate;
    vector<char> content;
    Response(vector<char> & response_content, string & url): url(url), if_cache(false), 
        expiration_time(time(0)), if_validate(false), content(response_content){
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
            if(header.find("must-revalidate", pos_cache) != string::npos || 
                    header.find("proxy-revalidate", pos_cache) != string::npos || 
                    header.find("no-cache", pos_cache) != string::npos ){
                if_validate = true;
            }
            if (size_t pos = header.find("s-maxage", pos_cache) != string::npos){
                size_t pos1 = header.find("=", pos);
                size_t pos2 = pos1 + 1;
                pos1++;
                while (isdigit(header[pos1])){
                    pos1++;
                }
                int age = atoi(header.substr(pos2, pos1 - pos2 - 1).c_str());
                time_t now = time(0);
                expiration_time = now + age;
            }
            else if (size_t pos = header.find("max-age", pos_cache) != string::npos){
                size_t pos1 = header.find("=", pos);
                size_t pos2 = pos1 + 1;
                pos1++;
                while (isdigit(header[pos1])){
                    pos1++;
                }
                int age = atoi(header.substr(pos2, pos1 - pos2 - 1).c_str());
                time_t now = time(0);
                expiration_time = now + age;
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

            if_cache = true;
        }
    }
};