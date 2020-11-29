#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_GROUPS 100

struct node{
    int client;
    struct node* next;
};

char** split(char*  str,int split_count);
struct node* create_node(int client_id);
struct node* remove_node(struct node* head ,int client);
struct node* add_node(struct node* head , int client);
void send_message_to_group(struct node* head , int sender  ,char* message );
void* handle_client(void* pclient_socket);
int create_server(struct sockaddr_in* paddress);
void check(int val , char* message);
struct sockaddr_in * get_address(char* ip_address);
void init(char * ip_address,struct sockaddr_in* address);
void create_service(int socket);
void join_to_group(char** message , int client_socket);
void send_to_group(char** message , int client_socket);
void leave_from_group(char** message , int client_socket);
void quit_from_server(int client_socket);
void handle_bad_request(int client_socket);


struct node** groups;

pthread_mutex_t lock;

int main(int argc, char const *argv[]) {
    char * ip_address =  malloc(sizeof(char)*strlen(argv[1]));
    strcpy(ip_address , argv[1]);
    groups = (struct node**) malloc(sizeof(struct node*)*BUFFER_SIZE);
    int server_fd;
    struct sockaddr_in address = *(get_address(ip_address)); 
    init(ip_address , &address);
    server_fd = create_server(&address);
    int addrlen = sizeof(address);
    fd_set current_socket , ready_socket;
    FD_ZERO(&current_socket);
    FD_SET(server_fd , &current_socket);
    while(1){
        ready_socket = current_socket;
        int client_socket;
        check(select(FD_SETSIZE , &ready_socket , NULL , NULL , NULL) , "select\n");
        for(int i = 0; i<FD_SETSIZE;i++){
            if(FD_ISSET(i ,&ready_socket)){
                if ( i == server_fd){
                    check((client_socket = accept(server_fd, (struct sockaddr *)&address,&addrlen)) , "not accepted\n");
                    FD_SET(client_socket , &current_socket);
                    printf("Hello client %s:%d:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port), client_socket);
                }else{
                    create_service(i);
                    FD_CLR(i,&current_socket);
                }
            }
        }
    }
    return 0;
}


void quit_from_server(int client_socket){
    for(int i=0 ;i < MAX_GROUPS;i++){
        groups[i] = remove_node(groups[i] , client_socket);
    }
    char * message = "successfully removed";
    check(send(client_socket , message , strlen(message) , 0) , "send");
    close(client_socket);
}


void create_service(int socket){
    int * pclient_socket = (int*)malloc(sizeof(int));
    *pclient_socket = socket;
    pthread_t thread;
    pthread_create(&thread , NULL , handle_client , pclient_socket);
}


void init(char* ip_address , struct sockaddr_in* address){
    if (pthread_mutex_init(&lock, NULL) != 0) { 
            printf("\n mutex init has failed\n"); 
            exit(EXIT_FAILURE); 
    }

        
    printf("%s\n" , ip_address);
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    
    printf("host name : %s\n" , hostname);
    
    printf("Listen on %s:%d\n", inet_ntoa(address->sin_addr), ntohs(address->sin_port));
    

}


int create_server(struct sockaddr_in* paddress){
    struct sockaddr_in address = *paddress;
    int server_fd=0;
    if ((server_fd =socket(AF_INET, SOCK_STREAM, 0)),"socket failed" ); 
    int addrlen = sizeof(address);
    check(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) , "bind");
    check(listen(server_fd, 1000) ,"listen");
    return server_fd;
}


void check(int val , char* message){
    if(val < 0){
        perror(message);
        exit(EXIT_FAILURE);
    }
}


struct sockaddr_in * get_address(char* ip_address){
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_address);
    address.sin_port = htons(PORT);
    struct sockaddr_in * paddress = &address;
    return paddress;    
}


