
proxy: proxy.cpp server.hpp client.hpp
	g++ -std=c++11 -pthread -o proxy -g proxy.cpp