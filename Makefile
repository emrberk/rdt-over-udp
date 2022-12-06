all:
	g++ -g client.cpp -std=c++11 -o client && g++ -g server.cpp -std=c++11 -o server

clear:
	rm -f server && rm -f client