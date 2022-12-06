#include <cstdio>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <climits>
#include "threads.h"

#define SERVERPORT "4950"    // the port users will be connecting to

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char** argv)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int addressInfo;
    int numbytes;

    std::cout << argc << argv[0] << argv[1] << std::endl;

    if (argc != 2) {
        fprintf(stderr,"Provide hostname\n");
        exit(1);
    }

    char* destinationAddress = argv[1];

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((addressInfo = getaddrinfo(destinationAddress, SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addressInfo));
        return -1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        /*
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        */

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    for (int i = 0; i < WINDOW_SIZE; i++) {
        retVals[i] = INIT;
    }

    for (auto& mutex : mutexCells) {
        mutex.lock();
    }

    baseIndex = 0;
    availableSequenceNumber = 0;

    clientAddress = new sockaddr;
    clientAddress = p->ai_addr;
    char* dummy = new char[16];
    int numBytes;
    dummy = "INITIAL";
    if ((numBytes = sendto(sockfd, dummy, PACKET_SIZE, 0, p->ai_addr, INET_ADDRSTRLEN)) == -1) {
        perror("Error on sendto: ");
        exit(-1);
    }

    std::thread listenerThread(listener, sockfd);

    for (int i = 0; i < WINDOW_SIZE; i++) {
        std::thread* senderThread = new std::thread(sender, sockfd, i, retVals + i, p);
        senderThreads.push_back(senderThread);
    }

    while (true) {
        std::string input;
        std::getline(std::cin, input);
        std::vector<std::string> chunks;
        std::string chunk;

        int inputSize = input.size();

        for (auto& c : input) {
            chunk.push_back(c);
            if (chunk.size() == MAX_MSG_SIZE) {
                chunks.push_back(chunk);
                chunk.erase();
            }
        }
        if (chunk.size()) {
            chunks.push_back(chunk);
        }
        int sequenceNumber = availableSequenceNumber;
        Packet p = Packet(sequenceNumber, chunks.size(), true);
        packetCells[sequenceNumber] = p;
        availableSequenceNumber = availableSequenceNumber == MAX_SEQUENCE - 1 ? 0 : availableSequenceNumber + 1;
        mutexCells[sequenceNumber].unlock();

        std::cout << "input is " << input << "len = " << inputSize << " and #chunks = " << chunks.size() << std::endl;
        for (int i = 0; i < chunks.size(); i++) {
            sequenceNumber = availableSequenceNumber;
            std::string chunk = chunks[i];
            bool end = i == chunks.size() - 1;
            Packet p = Packet(sequenceNumber, chunk, end);
            if (end) {
                std::cout << "end chunk #" << i << " and content = " << chunk << std::endl;
            }
            packetCells[sequenceNumber] = p;
            availableSequenceNumber = availableSequenceNumber == MAX_SEQUENCE - 1 ? 0 : availableSequenceNumber + 1;
            mutexCells[sequenceNumber].unlock();
        }
    }

    listenerThread.join();

    for (auto& thread : senderThreads) {
        thread->join();
    }

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}