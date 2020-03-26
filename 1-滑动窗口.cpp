#include "sysinclude.h"
#include <iostream>
#include <cstdio>
#include <list>
using namespace std;

extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4


typedef enum {data,ack,nak} frame_kind;

typedef struct frame_head{
    frame_kind kind;        //类型  
    unsigned int seq;       //序列号
    unsigned int ack;       //确认号
    unsigned char data[100];
};

typedef struct frame{
    frame_head head;        //帧头
    unsigned int size;      //数据大小
};  

typedef struct packFrame{   //包装Frame,包装的内容方便使用
    frame* pFrame;
    unsigned size;
    unsigned int seq;
};

packFrame waitToConfirm[WINDOW_SIZE_STOP_WAIT]; //发送了，但尚未收到ack的
list<packFrame> waitToSend; //缓冲区,尚未来得及发送
int lower = 0,upper = 0;
/*
* 停等协议测试函数
*/
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType){
    frame* p = (frame*)pBuffer;
    unsigned int currentSeq = ntohl(p->head.seq);
    unsigned int currentAck = ntohl(p->head.ack);
    frame_kind currentKind =(frame_kind)ntohl(p->head.kind);
    
    if(messageType == MSG_TYPE_TIMEOUT){
        currentSeq = *(unsigned int*)pBuffer;
    }
    switch(messageType){
        case MSG_TYPE_SEND:
            printf("-----MSG_TYPE_SEND\n");
            break;
        case MSG_TYPE_RECEIVE:
            printf("-----MSG_TYPE_RECEIVE\n");
            break;
        case MSG_TYPE_TIMEOUT:
            printf("-----MSG_TYPE_TIMEOUT\n");
            break;
    }
    printf("My Test Information\n");
    printf("currentSeq = %d currentAck = %d\n",currentSeq,currentAck);
    switch(messageType){
        case MSG_TYPE_SEND:
            packFrame currentPackFrame;     //提取传入的信息,打包
            currentPackFrame.pFrame = new frame();
            *(currentPackFrame.pFrame) = *p;
            currentPackFrame.size = (unsigned int)bufferSize;
            currentPackFrame.seq = currentSeq;

            if(upper - lower < WINDOW_SIZE_STOP_WAIT){  //可以发送  
                waitToConfirm[currentSeq % WINDOW_SIZE_STOP_WAIT] = currentPackFrame; //保存当前信息，以备之后检查或重发
                upper++;    //窗口向前进
                SendFRAMEPacket((unsigned char*)pBuffer,(unsigned int)bufferSize);
                return 0;
            }else{  //没有空间发送，保存
                waitToSend.push_back(currentPackFrame);
                return 0;
            }
            printf("wrong 1\n");
            break;
        
        case MSG_TYPE_RECEIVE:
            if(currentKind == ack){ //确认信息
                if(currentAck != lower + 1){
                    printf("wrong 2\n");
                    return 1;
                }
                lower++;    //窗口缩减1
                if(!waitToSend.empty() && upper-lower < WINDOW_SIZE_BACK_N_FRAME){  //用if是因为只有一个大小,不可能连续发送
                    packFrame currentPackFrame = waitToSend.front();
                    waitToSend.pop_front();
                    waitToConfirm[currentPackFrame.seq % WINDOW_SIZE_STOP_WAIT] = currentPackFrame;
                    SendFRAMEPacket((unsigned char*)currentPackFrame.pFrame,currentPackFrame.size);
                    upper++;
                }
                return 0;
            }else if(currentKind == nak){   //否定
                SendFRAMEPacket((unsigned char*)waitToConfirm[currentAck % WINDOW_SIZE_STOP_WAIT].pFrame,       //重新发送即可
                    waitToConfirm[currentAck % WINDOW_SIZE_STOP_WAIT].size);
                return 0;
            }
            printf("wrong 3\n");
            break;
        case MSG_TYPE_TIMEOUT:
            if(currentSeq != lower + 1){        
                printf("wrong 4\n");
                return 1;
            }
            SendFRAMEPacket((unsigned char*)waitToConfirm[currentAck % WINDOW_SIZE_STOP_WAIT].pFrame,       //重新发送即可
                    waitToConfirm[currentAck % WINDOW_SIZE_STOP_WAIT].size);
            return 0;
            break;
        default:
            printf("wrong 5\n");
            break;
    }
   
	return 0;
}


