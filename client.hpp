#include <string>
#include <cstdio>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <ctime>
#include <unordered_map>
#include <mutex> 
#include <list>
#include <shared_mutex>

using namespace std;

static size_t init_uid=0;
mutex log_file_lock;

class Client {
public:
    size_t uid;
    string method;
    string url;
    string line1;
    string host;
    string ipaddr;
    string header;
    vector<char> content;
    string port;
    string httpVersion;
    // below is the fields of cache control
    bool no_store;
    bool only_if_cached;
    bool no_cache;
    bool if_max_stale;
    bool if_max_stale_has_value;
    time_t max_stale;
    bool if_max_age;
    time_t max_age;
    bool if_min_fresh;
    time_t min_fresh;
    Client(vector<char> & request_content, string & ipadd): 
        ipaddr(ipadd), content(request_content), no_store(false), 
        only_if_cached(false), no_cache(false), if_max_stale(false), 
        if_max_stale_has_value(false), max_stale(0), if_max_age(false), 
        max_age(0), if_min_fresh(false), min_fresh(0) {
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
        this->line1 = line1;
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
        // parse time header fields
        char * header_end = strstr(content.data(), "\r\n\r\n");
        char header_temp[8192];
        strncpy(header_temp, content.data(), header_end - content.data());
        header_temp[header_end - content.data()] = 0; // extract the header part
        string header(header_temp);
        size_t pos_cache = header.find("Cache-Control:");
        if (pos_cache != string::npos) {
            if (header.find("no-store", pos_cache) != string::npos){
                no_store = true;
                return;
            }
            size_t pos_for_cache;
            if ((pos_for_cache = header.find("only-if-cached", pos_cache)) != string::npos) {
                only_if_cached = true;
            }
            else if ((pos_for_cache = header.find("no-cache", pos_cache)) != string::npos) {
                no_cache = true;
            }
            else if ((pos_for_cache = header.find("max-stale", pos_cache)) != string::npos) {
                if_max_stale = true;
                size_t pos_eq = header.find("=", pos_for_cache, 11);
                if (pos_eq != string::npos) {
                    if_max_stale_has_value = true;
                    size_t pos2 = pos_eq + 1;
                    pos_eq++;
                    while (isdigit(header[pos_eq])){
                        pos_eq++;
                    }
                    max_stale = atoi(header.substr(pos2, pos_eq - pos2 - 1).c_str());
                }
                else {
                    if_max_stale_has_value = false;
                }
            }
            else if ((pos_for_cache = header.find("max-age", pos_cache)) != string::npos) {
                if_max_age = true;
                size_t pos_eq = header.find("=", pos_for_cache);
                if (pos_eq != string::npos) {
                    size_t pos2 = pos_eq + 1;
                    pos_eq++;
                    while (isdigit(header[pos_eq])){
                        pos_eq++;
                    }
                    max_age = atoi(header.substr(pos2, pos_eq - pos2 - 1).c_str());
                }
            }
            else if ((pos_for_cache = header.find("min-fresh", pos_cache)) != string::npos) {
                if_min_fresh = true;
                size_t pos_eq = header.find("=", pos_for_cache);
                if (pos_eq != string::npos) {
                    size_t pos2 = pos_eq + 1;
                    pos_eq++;
                    while (isdigit(header[pos_eq])){
                        pos_eq++;
                    }
                    min_fresh = atoi(header.substr(pos2, pos_eq - pos2 - 1).c_str());
                }
            }
        }
    }
};

