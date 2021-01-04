#include "opencv2/opencv.hpp"
#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <assert.h>
#include <time.h>
#include <algorithm>
#include <netinet/in.h>
#include "message.hpp"
#define BUFF_SIZE 4000
using namespace std;
using namespace cv;

int socket_fd;
struct sockaddr_in agent;
deque <segment> sendingQueue;
string svr_path = "./server_folder/";
segment NewSeg(int seqNumber, int fin, const char* data, int length);
void ReliableSend(segment seg);
int ReliableRecv(int size);
void AdjustWinSize(int type);
void TriggerSend(int size);
void TriggerReSend(int size);
segment RecvRequest(int fd);

int windowSize = 1, thresize = 2;
int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "usage: <server port>  <agent port>\n");
        exit(0);
    }
    Init(&socket_fd, &agent, atoi(argv[1]), atoi(argv[2]));
    
    segment seg = RecvRequest(socket_fd);
    //fprintf(stderr, "seg.data: >%s<\n", seg.data);
    VideoCapture cap(string(svr_path + string(seg.data)).c_str());
    int cap_height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    int cap_width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int frame_count = cap.get(CV_CAP_PROP_FRAME_COUNT);
    Mat imgServer = Mat::zeros(cap_height, cap_width, CV_8UC3);
    if(!imgServer.isContinuous()){
        imgServer = imgServer.clone();
    }
    int imgSize = imgServer.total() * imgServer.elemSize();
    //send info about video
    string info = string(to_string(cap_height) + " " + to_string(cap_width) + " " + to_string(imgSize) + " ").c_str();
    int seqNumber = 1;
    segment video_info = NewSeg(seqNumber++, 0, info.c_str(), strlen(info.c_str()));
    video_info.head.syn = -1;
    struct timeval tv;
    tv.tv_sec = 0;
	tv.tv_usec = 100000;
	setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));  // set timeout
    ReliableSend(video_info);
    while(1){
        cap >> imgServer;
        if(imgServer.empty()){
            ReliableSend(NewSeg(seqNumber++, 1, NULL, 0));
            break;
        }
        for(int k = 0; k < imgSize; k += BUFF_SIZE){
            ReliableSend(NewSeg(seqNumber++, 0, (const char*)imgServer.data + k, min(BUFF_SIZE, imgSize - k)));
        }
    }
    cap.release();
    //fprintf(stderr, "finish!!!\n");
}
segment NewSeg(int seqNumber, int fin, const char* data, int length){
    segment seg;
    seg.head.ack = 0;
    seg.head.syn = 0;
    seg.head.seqNumber = seqNumber;
    seg.head.fin = fin;
    memset(&seg.data, '\0', sizeof(char) * BUFF_SIZE);
    memcpy(seg.data, data, length);
    seg.head.length = length;
    return seg;
}
void ReliableSend(segment seg){
    //fprintf(stderr, "in ReliableSend\n");
    sendingQueue.push_back(seg);
    if(sendingQueue.size() >= windowSize || seg.head.fin == 1){ //trigger send
        if(seg.head.fin == 1){
            while(sendingQueue.size() > 0){
                TriggerSend(min(windowSize, (int)sendingQueue.size()));
                while(ReliableRecv(min(windowSize, (int)sendingQueue.size())) != 0){
                    AdjustWinSize(-1);
                    TriggerReSend(min(windowSize, (int)sendingQueue.size()));
                }
                AdjustWinSize(1);
            }
            //fprintf(stderr, "fin!!!\n");
        }
        else{
            while(sendingQueue.size() >= windowSize){
                TriggerSend(windowSize);
                while(ReliableRecv(windowSize) != 0){
                    AdjustWinSize(-1);
                    TriggerReSend(windowSize);
                }
                AdjustWinSize(1);
            }
        }
    }
}
int ReliableRecv(int size){
    //fprintf(stderr, "in ReliableRecv\n");
    assert(size <= sendingQueue.size());
    segment seg;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_size = sizeof(tmp_addr);
    while(size--){
        if(recvfrom(socket_fd, &seg, sizeof(seg), MSG_WAITALL, (struct sockaddr *)&tmp_addr, &tmp_size) < 0){
            //fprintf(stderr, "recv no data\n");
            return -1;
        }
        if(seg.head.ackNumber >= sendingQueue[0].head.seqNumber){
            if(seg.head.fin == 0){
                fprintf(stderr, "recv   ack     #%d\n", seg.head.ackNumber);
            }
            else{
                fprintf(stderr, "recv   finack\n");
            }
            sendingQueue.pop_front();
        }
    }
    return 0;
}
void AdjustWinSize(int type){
    if(type == 1){
        if(windowSize < thresize){
            windowSize = windowSize * 2;
        }
        else{
            windowSize++;
        }
    }
    else if(type == -1){
        thresize = max(windowSize / 2, 1);
        windowSize = 1;
    }
}
void TriggerSend(int size){
    assert(size <= sendingQueue.size());
    //fprintf(stderr, "Resend from %d to %d\n", sendingQueue[0].head.seqNumber, sendingQueue[sendingQueue.size()- 1].head.seqNumber);
    for(int i = 0; i < size; i++){
        sendto(socket_fd, &sendingQueue[i], sizeof(segment), 0, (struct sockaddr *)&agent, sizeof(agent));
        if(sendingQueue[i].head.fin == 0){
            fprintf(stderr, "send   data    #%d,    winSize = %d\n", sendingQueue[i].head.seqNumber, windowSize);
        }
        else{
            fprintf(stderr, "send   fin\n");
        }
    }
}
void TriggerReSend(int size){
    assert(size <= sendingQueue.size());
    //fprintf(stderr, "Resend from %d to %d\n", sendingQueue[0].head.seqNumber, sendingQueue[sendingQueue.size()- 1].head.seqNumber);
    for(int i = 0; i < size; i++){
        sendto(socket_fd, &sendingQueue[i], sizeof(segment), 0, (struct sockaddr *)&agent, sizeof(agent));
        if(sendingQueue[i].head.fin == 0){
            fprintf(stderr, "Resnd   data    #%d,    winSize = %d\n", sendingQueue[i].head.seqNumber, windowSize);
        }
        else{
            fprintf(stderr, "Resnd   fin\n");
        }
    }
}
segment RecvRequest(int fd){
    segment seg;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_size = sizeof(tmp_addr);
    //fprintf(stderr, "start\n");
    if(recvfrom(fd, &seg, sizeof(seg), MSG_WAITALL, (struct sockaddr *)&tmp_addr, &tmp_size) < 0){
        fprintf(stderr, "error in RecvRequest\n");
        exit(0);
    }
    return seg;
}