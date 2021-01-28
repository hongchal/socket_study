#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUF_SIZE 1000


void zombie_handler()
{
    int status;
    pid_t pid;
    pid = wait(&status);
    // printf("wait: %d\n", pid);
}

char* find_file_name(char* ptr){
    char* a = strtok(ptr, " ");
    char* b[5];
    int i=0;
    int result=0;
    
    while( a != NULL){
        // printf("%s\n", a);
        b[i] = a;          
        a = strtok(NULL, " ");
        i++;
    }

    if( !(result = strcmp(b[1], "/"))){
        b[1] = "/index.html";
    }
    
    return &b[1][1]; //for remove "/"
}

int check_content_type(char* ptr){
    char* check_point_html1;
    char* check_point_html2;
    char* check_point_jpg;
    char* check_point_png;

    check_point_html1 = strstr(ptr, "/ ");
    check_point_html2 = strstr(ptr, ".html");
    check_point_jpg = strstr(ptr, ".jpg");
    check_point_png = strstr(ptr, ".png");

    if( (check_point_html1 != NULL) || (check_point_html2 !=NULL) ){
        return 1;
    }
    else if( check_point_jpg != NULL ){
        return 2;
    }
    else if(check_point_png != NULL ){
        return 3;
    }
    else{
        return 4;
    }
}

void send_request(int sock_fd, char *fname, int type)
{
    struct stat fileinfo;
    int file = 0; //file descriptor
    char header_buf[BUF_SIZE+1];
    char data_buf[BUF_SIZE+1];
    int data_len2 = 0;

    if(type == 4){
        fname="error.html"; 
    }
    file = open(fname, O_RDONLY);
    fstat(file, &fileinfo);

    header_buf[0]='\0';

    switch (type) // make header msg 
    {
    case 1:
        sprintf(header_buf,"HTTP/1.1 200 OK\r\nContect-Type : text/html\r\nContent-Length: %ld\r\n\r\n", fileinfo.st_size);
        break;
    case 2:
        sprintf(header_buf,"HTTP/1.1 200 OK\r\nContect-Type : image/jpg\r\nContent-Length: %ld\r\n\r\n", fileinfo.st_size);
        break;
    case 3:
        sprintf(header_buf,"HTTP/1.1 200 OK\r\nContect-Type : image/png\r\nContent-Length: %ld\r\n\r\n", fileinfo.st_size);
        break;
    case 4:
        sprintf(header_buf,"HTTP/1.1 404 Not Found\r\nContect-Type : text/html\r\nContent-Length: %ld\r\n\r\n", fileinfo.st_size);
        break;
    }

    //send header msg 
    printf("=============response msg==============\n");
    printf("%s",header_buf);
    send(sock_fd, header_buf, strlen(header_buf),0); //***strlen***

    //make data buf & send data 
    while(1){
       data_len2 = read(file, data_buf, BUF_SIZE);
       
       if(data_len2 == -1){
           perror("read: ");
           exit(0);
       }
       else if(data_len2 == 0){
        //    printf("-----------1\n");
           break;
       }
       data_buf[data_len2] = 0;
       data_len2 = send(sock_fd, data_buf, BUF_SIZE,0);
       if(data_len2 == -1){
         perror("write: ");
         exit(0);
       }
    }
    close(file);
}

int main(int argc, char *argv[])
{
    int s, client_s;
    struct sockaddr_in server_addr;
    struct sockaddr_in clinet_addr;
    int addr_len;
    int data_len;
    char buf[BUF_SIZE+1];
    int i;
    int opt = 1,optlen = 4; //주소 재사용 
    int port; 

    struct sigaction action;
    action.sa_handler = zombie_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGCHLD, &action, 0);

    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( s == -1) {
        perror("socket: ");
        return 0;
    }

    int len = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, optlen);
    if(len == -1){
        perror("setsockopt: ");
        return 0;
    } // 주소 재사용 

    if(argc ==1 || argc > 2){
        printf("invalid argument length : need PORT argument\n");
        return 0;
    }

    server_addr.sin_family = PF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(server_addr.sin_zero), 0, 8); 
    addr_len = sizeof(server_addr);

    if(bind(s, (struct sockaddr *)&server_addr, addr_len)){
        perror("bind: ");
        return 0;
    }

    if(listen(s, 5) == -1){
        perror("listen: ");
        return 0;
    }
    
    printf("Waiting....\n");
    while(1){ //for infinite server 
        client_s = accept(s, (struct sockaddr *)&clinet_addr, &addr_len);
        
        if(client_s == -1){
            switch(errno){
                case EINTR:
                    // printf("Interupt\n\n");
                    continue;
                default:
                    perror("accpet: ");
            }
            return 0;
        }

        printf("client_ip=%s:%d\n", inet_ntoa(clinet_addr.sin_addr), ntohs(clinet_addr.sin_port));
        
        if(fork() == 0){
            
            close(s);//close lisenting socket for child
            
            //receive request msg 
            while(1){ 
                data_len = recv(client_s, buf, 1000,0);
                if(data_len == -1){
                    perror("recv: ");
                    return 0;
                }
                else if(data_len ==0){
                     //printf("receive request data\n");
                     break;
                }
                buf[data_len] = '\0';
                printf("=============requset msg==============\n");
                printf("recv : %s\n", buf);
                

                //analyze request msg 
                char *ptr = strtok(buf,"\r\n");//first line of request message
                int type = 0;
                type = check_content_type(ptr); //content-type 
                char * fname = find_file_name(ptr);//file-name 
                
                send_request(client_s, fname, type);
            }
            close(client_s);
            exit(0);
        }
        
        else{
            close(client_s);//close data_connecet socket for parent
        }   
    }

    close(s);
 
    return 0;
}

