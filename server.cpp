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
#include <thread>
#include <climits>
#include "threads.h"

#define MYPORT "4950"    // the port users will be connecting to

// get sockaddr, IPv4 or IPv6:
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
    int rv;
    char* clientAddressStr = argv[1];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    for (int i = 0; i < WINDOW_SIZE; i++) {
        retVals[i] = INIT;
    }

    for (auto& mutex : mutexCells) {
        mutex.lock();
    }
    availableSequenceNumber = 0;
    baseIndex = 0;

    clientAddress = new sockaddr;
    // To set client address global
    int numbytes;
    socklen_t addressLength = sizeof(*clientAddress);
    char* buf = new char[PACKET_SIZE];
    char s[INET_ADDRSTRLEN];
    if ((numbytes = recvfrom(sockfd, buf, PACKET_SIZE , 0,
        (struct sockaddr *) clientAddress, &addressLength)) == -1) {
        perror("Error on recvfrom");
        exit(-1);
    }
    std::thread listenerThread(listener, sockfd);

    for (int i = 0; i < WINDOW_SIZE; i++) {
        std::thread* senderThread = new std::thread(sender, sockfd, i, retVals + i, nullptr);
        senderThreads.push_back(senderThread);
    }

    while (true) {
        std::string input;
        std::getline(std::cin, input);
        std::string chunk;
        std::vector<std::string> chunks;

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
        Packet p = Packet(sequenceNumber, (int) chunks.size(), true);
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