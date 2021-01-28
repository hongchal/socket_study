#include<stdio.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_MES_LEN  1000 //65536//2^16
#define MAX_NAME_LEN 10

int num_client = 0;
pthread_mutex_t max_client_num;
pthread_mutex_t list_lock;

//message struct 
typedef struct message {
    uint16_t len;
    char type;
    char msg[MAX_MES_LEN];
}message;

//client information struct 
typedef struct client_info{
    int port_num;
    int sock_num;
    char name[MAX_NAME_LEN];
    char *ip;

}client_info;

//client list 
typedef struct client_list{
    struct client_list *next;
    struct client_list *prev;
    struct client_info data;
} client_list;

client_list *latest; //pointer for client_list added latest
client_list *head; //client_list head
client_list *list_pointer; //pointer for find special client_list
client_list *temp; //temp is the pointer that pointing socket which send message to server

//receiver thread 
void *thread_main(void *arg){
    int s = *(int *)arg;
    int socket_pointer;
    int size;
    char recv_data_buf[MAX_MES_LEN - MAX_NAME_LEN -2]; //because 'name'+':'+' '+'recv_data_buf' = 1000 
    
    message recv_msg;
    message send_msg;

    free(arg);
    
    while(1){
         if((size = recv(s, (message *)&recv_msg, sizeof(message), 0)) > 0){  
            //change message length big endian to little endian
            recv_msg.len = ntohs(recv_msg.len);
            
            //receive connect message 
            if(recv_msg.type == 'c'){
                pthread_mutex_lock(&list_lock);//client_list is shared resource
                list_pointer = head; //set list_pointer to head 
                
                //this for loop find client_list(client) which send message
                for(int i=0 ; i<num_client ; i++){ 
                    list_pointer = list_pointer->next;  
                    if((list_pointer->data.sock_num == s) && (list_pointer != head)){
                        //copy client name to name in client_info (MAX NAME LEN is 10)
                        strncpy(list_pointer->data.name ,recv_msg.msg, recv_msg.len);
                        list_pointer->data.name[recv_msg.len] = '\0'; //recv_msg.len is changed when while start
                        printf("%s Connected\n", list_pointer->data.name);
                        printf("------------------------------\n");
                    }
                }
                pthread_mutex_unlock(&list_lock);
            }
            
            //when recv message type is 'm'
            else if(recv_msg.type == 'm'){
                pthread_mutex_lock(&list_lock);
                //find socket which send message 
                list_pointer = head;

                //this for loop is find socket which send message 
                for(int i=0 ; i<num_client ; i++){
                    list_pointer = list_pointer->next;
                    if((list_pointer->data.sock_num == s) && (list_pointer != head)){
                        temp = list_pointer; //find client_list for client_name 
                    }
                }

                //copy recv message to recv_buf and add '\0'
                //this is make send message for all clinet  
                strncpy(recv_data_buf ,recv_msg.msg, recv_msg.len);
                recv_data_buf[recv_msg.len] = '\0'; //recv_msg.len is already changed when while start
                printf("Sender: %s\n", temp->data.name); //temp is the client_list which send message to server
                printf("Recv : %s\n", recv_data_buf);
                //make send_msg like (client name : message)
                sprintf(send_msg.msg,"%s: %s", temp->data.name, recv_data_buf); //name + recv_data_buf = 1000 (MAX MES LEN)
                
                send_msg.len = htons(strlen(send_msg.msg));
                send_msg.type = 'm';
                // printf("%u\n", send_msg.len);

                list_pointer = head;
                //send message for all client
                for(int i=0 ; i<num_client ; i++){
                    list_pointer = list_pointer->next;
                    socket_pointer = list_pointer->data.sock_num;
                    send(socket_pointer, (message *)&send_msg, sizeof(message), 0);
                }
                pthread_mutex_unlock(&list_lock);
                printf("------------------------------\n");
            }

            else if(recv_msg.type=='q'){
                //remove disconnected client in list 
                pthread_mutex_lock(&list_lock);
                list_pointer = head;
                //find client that send /q message using for loop
                for(int i=0 ; i<num_client ; i++){
                    list_pointer = list_pointer->next;
                    if((list_pointer->data.sock_num == s) && (list_pointer != head)){
                        // if the client_list is last client_list 
                        if(list_pointer->next == NULL){ 
                            //if the client_list is last one 
                            if((list_pointer->prev == head) && (num_client == 1)){
                                // printf("1\n");
                                list_pointer->prev->next = NULL; //head->next = null 
                                latest = head;
                            }
                            else{
                            // printf("2\n");
                            list_pointer->prev->next = NULL;
                            latest = list_pointer->prev;
                            }
                        }
                        else{
                            // printf("3\n");
                            list_pointer->prev->next = list_pointer->next;   
                            list_pointer->next->prev = list_pointer->prev; 
                        }
                        //free for memory
                        free(list_pointer);
                        break;                   
                    }
                }
                pthread_mutex_unlock(&list_lock);
            
                pthread_mutex_lock(&max_client_num);
                num_client--;
                pthread_mutex_unlock(&max_client_num);
                
                printf("------------------------------\n");
                printf("Disconnected:\n");
                printf("disconnected sock_d : %d\n", s);
                printf("remain client number : %d\n", num_client);
                printf("------------------------------\n");
                break; //break while
            }
            else{
                // printf("why??\n");
            }
        }
        else{
            break;   
        }
    }
    close(s);
    return NULL;
}

