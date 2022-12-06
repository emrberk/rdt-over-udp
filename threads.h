#ifndef THREADS_H_
#define THREADS_H_

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
#include <vector>
#include <mutex>
#include <semaphore>
#include <deque>
#include <utility>
#include <climits>
#include "packet.h"

#define MAX_MSG_SIZE 12
#define PACKET_SIZE 16
#define MAX_BUFFER 1024
#define WINDOW_SIZE 5
#define MAX_SEQUENCE 86

std::vector<Packet*> packetCells(MAX_SEQUENCE);

std::vector<std::timed_mutex> mutexCells(MAX_SEQUENCE);

int availableSequenceNumber;

std::vector<std::thread*> ackListenerThreads;

int baseIndex = 0;

int windowIndex = 0;

struct sockaddr* clientAddress;

void sender(int sockfd) {
    int sequenceNumber = baseIndex + windowIndex;

    while (true) {
        mutexCells[sequenceNumber].lock();
        char* raw = new char[PACKET_SIZE];
        void* rawPacket = packetCells[sequenceNumber]->getRawPacket(raw);
        int numBytes;

        if ((numBytes = sendto(sockfd, rawPacket, PACKET_SIZE, 0, clientAddress, INET_ADDRSTRLEN)) == -1) {
            perror("Error on sendto: ");
            return;
        }
        delete[] raw;
        delete packetCells[sequenceNumber];
        sequenceNumber = sequenceNumber == MAX_SEQUENCE - 1 ? 0 : sequenceNumber + 1;
    }
}

void listener(int sockfd) {
    int numbytes;
    socklen_t addressLength = sizeof(*clientAddress);
    char s[INET_ADDRSTRLEN];
    std::vector<std::string>* overallMessage = new std::vector<std::string>(1024 / MAX_MSG_SIZE);
    int numReceived = 0;
    int expectedPackets = 0;
    int minSequenceNumber = 0;

    while (true) {
        char* buf = new char[PACKET_SIZE];
        if ((numbytes = recvfrom(sockfd, buf, PACKET_SIZE , 0,
            (struct sockaddr *) clientAddress, &addressLength)) == -1) {
            perror("Error on recvfrom");
            exit(-1);
        }
        Packet* receivedPacketPtr = new Packet(buf);
        Packet receivedPacket = *receivedPacketPtr;

        unsigned short sequenceNumber = receivedPacket.getSequenceNumber();
        if (receivedPacket.isACKPacket()) {
            if (receivedPacket.getIsEnd()) {
                Packet* ptr = new Packet(buf);
                Packet p = *ptr;
                expectedPackets = std::stoi(p.getData());
                minSequenceNumber = p.getSequenceNumber() + 1 == MAX_SEQUENCE ? 0 : p.getSequenceNumber() + 1;
                Packet* ackPacketPtr = new Packet(sequenceNumber, true);
                Packet ackPacket = *ackPacketPtr;
                char* raw = new char[PACKET_SIZE];
                void* rawPacket = ackPacket.getRawPacket(raw);
                int n;
                if ((n = sendto(sockfd, rawPacket, PACKET_SIZE, 0, clientAddress, addressLength)) == -1) {
                    perror("Error ack back:(");
                    return;
                }

                if (numReceived == expectedPackets) {
                    int cnt = 0;
                    unsigned short currentSequence = minSequenceNumber;
                    while (cnt < expectedPackets) {
                        if (overallMessage->at(currentSequence).size()) {
                            std::cout << overallMessage->at(currentSequence);
                        }
                        currentSequence = currentSequence + 1 == MAX_SEQUENCE ? 0 : currentSequence + 1;
                        cnt++;
                    }
                    for (int i = 0; i < 1024 / MAX_MSG_SIZE; i++) {
                        overallMessage->at(i) = "";
                    }
                    std::cout << std::endl;
                    numReceived = 0;
                    expectedPackets = 0;
                    fflush(stdout);
                }
                delete ackPacketPtr;
                delete ptr;
                delete[] raw;
                continue;
            }
            bool acquired = mutexCells[sequenceNumber].try_lock();
            if (acquired) { // Duplicate ACK
                mutexCells[sequenceNumber].unlock();
            } else { // A thread is waiting for ACK
                mutexCells[sequenceNumber].unlock();
            }
        } else {
            Packet* ackPacketPtr = new Packet(sequenceNumber, true);
            Packet ackPacket = *ackPacketPtr;
            char* raw = new char[PACKET_SIZE];
            void* rawPacket = ackPacket.getRawPacket(raw);
            int n;
            if ((n = sendto(sockfd, rawPacket, PACKET_SIZE, 0, clientAddress, addressLength)) == -1) {
                perror("Error ack back:(");
                return;
            }
            delete[] raw;
            delete ackPacketPtr;
            numReceived++;
            overallMessage->at(sequenceNumber) = receivedPacket.getData();
            if (numReceived == expectedPackets) {
                int cnt = 0;
                unsigned short currentSequence = minSequenceNumber;
                while (cnt < expectedPackets) {
                    if (overallMessage->at(currentSequence).size()) {
                        std::cout << overallMessage->at(currentSequence);
                    }
                    currentSequence = currentSequence + 1 == MAX_SEQUENCE ? 0 : currentSequence + 1;
                    cnt++;
                }
                for (int i = 0; i < 1024 / MAX_MSG_SIZE; i++) {
                    overallMessage->at(i) = "";
                }
                std::cout << std::endl;
                numReceived = 0;
                expectedPackets = 0;
                fflush(stdout);
            }
        }
        delete receivedPacketPtr;
        delete[] buf;
    }
    delete overallMessage;
}

#endif