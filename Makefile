all:
	client server

client: 
	g++ -g client.cpp -std=c++11 -o client && ./client 172.24.0.10

server:
	g++ -g server.cpp -std=c++11 -o server && ./server 172.24.0.20

clear:
	rm -f server && rm -f client