/*
* 回退n帧测试函数
*/
packFrame waitToConfirm1[WINDOW_SIZE_BACK_N_FRAME];
list<packFrame> waitToSend1;
int lower1 = 0, upper1 = 0;
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType){
    frame* p = (frame*)pBuffer;
    unsigned int currentSeq = ntohl(p->head.seq);
    unsigned int currentAck = ntohl(p->head.ack);
    frame_kind currentKind =(frame_kind)ntohl(p->head.kind);
    
    if(messageType == MSG_TYPE_TIMEOUT){
        currentSeq = *(unsigned int*)pBuffer;
    }
    switch(messageType){
        case MSG_TYPE_SEND:
            printf("-----MSG_TYPE_SEND\n");
            break;
        case MSG_TYPE_RECEIVE:
            printf("-----MSG_TYPE_RECEIVE\n");
            break;
        case MSG_TYPE_TIMEOUT:
            printf("-----MSG_TYPE_TIMEOUT\n");
            break;
    }
    printf("My Test Information\n");
    printf("currentSeq = %d currentAck = %d\n",currentSeq,currentAck);
    
    switch(messageType){
        case MSG_TYPE_SEND: //send的情况应该完全和之前相同
            packFrame currentPackFrame;
            currentPackFrame.pFrame = new frame();
            *currentPackFrame.pFrame = *p;
            currentPackFrame.size = (unsigned int)bufferSize;
            currentPackFrame.seq = currentSeq;

            if(upper1 - lower1 < WINDOW_SIZE_BACK_N_FRAME){  //可以发送  
                waitToConfirm1[currentSeq % WINDOW_SIZE_BACK_N_FRAME] = currentPackFrame; //保存当前信息，以备之后检查或重发
                upper1++;
                SendFRAMEPacket((unsigned char*)pBuffer,(unsigned int)bufferSize);
                printf("lower = %d,upper = %d\n",lower1,upper1);
                return 0;
            }else{  //没有空间发送，保存
                printf("no place to send\n");
                waitToSend1.push_back(currentPackFrame);
                printf("lower = %d,upper = %d\n",lower1,upper1);
                return 0;
            }
            printf("wrong 1\n");
            break;
        
        case MSG_TYPE_RECEIVE:
            if(currentAck<=lower1 || currentAck>upper1){
                printf("wrong 2\n");
                return 1;
            }
            //我不确定是不是会出现 较大seq先确认而较小seq后确认的情况。没办法解决那种情况
            /*if(currentAck == lower + 1){
                lower++;
                while(!waitToSend.empty() && upper-lower<WINDOW_SIZE_BACK_N_FRAME){ //但其实如果不会出现大seq先确认，小seq后确认的情况的话，那就不会连续发送
                    packFrame currentPackFrame = waitToSend.front();
                    waitToSend.pop_front();
                    waitToConfirm1[currentPackFrame.seq % WINDOW_SIZE_BACK_N_FRAME] = currentPackFrame;
                    SendFRAMEPacket((unsigned char*)currentPackFrame.pFrame,currentPackFrame.size);
                    upper++;
                }
                return 0;
            }*/
            if(currentKind == ack){
                lower1 = currentAck;
                while(!waitToSend1.empty() && upper-lower<WINDOW_SIZE_BACK_N_FRAME){ //但其实如果不会出现大seq先确认，小seq后确认的情况的话，那就不会连续发送
                    packFrame currentPackFrame = waitToSend1.front();
                    waitToSend1.pop_front();
                    waitToConfirm1[currentPackFrame.seq % WINDOW_SIZE_BACK_N_FRAME] = currentPackFrame;
                    SendFRAMEPacket((unsigned char*)currentPackFrame.pFrame,currentPackFrame.size);
                    upper1++;
                }
            }else if(currentKind == nak){
                for(int i = lower1 + 1;i<=upper1;++i){
                //printf("%d\n",waitToConfirm1[i%WINDOW_SIZE_BACK_N_FRAME].seq);
                    SendFRAMEPacket((unsigned char*)waitToConfirm1[i%WINDOW_SIZE_BACK_N_FRAME].pFrame,
                        waitToConfirm1[i%WINDOW_SIZE_BACK_N_FRAME].size);
                }
            }
            printf("lower = %d,upper = %d\n",lower1,upper1);
            break;
        case MSG_TYPE_TIMEOUT:
           /* if(currentSeq != lower + 1){
                printf("wrong 4\n");
                return 1;
            }*/
            for(int i = lower1 + 1;i<=upper1;++i){
                //printf("%d\n",waitToConfirm1[i%WINDOW_SIZE_BACK_N_FRAME].seq);
                SendFRAMEPacket((unsigned char*)waitToConfirm1[i%WINDOW_SIZE_BACK_N_FRAME].pFrame,
                    waitToConfirm1[i%WINDOW_SIZE_BACK_N_FRAME].size);
            }
            printf("lower = %d,upper = %d\n",lower1,upper1);
            return 0;
            break;
        default:
            printf("wrong 5\n");
            break;
    }
   
	return 0;
}

