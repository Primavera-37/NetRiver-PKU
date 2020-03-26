/*
* THIS FILE IS FOR IP TEST
*/
// system support
#include "sysInclude.h"
#include <iostream>
#include <cstdio>
using namespace std;

extern void ip_DiscardPkt(char* pBuffer, int type);

extern void ip_SendtoLower(char* pBuffer, int length);

extern void ip_SendtoUp(char* pBuffer, int length);

extern unsigned int getIpv4Address();

// implemented by students
//用于在接受时进行头检验和检查,返回值为是否合法
bool headCheck(unsigned short* pBuffer, unsigned short length)
{
    unsigned int sum = 0;
    unsigned short len = length;
    while (len > 1) {
        sum += *pBuffer++;
        len -= sizeof(unsigned short);
    }
    if (len) {
        sum += *pBuffer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    return sum == 0xffff;
}
int stud_ip_recv(char* pBuffer, unsigned short length)
{
    //判断版本
    int version;
    version = pBuffer[0] >> 4;
    if (version != 4) { //实验中只处理IPv4的情况
        printf("Wrong Reason:Wrong Version\n");
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    }

    //检验头部长度，不能小于20
    //原数据是以四字节字为单位
    unsigned int headLength = (unsigned int)(pBuffer[0] & 0x0f) * 4;
    if (headLength < 20) {
        printf("Wrong Reason:Wrong Headlength\n");
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
    }

    //检验生存期
    //若小于等于零说明应当丢弃
    unsigned int timeToLive = (unsigned int)(pBuffer[8]);
    if (timeToLive <= 0) {
        printf("Wrong Reason:Wrong TimetoLive\n");
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    }

    //检验头校验和
    //调用实现的接收时检查头校验和的函数
    if (!headCheck((unsigned short*)pBuffer, length)) {
        printf("Wrong Reason:Wrong HeadCheck\n");
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }

    //检查分组
    //注意这里是网络序
    unsigned destAddr = ((unsigned*)pBuffer)[4];
    destAddr = ntohl(destAddr);
    if (destAddr != getIpv4Address() || destAddr == 0xffffffff) {
        printf("Wrong Reason:Wrong Address\n");
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }
    pBuffer += headLength;  //将头部丢掉,只传输数据部分
    ip_SendtoUp(pBuffer, length - headLength);
    return 0;
}

//发送时进行头校验和部分的计算
unsigned calHeadCheck(unsigned short* pBuffer)
{
    unsigned int sum = 0;
    int i = 0;
    while (i < 10) {
        sum += (unsigned int)pBuffer[i];
        i++;
    }
    if (sum >> 16) { //如果高位还有数字
        sum = (sum >> 16) + (sum & 0xffff);
    }
    return ~sum; //取反
}

int stud_ip_Upsend(char* pBuffer, unsigned short len, unsigned int srcAddr,
    unsigned int dstAddr, byte protocol, byte ttl)
{
    //发送时将所有头部都固定为20字节
    char* packet = (char*)malloc(20 + len);
    unsigned short totalLen = 20 + len;
    memcpy(packet + 20, pBuffer, len);

    packet[0] = 0x45;   //Version为4,头部长度为5(四字节)
    packet[1] = 0x00;   //Type of service不涉及
    packet[3] = totalLen & 0xff;    //注意,这里是大端法,高地址存放低位数据
    packet[2] = totalLen >> 8;

    ((unsigned int*)packet)[1] = 0; //不涉及的部分

    packet[8] = ttl;    //time to live,输入参数长度即为1Byte
    packet[9] = protocol;   //同上

    ((unsigned short*)packet)[5] = 0;   //初始化检验和部分,赋值为0

    ((unsigned int*)packet)[3] = htonl(srcAddr);    //传入为主机序,需要转换为网络序
    ((unsigned int*)packet)[4] = htonl(dstAddr);

    unsigned short checkSum = calHeadCheck((unsigned short*)packet);    //对头部进行校验和的计算
    ((unsigned short*)packet)[5] = checkSum;

    ip_SendtoLower(packet, totalLen);
    return 0;
}