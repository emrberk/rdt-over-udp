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
#define MAX_SEQUENCE 9

std::vector<Packet> packetCells(MAX_SEQUENCE);

std::vector<std::timed_mutex> mutexCells(MAX_SEQUENCE);

std::mutex goBackMutex;

int backTo;

int availableSequenceNumber;

std::vector<std::thread*> senderThreads;

typedef enum threadRetval { TIMED_OUT, OK, INIT, ERROR } threadRetval;

threadRetval retVals[WINDOW_SIZE];

int baseIndex;

std::timed_mutex acked[WINDOW_SIZE];

struct sockaddr* clientAddressGlobal;

struct sockaddr* clientAddress;

bool first = true;

int getIndexOfNext(int order) {
    int remaining = MAX_SEQUENCE - 1 - (baseIndex + order - 1);
    if (remaining >= 0) {
        return baseIndex + WINDOW_SIZE + order;
    }
    return -(remaining);
}

void sender(int sockfd, int windowIndex, threadRetval* ret, addrinfo* p) {
    // std::cout << "sender init with index = " << windowIndex << std::endl;
    int sequenceNumber = windowIndex;

    while (true) {
        std::cout << "my seq = " << sequenceNumber << " and w = " << windowIndex << std::endl;
        // std::cout << "sender seq no = " << sequenceNumber << std::endl;
        mutexCells[sequenceNumber].lock();
        std::cout << "cell lock successful in w = " << windowIndex << " seq = " << sequenceNumber << std::endl;
        char* raw = new char[PACKET_SIZE];
        void* rawPacket = packetCells[sequenceNumber].getRawPacket(raw);
        // std::cout << "in cell data was = " << packetCells[sequenceNumber].getData() << std::endl;
        int numBytes;

        addrinfo* address = p != nullptr ? p : (addrinfo*) clientAddressGlobal;

        // std::cout << "client addres set." << std::endl;
        // std::cout << "p was full ? " << (p != nullptr) << std::endl;
        // std::cout << "raw pack is = " << (char*) rawPacket << std::endl;

        if ((numBytes = sendto(sockfd, rawPacket, PACKET_SIZE, 0, clientAddress, INET_ADDRSTRLEN)) == -1) {
            *ret = ERROR;
            perror("Error on sendto: ");
            return;
        }
        delete[] raw;
        std::cout << "no error sendto." << std::endl;

        packetCells[sequenceNumber].markSent();
        
        bool isAcked = mutexCells[sequenceNumber].try_lock_for(std::chrono::milliseconds(500));
        std::cout << "ACK in seq = " << sequenceNumber << " " <<isAcked << std::endl;
        if (isAcked) { // 4 + 5 - (9 + 1)
            sequenceNumber = WINDOW_SIZE + sequenceNumber >= MAX_SEQUENCE ? WINDOW_SIZE + sequenceNumber - (MAX_SEQUENCE) : WINDOW_SIZE + sequenceNumber;
        } else if (!isAcked) {
            *ret = TIMED_OUT;
        }
        // unlock cells outside
    }
}

void listener(int sockfd) {
    int numbytes;
    socklen_t addressLength = sizeof(*clientAddress);
    char s[INET_ADDRSTRLEN];
    std::vector<std::string>* overallMessage = new std::vector<std::string>(1024 / MAX_MSG_SIZE);
    int numReceived = 0;
    int expectedPackets = 0;

    while (true) {
        char* buf = new char[PACKET_SIZE];
        //std::cout << "listener is working... " << std::endl;
        if ((numbytes = recvfrom(sockfd, buf, PACKET_SIZE , 0,
            (struct sockaddr *) clientAddress, &addressLength)) == -1) {
            perror("Error on recvfrom");
            exit(-1);
        }        
        void* voidBuf = (void*) buf;
        Packet receivedPacket(buf);
        std::cout << "Received " << receivedPacket.getData() << std::endl;
        std::cout << "Received ack ? " << (receivedPacket.isACKPacket() && !receivedPacket.getIsEnd()) << std::endl;
        std::cout << "Received info ? " << (receivedPacket.isACKPacket() && receivedPacket.getIsEnd()) << std::endl;
        // std::cout << "seq no = " << receivedPacket.getSequenceNumber() << std::endl;
        // std::cout << "data = " << receivedPacket.getData() << std::endl;
        unsigned short sequenceNumber = receivedPacket.getSequenceNumber();
        if (receivedPacket.isACKPacket()) {
            if (receivedPacket.getIsEnd()) {
                Packet p(buf);
                expectedPackets = std::stoi(p.getData());
                std::cout << "Expected packets = " << expectedPackets << std::endl;
                Packet ackPacket(sequenceNumber, true);
                char* raw = new char[PACKET_SIZE];
                void* rawPacket = ackPacket.getRawPacket(raw);
                int n;
                if ((n = sendto(sockfd, rawPacket, PACKET_SIZE, 0, clientAddress, addressLength)) == -1) {
                    perror("Error ack back:(");
                    return;
                }

                if (numReceived == expectedPackets) {
                    numReceived = 0;
                    expectedPackets = 0;
                    std::cout << "END --" << std::endl;
                    for (int i = 0; i < overallMessage->size(); i++) {
                        if (overallMessage->at(i).size()) {
                            std::cout << overallMessage->at(i);
                        }
                        overallMessage->at(i) = "";
                    }
                    std::cout << std::endl;
                    fflush(stdout);
                    std::cout << "-- END --" << std::endl;
                }
                delete[] raw;
                continue;
            }
            std::cout << "ack packet! seq = " << sequenceNumber << std::endl;
            bool acquired = mutexCells[sequenceNumber].try_lock();
            if (acquired) { // Duplicate ACK
                std::cout << "Duplicate ACK!" << std::endl;
                backTo = sequenceNumber == MAX_SEQUENCE - 1 ? 0 : sequenceNumber + 1;
                mutexCells[sequenceNumber].unlock();
                goBackMutex.unlock();
            } else { // A thread is waiting for ACK
                if (sequenceNumber == baseIndex) {
                    baseIndex++;
                }
                std::cout << "ACK is handled!" << std::endl;
                mutexCells[sequenceNumber].unlock();
            }
        } else {
            std::cout << "normal packet seq = " << sequenceNumber << std::endl;
            Packet ackPacket(sequenceNumber, true);
            char* raw = new char[PACKET_SIZE];
            void* rawPacket = ackPacket.getRawPacket(raw);
            int n;
            if ((n = sendto(sockfd, rawPacket, PACKET_SIZE, 0, clientAddress, addressLength)) == -1) {
                perror("Error ack back:(");
                return;
            }
            delete[] raw;
            std::cout << "i sent ack packet seq = " << sequenceNumber << std::endl;
            numReceived++;
            std::cout << "numreceived = " << numReceived << " expected =" << expectedPackets << std::endl;

            overallMessage->at(sequenceNumber) = receivedPacket.getData();
            bool isEndPacket = receivedPacket.getIsEnd();
            if (numReceived == expectedPackets) {
                numReceived = 0;
                expectedPackets = 0;
                std::cout << "END --" << std::endl;
                for (int i = 0; i < overallMessage->size(); i++) {
                    if (overallMessage->at(i).size()) {
                        std::cout << overallMessage->at(i);
                    }
                    overallMessage->at(i) = "";
                }
                std::cout << std::endl;
                fflush(stdout);
                std::cout << "-- END --" << std::endl;
            }
        }
        delete[] buf;
    }
}

#endif