int main(int argc, char *argv[])
{
    int sockfd, newfd;
    struct sockaddr_in my_addr, their_addr;
    unsigned int sin_size, len_inet;
    int size;
    pthread_t t_id;
    int *arg;
    
    pthread_mutex_init(&max_client_num, NULL);
    pthread_mutex_init(&list_lock, NULL);

    //check params     
    if(argc <2){
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        return 0;
    }

    //make listening socket 
    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    my_addr.sin_family = PF_INET;
    my_addr.sin_port = htons(atoi(argv[1]));
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(my_addr.sin_zero), 0,0);
    
    if(bind(sockfd,(struct sockaddr *)&my_addr, sizeof(struct sockaddr_in))){
        perror("bind: ");
        return 0; 
    }

    if(listen(sockfd, 5) < 0){
        perror("listen: ");
        return 0;
    }

    sin_size = sizeof(struct sockaddr_in);
    
    //make client list 
    pthread_mutex_lock(&list_lock); 
    //mallock head and set head 
    head = malloc(sizeof(struct client_list)); 
    head->prev = NULL;
    head->next = NULL;
    latest = head;
    pthread_mutex_unlock(&list_lock);

    printf("Waiting...\n");

    while(1){
        //make data_socket;
        newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if(newfd < 0){
            perror("accept: ");
            return 0;
        }
        printf("------------------------------\n");
        printf("Connected: %s %d\n", inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
        
        //make new client data when client connected 
        client_info* new_data= malloc(sizeof(struct client_info));
        new_data->port_num = ntohs(their_addr.sin_port);
        new_data->sock_num = newfd;
        new_data->name[0] = 0;
        new_data->ip = inet_ntoa(their_addr.sin_addr);
        
        //make new client 
        client_list *new = malloc(sizeof(struct client_list));  
        
        pthread_mutex_lock(&list_lock);
        //linked new client
        new->next = NULL;
        new->prev = latest; //latest is the last client_list before make new 
        latest->next = new;
        latest = new; //update latest client_list to new 
        new->data = *new_data; 
        free(new_data); //free for memory 
        pthread_mutex_unlock(&list_lock);

        //add num_clinet
        pthread_mutex_lock(&max_client_num);
        num_client++;
        pthread_mutex_unlock(&max_client_num);

        printf("connected: %d\n", num_client);
        printf("socket_d: %d\n", newfd);
        printf("------------------------------\n");

        arg = (int *)malloc(sizeof(int));
        *arg = newfd;
        
        //make new client_list(and client_info) for new client and make thread for send and recv message
        pthread_create(&t_id, NULL, thread_main, arg);
        pthread_detach(t_id);
    }

       
    close(sockfd);
    return 0;
}
