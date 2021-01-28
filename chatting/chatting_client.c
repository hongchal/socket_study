#include<stdio.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_MES_LEN 1000 

//message struct 
typedef struct message {
    uint16_t len;
    char type;
    char msg[MAX_MES_LEN];
}message;

//message send thread 
void *thread_send(void *arg){
    int sockfd = *(int *)arg;
    int size;
    char send_buf[MAX_MES_LEN];
    message send_msg;

    while(1){
        //read data from keyboard and store to send_buf
        size = read(0, send_buf, MAX_MES_LEN);
        if(size < 0){
            perror("name read error: ");
            exit(1);
        } 
        
        //make empty buf for copy 
        memset(send_msg.msg, 0, sizeof(send_msg.msg)); 
        //copy  send_buf to send_msg.msg except '\n'
        strncpy(send_msg.msg, send_buf, size-1);
        send_msg.len = size-1;
        if(send_msg.len < 0) {
            perror("connect_msg read error: ");
            exit(1);
        }
        send_msg.len = htons(send_msg.len);
        send_msg.type = 'm';
        
        //when /q message => quit
        if(strncmp(send_buf, "/q", 2)==0){
            send_msg.len = 0;
            send_msg.type = 'q';
            memset(send_msg.msg, 0, sizeof(send_msg.msg));
            //send quit msg 
            send(sockfd, (message *)&send_msg, sizeof(message), 0);
            //close write buffer 
            shutdown(sockfd, SHUT_WR);
            break;
        }
        else{
            //send chatting msg
            send(sockfd, (message *)&send_msg, sizeof(message), 0);
        }
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in my_addr, their_addr;
    unsigned int sin_size, len_inet;
    int size, name_len;
    char buf[MAX_MES_LEN];
    message connect_msg;
    message recv_msg;
    pthread_t t_id;


    //check params 
    if(argc <4){
        fprintf(stderr, "Usage: %s <IP> <PORT> <NAME>\n", argv[0]);
        return 0;
    }
    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    //make socket
    my_addr.sin_family = PF_INET;
    my_addr.sin_port = htons(atoi(argv[2]));
    my_addr.sin_addr.s_addr = inet_addr(argv[1]);
    memset(&(my_addr.sin_zero), 0,0);
    
    sin_size = sizeof(struct sockaddr_in);

    if(connect(sockfd, (struct sockaddr*)&my_addr,sin_size) < 0){
        perror("connect: ");
        exit(1);
    }   

    //for connect message 
    connect_msg.len = strlen(argv[3]);
    // strncpy(connect_msg.msg, argv[3], connect_msg.len);
    sprintf(connect_msg.msg, "%s", argv[3]);
    if(connect_msg.len < 0){
            perror("connect_msg read error: ");
            exit(1);
    }
    connect_msg.len = htons(connect_msg.len);
    connect_msg.type = 'c';
    //send connect msg
    send(sockfd, (message *)&connect_msg, sizeof(message), 0);

    printf("Connected. (Enter \"/q\" to quit)\n");
    printf("Name: %s\n", argv[3]);
    fflush(stdout);
    //insert name to name array 
        
    //make message send thread and detach
    pthread_create(&t_id, NULL, thread_send, &sockfd);
    pthread_detach(t_id);

    while(1){
        memset(buf, 0, sizeof(buf));
        size = recv(sockfd, (message *)&recv_msg, sizeof(message), 0);
        if(size <= 0) break; 
        // strncpy(buf, recv_msg.msg, recv_msg.len); 
        sprintf(buf, "%s", recv_msg.msg);
        buf[ntohs(recv_msg.len)] = '\0';
        // printf("%u\n", recv_msg.len);
        // printf("%u\n", ntohs(recv_msg.len));
        
        //printf message 
        if(buf[0] != '\0'){
            printf("%s\n", buf);
        }
    }

    //close socket
    close(sockfd);
    return 0;
}