class Response{
public:
    string status;
    string url;
    string line1;
    bool if_cache;
    bool if_nocache;
    bool if_validate;
    time_t expiration_time;
    time_t age;
    string last_modified;
    string etag;
    vector<char> content;
    // change made by fking mind
    time_t receive;
    string location;
    Response() {};
    Response(vector<char> & response_content, string & url): 
        url(url), if_cache(false), if_nocache(false), 
        if_validate(false), expiration_time(time(0)), age(0),  
        content(response_content),receive(time(0)) {
        char * header_end = strstr(response_content.data(), "\r\n\r\n");
        char temp[8192];
        strncpy(temp, response_content.data(), header_end - response_content.data());
        temp[header_end - response_content.data()] = 0;
        string header(temp);
        size_t pos_line1 = header.find("\r\n");
        line1 = header.substr(0, pos_line1);
        // parse status
        size_t space_before, space_end;
        space_before = header.find(" ");
        space_end = header.find(" ", ++space_before);
        status = header.substr(space_before, space_end - space_before);
        // parse cache info
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
            size_t pos;
            if ((pos = header.find("s-maxage", pos_cache)) != string::npos){
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
            else if ((pos = header.find("max-age", pos_cache)) != string::npos){
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
            if_cache = true;
        }
        else {
            size_t pos;
            if ((pos = header.find("Expires: ")) != string::npos){
                cout << "expires pos: " << pos << endl;
                size_t pos1 = header.find(" ", pos); // after Expires:
                size_t pos2 = header.find("\r\n", pos); // the end of the line
                pos1 = header.find(" ", pos1 + 1); // after week string
                string temp = header.substr(pos1 + 1, pos2 - pos1); // day month year time GMT
                cout << "complete time format: " << temp << endl;
                size_t pos_ws = temp.find(" "); // whitespace after day
                vector<string> time;
                while (pos_ws != string::npos){
                    string tem = temp.substr(0, pos_ws);
                    time.push_back(tem);
                    cout << "every mistake: " << tem << endl;
                    temp = temp.substr(pos_ws + 1, string::npos);
                    pos_ws = temp.find(" ");
                }
                string toTrans = time[0] + " " + time[1] + " " + time[2] + " " + time[3];
                struct tm timest = {0};
                strptime(toTrans.c_str(), "%d %b %Y %H:%M:%S", &timest);

                expiration_time = mktime(&timest);
                //scan_httpdate(header.substr(pos1 + 1, pos2 - pos1 - 1).c_str, &x);
            }
            if_cache = true;
        }
        size_t pos_etag = header.find("ETag: ");
        if (pos_etag != string::npos) {
            cout << "etag position: " << pos_etag << endl;
            pos_etag += 6;
            size_t pos_end = header.find("\r\n", pos_etag);
            etag = header.substr(pos_etag, pos_end - pos_etag);
        }
        size_t pos_lastmod = header.find("Last-Modified: ");
        if (pos_lastmod != string::npos) {
            pos_lastmod += 15;
            size_t pos_end = header.find("\r\n", pos_lastmod);
            last_modified = header.substr(pos_lastmod, pos_end - pos_lastmod);
        }
        if (status == "301") {
            size_t stat = header.find("Location:");
            if (stat != string::npos) {
                size_t loc_end = header.find("\r\n", stat);
                location = header.substr(stat + 10, loc_end - stat - 10);
            }
        }
    }
    Response(const Response &rhs) {
        status = rhs.status;
        url = rhs.url;
        line1 = rhs.line1;
        if_cache = rhs.if_cache;
        if_nocache = rhs.if_nocache;
        if_validate = rhs.if_validate;
        expiration_time = rhs.expiration_time;
        age = rhs.age;
        last_modified = rhs.last_modified;
        etag = rhs.etag;
        content = rhs.content;
        receive = rhs.receive;
        location = rhs.location;
    }
    Response & operator = (const Response &rhs) {
        if (this != &rhs) {
            status = rhs.status;
            url = rhs.url;
            line1 = rhs.line1;
            if_cache = rhs.if_cache;
            if_nocache = rhs.if_nocache;
            if_validate = rhs.if_validate;
            expiration_time = rhs.expiration_time;
            age = rhs.age;
            last_modified = rhs.last_modified;
            etag = rhs.etag;
            content = rhs.content;
            receive = rhs.receive;
            location = rhs.location;
        }
        return *this;
    }
};

class Log {
    ofstream log_file;
public:
    Log(string log_filename){
        log_file.open(log_filename, ofstream::out | ofstream::trunc);
    }
    ~Log() {
        log_file.close();
    }
    void request_from_client(Client & client){
        time_t tmp = time(0);
        struct tm * t = localtime(&tmp);
        char buffer[80];
        memset(buffer, 0, 80);
        strftime(buffer, 80, "%a %b %d %H:%M:%S %Y", t);
        log_file_lock.lock();
        log_file << client.uid << ": \"" << client.line1 << "\" from " << client.ipaddr << " @ " << buffer << endl;
        log_file_lock.unlock();
        cout << client.uid << ": \"" << client.line1 << "\" from " << client.ipaddr << " @ " << buffer << endl;
    }
    void not_in_cache(Client & client){
        log_file_lock.lock();
        log_file << client.uid << ": not in cache" << endl;
        log_file_lock.unlock();
        cout << client.uid << ": not in cache" << endl;
    }
    void expired(Client & client, Response & response){
        struct tm * t = localtime(&response.expiration_time);
        char buffer[80];
        memset(buffer, 0, 80);
        strftime(buffer, 80, "%a %b %d %H:%M:%S %Y", t);
        log_file_lock.lock();
        log_file << client.uid << ": in cache, but expired at " << buffer << endl;
        log_file_lock.unlock();
        cout << client.uid << ": in cache, but expired at " << buffer << endl;
    }
    void validate(Client & client){
        log_file_lock.lock();
        log_file << client.uid << ": in cache, requires validation" << endl;
        log_file_lock.unlock();
        cout << client.uid << ": in cache, requires validation" << endl;
    }
    void valid(Client & client){
        log_file_lock.lock();
        log_file << client.uid << ": in cache, valid" << endl;
        cout << client.uid << ": in cache, valid" << endl;
        log_file_lock.unlock();
    }
    void re_request(Client & client){
        log_file_lock.lock();
        log_file << client.uid << ": Requesting " << client.line1 << " from " << client.host << endl;
        log_file_lock.unlock();
        cout << client.uid << ": Requesting " << client.line1 << " from " << client.host << endl;
    }
    void receive_response(Client & client, Response & response){
        log_file_lock.lock();
        log_file << client.uid << ": Received " << response.line1 << " from " << client.host << endl;
        log_file_lock.unlock();
        cout << client.uid << ": Received " << response.line1 << " from " << client.host << endl;
    }
    void not_cacheable(Client & client){
        log_file_lock.lock();
        log_file << client.uid << ": not cacheable because Either no-store or private" << endl;
        log_file_lock.unlock();
        cout << client.uid << ": not cacheable because Either no-store or private" << endl;
    }
    void expire_cache(Client & client, Response & response) {
        struct tm * t = localtime(&response.expiration_time);
        char buffer[80];
        memset(buffer, 0, 80);
        strftime(buffer, 80, "%a %b %d %H:%M:%S %Y", t);
        log_file_lock.lock();
        log_file << client.uid << ": cached, expired at " << buffer << endl;
        log_file_lock.unlock();
        cout << client.uid << ": cached, expired at " << buffer << endl;
    }
    void need_revalidate(Client & client) {
        log_file_lock.lock();
        log_file << client.uid << ": cached, but requires re-validation" << endl;
        log_file_lock.unlock();
        cout << client.uid << ": cached, but requires re-validation" << endl;
    }
    void responding(Client & client, Response & response) {
        log_file_lock.lock();
        log_file << client.uid << ": Responding " << response.line1 << endl;
        log_file_lock.unlock();
        cout << client.uid << ": Responding " << response.line1 << endl;
    }
    void responding_code(Client & client, string code) {
        log_file_lock.lock();
        log_file << client.uid << ": Responding " << code << endl;
        log_file_lock.unlock();
        cout << client.uid << ": Responding " << code << endl;
    }
    void close_tunnel(Client & client) {
        log_file_lock.lock();
        log_file << client.uid << ": Tunnel closed" << endl;
        log_file_lock.unlock();
        cout << client.uid << ": Tunnel closed" << endl;
    }
    void err_unresolvable_method(Client & client) {
        log_file_lock.lock();
        log_file << client.uid << ": ERROR Unresolvable Url" << endl;
        log_file_lock.unlock();
        cout << client.uid << ": ERROR Unresolvable Url" << endl;
    }
};

Log logfile(string("/home/hx54/568/proxy/log/proxy.log"));


// derived from leetcode LRU cache C++ solusions
class Cache {
    // mutable mutex cache_lock;
    mutable shared_timed_mutex cache_lock;
    int capacity;
    list<string> mlist;
    unordered_map<string, pair<Response, list<string>::iterator>> cache;
public:
    Cache(int cap) : capacity(cap) {}
    Response *find(string url) {
        cache_lock.lock_shared();
        unordered_map<string, pair<Response, list<string>::iterator>>::iterator it = cache.find(url);
        cache_lock.unlock_shared();
        cache_lock.lock();
        if (it == cache.end()) {
            cache_lock.unlock();
            return NULL;
        }
        move_to_front(it);
        cache_lock.unlock();
        return &(it->second.first);
    }
    void insert(string url,  Response & resp) {
        cache_lock.lock_shared();
        unordered_map<string, pair<Response, list<string>::iterator>>::iterator it = cache.find(url);
        cache_lock.unlock_shared();
        cache_lock.lock();
        if (it != cache.end()) {
            move_to_front(it);
        } else {
            if (cache.size() == capacity) {
                cache.erase(mlist.back());
                mlist.pop_back();
            }
            mlist.push_front(url);
        }
        cache[url] = pair<Response, list<string>::iterator>(resp, mlist.begin());
        cache_lock.unlock();
    }
    void move_to_front(unordered_map<string, pair<Response, list<string>::iterator>>::iterator it) {
        string key_url = it->first;
            mlist.erase(it->second.second);
            mlist.push_front(key_url);
            it->second.second = mlist.begin();
    }
    void print() {
        cache_lock.lock_shared();
        unordered_map<string, pair<Response, list<string>::iterator>>::iterator it = cache.begin();
        cout << "printing cache elements" << endl;
        for (; it != cache.end(); ++it) {
            cout << "url: " << it->first << endl;
            // cout << "first line: " << it->second.first.line1 << endl;
        }
        cache_lock.unlock_shared();
    }
};