/*
* 选择性重传测试函数
*/

packFrame waitToConfirm2[WINDOW_SIZE_BACK_N_FRAME];
list<packFrame> waitToSend2;
int lower2 = 0, upper2 = 0;
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType){
    frame* p = (frame*)pBuffer;
    unsigned int currentSeq = ntohl(p->head.seq);
    unsigned int currentAck = ntohl(p->head.ack);
    frame_kind currentKind =(frame_kind)ntohl(p->head.kind);
    
    if(messageType == MSG_TYPE_TIMEOUT){
        currentSeq = *(unsigned int*)pBuffer;
    }
    switch(messageType){
        case MSG_TYPE_SEND:
            printf("-----MSG_TYPE_SEND\n");
            break;
        case MSG_TYPE_RECEIVE:
            printf("-----MSG_TYPE_RECEIVE\n");
            break;
        case MSG_TYPE_TIMEOUT:
            printf("-----MSG_TYPE_TIMEOUT\n");
            break;
    }
    printf("My Test Information\n");
    printf("currentSeq = %d currentAck = %d\n",currentSeq,currentAck);
    if(currentKind == nak){
        printf("this is nak\n");
    }
    switch(messageType){
        case MSG_TYPE_SEND: //send的情况应该完全和之前相同
            packFrame currentPackFrame;
            currentPackFrame.pFrame = new frame();
            *currentPackFrame.pFrame = *p;
            currentPackFrame.size = (unsigned int)bufferSize;
            currentPackFrame.seq = currentSeq;

            if(upper2 - lower2 < WINDOW_SIZE_BACK_N_FRAME){  //可以发送  
                waitToConfirm2[currentSeq % WINDOW_SIZE_BACK_N_FRAME] = currentPackFrame; //保存当前信息，以备之后检查或重发
                upper2++;
                SendFRAMEPacket((unsigned char*)pBuffer,(unsigned int)bufferSize);
                printf("lower = %d,upper = %d\n",lower2,upper2);
                return 0;
            }else{  //没有空间发送，保存
                printf("no place to send\n");
                waitToSend1.push_back(currentPackFrame);
                printf("lower = %d,upper = %d\n",lower2,upper2);
                return 0;
            }
            printf("wrong 1\n");
            break;
        
        case MSG_TYPE_RECEIVE:
            if(currentAck<=lower2 || currentAck>upper2){
                printf("wrong 2\n");
                return 1;
            }
            if(currentKind == ack){
                lower2 = currentAck;
                while(!waitToSend1.empty() && upper-lower<WINDOW_SIZE_BACK_N_FRAME){ //但其实如果不会出现大seq先确认，小seq后确认的情况的话，那就不会连续发送
                        packFrame currentPackFrame = waitToSend1.front();
                        waitToSend1.pop_front();
                        waitToConfirm2[currentPackFrame.seq % WINDOW_SIZE_BACK_N_FRAME] = currentPackFrame;
                        SendFRAMEPacket((unsigned char*)currentPackFrame.pFrame,currentPackFrame.size);
                        upper2++;
                    }
                
                printf("lower = %d,upper = %d\n",lower2,upper2);
            }
            else if(currentKind == nak){
                SendFRAMEPacket((unsigned char*)waitToConfirm2[currentAck%WINDOW_SIZE_BACK_N_FRAME].pFrame,
                    waitToConfirm2[currentAck%WINDOW_SIZE_BACK_N_FRAME].size);
            }
            break;
        case MSG_TYPE_TIMEOUT:
            SendFRAMEPacket((unsigned char*)waitToConfirm2[currentSeq%WINDOW_SIZE_BACK_N_FRAME].pFrame,
                waitToConfirm2[currentSeq%WINDOW_SIZE_BACK_N_FRAME].size);

            printf("lower = %d,upper = %d\n",lower2,upper2);
            return 0;
            break;
        default:
            printf("wrong 5\n");
            break;
    }
   
	return 0;
}