void* handle_client(void* pclient_socket){
    int client_socket = *((int*) pclient_socket);
    while (1)
    {
        char buffer[BUFFER_SIZE] = {0};
        check(read(client_socket, buffer, BUFFER_SIZE),"read");
        char ** message = split(buffer,3);
        printf("(s = %d) %s\n", client_socket, buffer);
        pthread_mutex_lock(&lock);
        if(strcmp(message[0],"join")==0){
            join_to_group(message , client_socket);
        }else if (strcmp(message[0],"send")==0){
            send_to_group(message , client_socket);
        }else if (strcmp(message[0],"leave")==0){
            leave_from_group(message , client_socket);
        }else if (strcmp(message[0],"quit")==0)
        {
            quit_from_server(client_socket);
        }else{
            handle_bad_request(client_socket);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
} 


void handle_bad_request(int client_socket){
    int group_id;
    char* to_client;
    to_client = "bad request";
    check(send(client_socket, to_client, strlen(to_client), 0),"send");
}


void join_to_group(char** message , int client_socket){
    int group_id;
    char* to_client = (char *)malloc(sizeof(char)*BUFFER_SIZE);
    sscanf(message[1] , "%d" , &group_id);
    if(group_id >MAX_GROUPS){
        to_client = "not a valid group id";
        check(send(client_socket, to_client, strlen(to_client), 0), "send");
    }else
    {
        groups[group_id] = add_node(groups[group_id] , client_socket);
        printf("group %d members: \n" , group_id);
        struct node * curr = groups[group_id];
        while (curr)
        {
            printf("member %d\n",curr->client);
            curr=  curr->next;
        }
        
        sprintf(to_client , "you have added to %d" , group_id);
        check(send(client_socket, to_client, strlen(to_client), 0),"send");
    }

}


void send_to_group(char** message , int client_socket){
    int group_id;
    char* to_client;
    sscanf(message[1] , "%d" , &group_id);
    if(group_id >MAX_GROUPS){
        to_client = "not a valid group id";
        check(send(client_socket, to_client, strlen(to_client), 0),"send");
    }else{
        printf("***%s\n" , message[2]);
        send_message_to_group(groups[group_id] , client_socket , message[2]);
    }
}


void leave_from_group(char** message , int client_socket){
    int group_id;
    char* to_client;
    sscanf(message[1] , "%d" , &group_id);
    if(group_id >MAX_GROUPS){
        to_client = "not a valid group id";
        check(send(client_socket, to_client, strlen(to_client), 0),"send");
    }else{
        remove_node(groups[group_id] , client_socket);
    }
}


void send_message_to_group(struct node* head , int sender  ,char* message ){
    struct node* tmp = head;
    int is_member = 0;
    while (tmp){
        if(tmp->client == sender)
            is_member = 1;
        tmp = tmp->next;
    }
    if(!is_member){
        char * to_client = "you are not a member of this group";        
        check(send(sender, to_client, strlen(to_client), 0),"send");
    }
    tmp = head;
    while (tmp){
        if (!(tmp->client == sender))
            check(send(tmp->client, message, strlen(message), 0),"send"); 
        tmp = tmp->next;
    }          

}


char** split(char*  str ,int split_count){
    int length = strlen(str);
    char ** splited; 
    splited = (char**) malloc(sizeof(char*)*3);
    for(int i = 0 ; i < split_count;i++){
        splited[i] = (char *)malloc(sizeof(char)*length*2);
    }
    int k = 0;
    int p = 0;
    for (int i = 0; i < length; i++) { 
        if ((str[i] == ' '|| i == 0)&& k < split_count-1) {
            int f= 0;
            int j= i > 0 ? i+1: 0;
            for(;str[j] != ' '&& str[j]!='\n';j++,f++)
                splited[k][f] = str[j];
            i=j-1;
            k++;
        }else if (k == split_count-1){
            splited[split_count-1][p] = str[i];
            p++;
        }
    }
    return splited;
}


struct node* create_node(int client_id){
    struct node* n_node = (struct node*)malloc(sizeof(struct node));
    n_node->client = client_id;
    return n_node;
}


struct node* add_node(struct node* head , int client){
    struct node* current = head;
    while (current){
        if(current->client == client){
            return head;
        }
        current = current->next;
    }
    struct node* nod = create_node(client);
    nod->next = head;
    head = nod;
    return head;
}


struct node* remove_node(struct node* head ,int client){
    struct node* current = head;
    struct node* prev = NULL;
    if (current != NULL && current->client == client){
        head = current->next;
        free(current);
        return head;
    }
    while (current)
    {
        if(current->client == client){
            prev->next = current->next;
            free(current);
            return head;
        }
        prev = current;
        current = current->next;
    }
    return head;   
}