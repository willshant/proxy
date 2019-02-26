
proxy: proxy.cpp server.hpp client.hpp
	g++ -std=c++14 -pthread -o proxy -g proxy.cpp