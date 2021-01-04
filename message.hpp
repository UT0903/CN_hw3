#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
using namespace std;

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[4000];
} segment;

static void Init(int *fd, struct sockaddr_in *agent, int my_port, int his_port){
    segment s_tmp;
    struct sockaddr_in sender;
    
    /*Create UDP socket*/
    if(((*fd) = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(0);
    }

    /*Configure settings in sender struct*/
    sender.sin_family = AF_INET;
    sender.sin_port = htons(my_port);
    sender.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));
    if(bind(*fd,(struct sockaddr *)&sender,sizeof(sender)) < 0){
        perror("bind");
        exit(0);
    }

    agent->sin_family = AF_INET;
    agent->sin_port = htons(his_port);
    agent->sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(agent->sin_zero, '\0', sizeof(agent->sin_zero));
}


//syn == -1: play request
//syn == -2: video info
//syn == -3: file not found

//
vector<string> splitStr2Vec(string s, string splitSep){
    vector<string> result;
    int current = 0;
    int next = 0;
    while (next != -1){
        next = s.find_first_of(splitSep, current); 
        if (next != current){
            string tmp = s.substr(current, next - current);
            if(!tmp.empty()){
                result.push_back(tmp);
            }
        }
        current = next + 1;
    }
    return result;
} 