#include "sysInclude.h"
#include <map>
#define CLOSED 1
#define SYN_SENT 2
#define ESTABLISHED 3
#define FIN_WAIT1 4
#define FIN_WAIT2 5
#define TIME_WAIT 6

using std::map;
using std::pair;
extern void tcp_DiscardPkt(char* pBuffer, int type);
extern void tcp_sendIpPkt(unsigned char* pData, unsigned short len, unsigned int srcAddr, unsigned int dstAddr, unsigned short ttl);
extern int waitIpPacket(char* pBuffer, int timeout);
extern unsigned int getIpv4Address();
extern unsigned int getServerIpv4Address();
int gSrcPort = 2005;
int gDstPort = 2006;
int gSeqNum = 1, gAckNum = 1, socknum = 1;
class TCB {
public:
    unsigned int srcAddr;
    unsigned int dstAddr;
    unsigned short srcPort;
    unsigned short dstPort;
    unsigned int seq;
    unsigned int ack;
    int sockfd;
    int curState;
    unsigned char* data;
    TCB()
    {
        sockfd = socknum++;
        srcPort = gSrcPort++;
        seq = gSeqNum++;
        ack = gAckNum;
        curState = CLOSED;
    }
};

class TCPHead {
public:
    unsigned short srcPort;
    unsigned short destPort;
    unsigned seqNo;
    unsigned ackNo;
    unsigned short headLen;
    unsigned short flag;
    unsigned short windowsize;
    unsigned short checksum;
    unsigned short urgentPointer;
    char data[100];
    void NtoH();
    unsigned CalChecksum(unsigned int srcAddr, unsigned int dstAddr, int type, int len);
};
void TCPHead::NtoH()
{
    checksum = ntohs(checksum);
    srcPort = ntohs(srcPort);
    destPort = ntohs(destPort);
    seqNo = ntohl(seqNo);
    ackNo = ntohl(ackNo);
    windowsize = ntohs(windowsize);
    urgentPointer = ntohs(urgentPointer);
}
unsigned TCPHead::CalChecksum(unsigned int srcAddr, unsigned int dstAddr, int type, int len)
{
    unsigned int sum = 0;
    sum += srcPort + destPort;
    sum += (seqNo >> 16) + (seqNo & 0xFFFF);
    sum += (ackNo >> 16) + (ackNo & 0xFFFF);
    sum += (headLen << 8) + flag;
    sum += windowsize + urgentPointer;
    sum += (srcAddr >> 16) + (srcAddr & 0xffff);
    sum += (dstAddr >> 16) + (dstAddr & 0xffff);
    sum += IPPROTO_TCP;
    sum += 0x14;
    if (type == 1) {
        sum += len;
        for (int i = 0; i < len; i += 2)
            sum += (data[i] << 8) + (data[i + 1] & 0xFF);
    }
    sum += (sum >> 16);
    return (~sum) & 0xFFFF;
}
map<int, TCB*> TCBTable;
TCB* tcb;
int stud_tcp_input(char* pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr)
{
    srcAddr = ntohl(srcAddr);
    dstAddr = ntohl(dstAddr);
    TCPHead* head = (TCPHead*)pBuffer;
    head->NtoH();
    if (head->CalChecksum(srcAddr, dstAddr, 0, 0) != head->checksum)
        return -1;
    if (head->ackNo != tcb->seq + (tcb->curState != FIN_WAIT2)) {
        tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
        return -1;
    }
    tcb->ack = head->seqNo + 1;
    tcb->seq = head->ackNo;
    if (tcb->curState == SYN_SENT) {
        tcb->curState = ESTABLISHED;
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, DEFAULT_TCP_SRC_PORT, DEFAULT_TCP_DST_PORT, getIpv4Address(), getServerIpv4Address());
    } else if (tcb->curState == FIN_WAIT1) {
        tcb->curState = FIN_WAIT2;
    } else if (tcb->curState == FIN_WAIT2) {
        tcb->curState = TIME_WAIT;
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, DEFAULT_TCP_SRC_PORT, DEFAULT_TCP_DST_PORT, getIpv4Address(), getServerIpv4Address());
    } else
        return -1;
    return 0;
}
void stud_tcp_output(char* pData, unsigned short len, unsigned char flag, unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr)
{
    if (tcb == NULL) {
        tcb = new TCB();
    }
    TCPHead* head = new TCPHead();
    memcpy(head->data, pData, len);
    head->srcPort = srcPort;
    head->destPort = dstPort;
    head->seqNo = tcb->seq;
    head->ackNo = tcb->ack;
    head->headLen = 0x50;
    head->flag = flag;
    head->windowsize = 1;
    head->checksum = head->CalChecksum(srcAddr, dstAddr, (flag == PACKET_TYPE_DATA), len);
    head->NtoH();
    tcp_sendIpPkt((unsigned char*)head, 20 + len, srcAddr, dstAddr, 60);
    if (flag == PACKET_TYPE_SYN && tcb->curState == CLOSED)
        tcb->curState = SYN_SENT;
    if (flag == PACKET_TYPE_FIN_ACK && tcb->curState == ESTABLISHED)
        tcb->curState = FIN_WAIT1;
}
int stud_tcp_socket(int domain, int type, int protocol)
{
    tcb = new TCB();
    TCBTable.insert(std::pair<int, TCB*>(tcb->sockfd, tcb));
    return tcb->sockfd;
}
int stud_tcp_connect(int sockfd, struct sockaddr_in* addr, int addrlen)
{
    int res = 0;
    map<int, TCB*>::iterator iter = TCBTable.find(sockfd);

    tcb = iter->second;
    tcb->dstPort = ntohs(addr->sin_port);
    tcb->curState = SYN_SENT;
    tcb->srcAddr = getIpv4Address();
    tcb->dstAddr = htonl(addr->sin_addr.s_addr);

    stud_tcp_output(NULL, 0, PACKET_TYPE_SYN, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    TCPHead* r = new TCPHead();
    do {
        res = waitIpPacket((char*)r, 5000);
    } while (res == -1);
    if (r->flag == PACKET_TYPE_SYN_ACK) {
        tcb->ack = ntohl(r->seqNo) + 1;
        tcb->seq = ntohl(r->ackNo);
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
        tcb->curState = ESTABLISHED;
        return 0;
    }
    return -1;
}
int stud_tcp_send(int sockfd, const unsigned char* pData, unsigned short datalen, int flags)
{
    int res = 0;
    map<int, TCB*>::iterator iter = TCBTable.find(sockfd);
    tcb = iter->second;
    if (tcb->curState == ESTABLISHED) {
        tcb->data = (unsigned char*)pData;
        stud_tcp_output((char*)tcb->data, datalen, PACKET_TYPE_DATA, tcb->srcPort, tcb->dstPort, getIpv4Address(), tcb->dstAddr);
        TCPHead* r = new TCPHead();
        do {
            res = waitIpPacket((char*)r, 5000);
        } while (res == -1);
        if (r->flag == PACKET_TYPE_ACK) {
            if (ntohl(r->ackNo) != (tcb->seq + datalen)) {
                tcp_DiscardPkt((char*)r, STUD_TCP_TEST_SEQNO_ERROR);
                return -1;
            }
            tcb->ack = ntohl(r->seqNo) + datalen;
            tcb->seq = ntohl(r->ackNo);
            return 0;
        }
    }
    return -1;
}
int stud_tcp_recv(int sockfd, unsigned char* pData, unsigned short datalen, int flags)
{
    int res = 0;
    map<int, TCB*>::iterator iter = TCBTable.find(sockfd);
    tcb = iter->second;
    if (tcb->curState == ESTABLISHED) {
        TCPHead* r = new TCPHead();
        do {
            res = waitIpPacket((char*)r, 5000);
        } while (res == -1);
        memcpy(pData, r->data, sizeof(r->data));
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, getIpv4Address(), tcb->dstAddr);
        return 0;
    }
    return -1;
}
int stud_tcp_close(int sockfd)
{
    int res = 0;
    map<int, TCB*>::iterator iter = TCBTable.find(sockfd);
    tcb = iter->second;
    if (tcb->curState == ESTABLISHED) {
        stud_tcp_output(NULL, 0, PACKET_TYPE_FIN_ACK, tcb->srcPort, tcb->dstPort, getIpv4Address(), tcb->dstAddr);
        tcb->curState = FIN_WAIT1;
        TCPHead* r = new TCPHead();
        do {
            res = waitIpPacket((char*)r, 5000);
        } while (res == -1);
        if (r->flag == PACKET_TYPE_ACK) {
            tcb->curState = FIN_WAIT2;
            do {
                res = waitIpPacket((char*)r, 5000);
            } while (res == -1);
            if (r->flag == PACKET_TYPE_FIN_ACK) {
                tcb->ack = ntohl(r->seqNo);
                tcb->seq = ntohl(r->ackNo);
                tcb->ack++;
                stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, getIpv4Address(), tcb->dstAddr);
                tcb->curState = TIME_WAIT;
                return 0;
            }
        }
        return -1;
    }
    delete tcb;
    return -1;
}