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
#include <time.h>
#include <netinet/in.h>
#include <thread>
#include <signal.h>
#include "message.hpp"
#define BUFF_SIZE 4000
#define MAXQUEUESIZE 10000
using namespace std;
using namespace cv;
deque <segment> buffer;
void buffering();
segment newAck(int ackNumber, int fin);
segment RecvSeg(int fd);
void SendAck(segment seg);

int socket_fd, seqNumber = 0;
struct sockaddr_in agent;

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "usage: <receiver port>  <agent port>  <filename>\n");
        exit(0);
    }

    Init(&socket_fd, &agent, atoi(argv[1]), atoi(argv[2]));
    segment seg = newAck(seqNumber++, 0);
    sprintf(seg.data, "%s\0", argv[3]);
    sendto(socket_fd, &seg, sizeof(segment), 0, (struct sockaddr *)&agent, sizeof(agent));
    int cap_height, cap_width, imgSize, ptr = 0;
    
    Mat imgClient;
    uchar *ubuf;
    while(1){
        buffering();
        //printf("buffer.size(): %u\n", buffer.size());
        for(int i = 0; i < buffer.size(); i++){
            if(buffer[i].head.syn == -1){
                //fprintf(stderr, "alloc imgClient\n");
                sscanf(buffer[i].data, "%d %d %d", &cap_height, &cap_width, &imgSize);
                //printf("imgSize: %d\n", imgSize);
                imgClient = Mat::zeros(cap_height, cap_width, CV_8UC3);
                if(!imgClient.isContinuous()){
                    imgClient = imgClient.clone();
                }
                ubuf = imgClient.data;
            }
            else if(buffer[i].head.fin == 1){
                goto END;
            }
            else{
                memcpy(ubuf + ptr, (uchar*)buffer[i].data, buffer[i].head.length);
                //printf("nowlen: %d\n", ptr);
                ptr += buffer[i].head.length;
                if(ptr == imgSize){
                    //fprintf(stderr, "imgShow\n");
                    ptr = 0;
                    imshow("Video", imgClient);
                    char c = (char)waitKey(1);
                }
            }
        }
    }
END:
    destroyAllWindows();
    return 0;
}
void buffering(){
    buffer.clear();
    while(buffer.size() < 33){
        //fprintf(stderr, "buffering\n");
        segment seg = RecvSeg(socket_fd);
        if(buffer.size() == 32){
            fprintf(stderr, "drop   data    #%d\n", seg.head.seqNumber);
            break;
        }
        if(seg.head.seqNumber == seqNumber){
            buffer.push_back(seg);
            if(seg.head.fin == 1){
                fprintf(stderr, "recv   fin\n");
                SendAck(newAck(seqNumber++, 1));
                fprintf(stderr, "send   finack\n");
                break;
            }
            else{
                fprintf(stderr, "recv   data    #%d\n", seg.head.seqNumber);
                SendAck(newAck(seqNumber++, 0));
                fprintf(stderr, "send   ack     #%d\n", seg.head.seqNumber);
            }
        }
        else{
            fprintf(stderr, "drop   data    #%d\n", seg.head.seqNumber);
            SendAck(newAck(seqNumber - 1, 0));
            fprintf(stderr, "send   ack     #%d\n", seqNumber);
        }
    }
    fprintf(stderr, "flush\n");
}
segment newAck(int ackNumber, int fin){
    segment seg;
    seg.head.ack = 1;
    seg.head.ackNumber = ackNumber;
    seg.head.fin = fin;
    return seg;
}
segment RecvSeg(int fd){
    segment seg;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_size = sizeof(tmp_addr);
    //fprintf(stderr, "start\n");
    if(recvfrom(fd, &seg, sizeof(seg), MSG_WAITALL, (struct sockaddr *)&tmp_addr, &tmp_size) < 0){
        fprintf(stderr, "error in RecvSeg\n");
        exit(0);
    }
    return seg;
}
void SendAck(segment seg){
    if(sendto(socket_fd, &seg, sizeof(segment), 0, (struct sockaddr *)&agent, sizeof(agent)) < 0){
        perror("sendto");
        exit(0);
    }
}