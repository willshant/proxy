#include <string>
#include <vector>

using namespace std;

static size_t init_uid=0;

class Client {
public:
    size_t uid;
    string method;
    string line1;
    string host;
    vector<char> body;
    Client(string & target):body(target.begin(), target.end()) {
        uid = init_uid;
        init_uid++;
        size_t pos = target.find(' ');
        method = target.substr(0, pos);
        pos = target.find('\n');
        line1 = target.substr(0, pos);
        pos = target.find("Host: ");
        size_t space = target.find(' ', pos);
        size_t enter = target.find('\n', space);
        host = target.substr(space+1, enter-space-2);
    }

};