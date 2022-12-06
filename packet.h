#ifndef PACKET_H_
#define PACKET_H_

#include <string>
#include <iostream>
#include <climits>

#define MAX_MSG_SIZE 12

class Packet {
    std::string data; // 12 Bytes
    bool isACK; // 1 Byte
    unsigned short sequenceNumber; // 2 Bytes
    bool end; // 1 Byte
    bool isSent;
public:
    Packet() {
        sequenceNumber = 0;
        data = "";
        isACK = false;
        end = false;
        isSent = true;
    }
    Packet(unsigned short sequenceNumber, std::string data, bool end) {
        this->sequenceNumber = sequenceNumber;
        for (auto& c : data) {
            this->data.push_back(c);
        }
        this->isACK = false;
        this->end = end;
        this->isSent = false;
    }
    ~Packet() {
        this->data.erase();
    }

    Packet(unsigned short sequenceNumber, bool isACK) {
        this->sequenceNumber = sequenceNumber;
        this->isACK = isACK;
        this->data = "";
        end = false;
        isSent = false;
    }

    Packet(void* rawPacket) {
        char* bytes = (char*) rawPacket;

        bool* ackStart = (bool*) (bytes + 12);
        unsigned short* seqStart = (unsigned short*) (bytes + 13);
        bool* endFlagStart = (bool*) (bytes + 15);

        for (int i = 0; i < MAX_MSG_SIZE; i++) {
            data.push_back(bytes[i]);
        }

        this->isACK = *ackStart;

        this->sequenceNumber = *seqStart;

        this->end = *endFlagStart;
    }

    Packet(unsigned short sequenceNumber, int numPackets, bool isInfo) {
        this->sequenceNumber = sequenceNumber;
        data = std::to_string(numPackets);
        isACK = true;
        end = true;
    }

    void* getRawPacket(char* rawPacket) {
        for (int i = 0; i < 12; i++) {
            rawPacket[i] = this->data[i];
        }

        bool* ack = (bool*) (rawPacket + 12);
        *ack = this->isACK;

        unsigned short* seq = (unsigned short*) (rawPacket + 13);
        *seq = sequenceNumber;

        bool* endByte = (bool*) (rawPacket + 15);
        *endByte = end;
        
        return (void*) rawPacket;
    }

    bool isACKPacket() {
        return this->isACK;
    }

    std::string getData() {
        return this->data;
    }

    int getSequenceNumber() {
        return sequenceNumber;
    }
    
    bool getIsEnd() {
        return end;
    }

    void markSent() {
        isSent = true;
    }

    bool getIsSent() {
        return isSent;
    }
};

#endif