/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <map>
using namespace std;

// system support
extern void fwd_LocalRcv(char* pBuffer, int length);

extern void fwd_SendtoLower(char* pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char* pBuffer, int type);

extern unsigned int getIpv4Address();

// implemented by students

unsigned calCheckSum(unsigned short* pBuffer)   //计算检验和，直接沿用第二次的代码
{
    unsigned int sum = 0;
    int i = 0;
    while (i < 10) {
        sum += (unsigned int)pBuffer[i];
        i++;
    }
    if (sum >> 16) {
        sum = (sum >> 16) + (sum & 0xffff);
    }
    return ~sum;
}

map<unsigned, stud_route_msg> myRouteTable; //用map结构来存储，根据IP地址来查询

void stud_Route_Init()
{
    printf("Initialize\n");
    return;
}

void stud_route_add(stud_route_msg* proute)
{
    unsigned int thisDest = ntohl(proute->dest);    //取出数据包的目的地址，需要注意字节序的转换
    unsigned int thisNexthop = ntohl(proute->nexthop);  //下一条的地址
    map<unsigned, stud_route_msg>::iterator iter = myRouteTable.find(thisDest); //首先检查是否存在，若存在要进行删除后重新创建
    if (iter != myRouteTable.end()) {   //若存在，则删除
        myRouteTable.erase(iter);
    }
    myRouteTable.insert(pair<unsigned, stud_route_msg>(thisDest, *proute)); //新增条目
    printf("Dest = %x,Nexthop = %x\n", thisDest, thisNexthop);
    return;
}

int stud_fwd_deal(char* pBuffer, int length)
{
    unsigned int localIpv4 = getIpv4Address();
    unsigned int targetIpv4 = ntohl(((unsigned int*)pBuffer)[4]);   //取出目的地址，转换为主机序
    printf("targetIpv4 = %x\n", targetIpv4);
    if (localIpv4 == targetIpv4) {  //本机即为接收的主机，调用系统函数进行接收
        printf("Should be received by this computer\n");
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }
    map<unsigned, stud_route_msg>::iterator iter = myRouteTable.find(targetIpv4);//查找路由表
    if (iter == myRouteTable.end()) {   //没有找到对应条目，出错返回
        printf("cannot find next hop\n");
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
        return 1;
    }
    //能到这里都是需要转发的
    printf("next hop = %x\n", iter->second);
    unsigned int nextIpv4 = iter->second.nexthop;
    ((unsigned short*)pBuffer)[5] = 0; //清除校验和
    pBuffer[8] -= 1; //ttl要减去1，ttl只占一个字节，不需要进行字节序的转换
    unsigned short checkSum = calCheckSum((unsigned short*)pBuffer);    //调用检验和的计算函数
    ((unsigned short*)pBuffer)[5] = checkSum;
    printf("packed successfully\n");
    fwd_SendtoLower(pBuffer, length, nextIpv4);
    return 0;
}