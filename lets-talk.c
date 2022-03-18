#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/uio.h>
#include <pthread.h>
#include <time.h>
#include "list.h"
#include "list.c"

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr_in local_addr, remote_addr;
int sockfd;                             //refers to socket file descriptor that created 
int is_exit=0;
int exit_send=0;
int key = 1;
int check_online = 0;


struct args{
    List* list1;
    List* list2;
};


void *key_in( void *ptr ){	
	//char *flag = "!";
    char user_input[9999];
    List *list1 = (List*) ptr;
	while(is_exit == 0 && exit_send == 0){	
		fgets(user_input, 9999, stdin);
		user_input[strlen(user_input) - 1] = '\0'; //remove new line character
        pthread_mutex_lock(&mutex1);
		List_add(list1, user_input);
        //memset(&user_input, 0, sizeof(user_input));
		pthread_mutex_unlock(&mutex1);
        //printf("finished input pthread.\n");
		if(strcmp(user_input, "!exit") == 0 ){
			is_exit = 1;
		}
        
	}
    
	pthread_exit(0);
}

void *send_out( void *ptr ){
    //printf("start sendto pthread.\n");
    List *list1 = (List*) ptr;
	char *msg;
	char *removed;
    clock_t before;
    clock_t difference;
    int msec, trigger = 2000;

    while(is_exit == 0 && exit_send == 0){
        if(List_curr(list1) != NULL){
            pthread_mutex_lock(&mutex1);
            msg = List_first(list1);
            removed = List_remove(list1);
            pthread_mutex_unlock(&mutex1);
            for(int i=0; i<strlen(msg);i++)
            {
                msg[i]=(msg[i]+key)%256;
            }
            sendto(sockfd, (char *)msg, strlen(msg), 0, (const struct sockaddr *) &remote_addr, sizeof(remote_addr));
            for(int i=0; i<strlen(msg);i++)
            {
                msg[i]=(msg[i]+256-key)%256;
            }
            if(strcmp(msg,"!status") == 0)
            {
                before = clock();
                msec = 0;
                while (msec < trigger)
                {
                    difference = clock() - before;
                    msec = difference * 1000 / CLOCKS_PER_SEC;
                    //printf("%d\n", msec);
                }
                // recv 
                // if not recv in 2 seconds, print Offline

                if(check_online == 0){
                    printf("Offline\n");
                    fflush(stdout);
                }
                check_online = 0;    
            }
            else if(strcmp(msg,"!exit") == 0)
            {
                //printf("!exit send !!!!!\n");
                exit_send=1;
                pthread_mutex_lock(&mutex1);
                List_free(list1, NULL);
                pthread_mutex_unlock(&mutex1);
	            shutdown(sockfd, SHUT_RDWR);
            }
            //printf("finished sendto pthread.\n");
        }
    }

	pthread_exit(0);
}

void *print_screen( void *ptr ){
    //printf("start print_screen pthread.\n");
    List *list2 = (List*) ptr;
    char *msg;
    void *removed;

    while(is_exit == 0){
        if(List_curr(list2) != NULL){
            pthread_mutex_lock(&mutex2);

            msg = List_first(list2);
            removed = List_remove(list2);
            printf("%s\n",msg);
            fflush(stdout);
            pthread_mutex_unlock(&mutex2);

        }
    }
    pthread_mutex_lock(&mutex2);
    List_free(list2, NULL);
    pthread_mutex_unlock(&mutex2);
	pthread_exit(0);
}
void *recv_in( void *ptr ){
    //printf("start recv_in pthread \n");

    int n=0;
    char buffer[9999];
    List *list1 = ((struct args*) ptr)->list1;
    List *list2 = ((struct args*) ptr)->list2;
	socklen_t fromlen = sizeof(remote_addr);
    
	while(is_exit == 0){
		n = recvfrom(sockfd, (char *)buffer, 9999, 0, (struct sockaddr *) &remote_addr, &fromlen);
        buffer[n] = '\0';
        if (n >= 0) {
            for(int i=0; i<strlen(buffer);i++)
            {
                buffer[i]=(buffer[i]+256-key)%256;
            }
            if(strcmp(buffer,"ONLINE#$!") == 0)
            {
                check_online = 1;
                printf("Online\n");
                fflush(stdout);
            }
            else if(strcmp(buffer,"!status") == 0)
            {
                char msg[9]="ONLINE#$!";
                for(int i=0; i<strlen(msg);i++)
                {
                    msg[i]=(msg[i]+key)%256;
                }
                sendto(sockfd, msg, 9, 0, (const struct sockaddr *) &remote_addr, sizeof(remote_addr));
            }
            
            else if(strcmp(buffer,"!exit") == 0)
            {
                is_exit = 1;
                exit_send=1;
                printf("%s\n",buffer);
                pthread_mutex_lock(&mutex1);
                List_free(list1, NULL);
                pthread_mutex_unlock(&mutex1);
                pthread_mutex_lock(&mutex2);
                List_free(list2, NULL);
                pthread_mutex_unlock(&mutex2);
                free(ptr);
                shutdown(sockfd, SHUT_RDWR);
                exit(0);
            }
            else
            {
                pthread_mutex_lock(&mutex2);
                List_add(list2, buffer);
                pthread_mutex_unlock(&mutex2);
            }
        }
//printf("finished recv_in pthread. and buffer is %s\n",buffer);
	}
	pthread_exit(0);
}

int main (int argc, char *argv[]){
    //initialize
    int local_port = 0;
    int rem_port = 0;
    //printf("argc is %d ,argv[1] is %s, argv[2] is %s, argv[3] is %s:\n",argc, argv[1],argv[2],argv[3]);
    if (argc != 4)
    {
        printf("Usage:\n");
        printf("    ./lets-talk <local port> <remote host> <remote port>\n");
        printf("Examples:\n");
        printf("    ./lets-talk 3000 192.168.0.513 3001\n");
        printf("    ./lets-talk 3000 some-computer-name 3001\n");
        fflush(stdout);
        exit(0);
    }
    local_port = atoi(argv[1]);
    rem_port = atoi(argv[3]);
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed\n");
        exit(0);
    }
    //else
        //printf("socket successfully created ..\n");

    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));

    struct hostent hostentstruct;
    struct hostent *hostentptr;
    if ((hostentptr = gethostbyname (argv[2])) == NULL)
    {
        printf("gethostbyname failed\n");
        exit(0);
    }
    
    hostentstruct = *hostentptr;
    // Filling local information
    local_addr.sin_family = hostentstruct.h_addrtype;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr = * ((struct in_addr *) hostentstruct.h_addr);

    remote_addr.sin_family = hostentstruct.h_addrtype;
    remote_addr.sin_port = htons(rem_port);
    remote_addr.sin_addr = * ((struct in_addr *) hostentstruct.h_addr);

    if (bind(sockfd,(struct sockaddr *)&local_addr, sizeof local_addr) != 0 )
    {
        perror("bind failed");
        exit(0);
    }
    printf("Welcome to LetS-Talk! Please type your messages now.\n");
    fflush(stdout);
    // else
    // {
    //     printf("Socket successfully binded..\n");
    // }

    struct args *lists=(struct args *)malloc(sizeof(struct args));
    List *list1 = List_create();
    List *list2 = List_create();
    lists->list1 = list1;
    lists->list2 = list2;


    int p1, p2, p3, p4;
    pthread_t input, send, print, receive;
  

    int arg1=1;
    int arg2=2;
    p1 = pthread_create(&input, NULL, key_in,list1);
	p2 = pthread_create(&send, NULL, send_out,list1);
	p3 = pthread_create(&print, NULL, print_screen,list2);
	p4 = pthread_create(&receive, NULL, recv_in, (void *)lists);
    
    pthread_join(input, NULL);
	pthread_join(send, NULL);
	pthread_join(print, NULL);
	pthread_join(receive, NULL);
    free(lists);
    close(sockfd);
    return 0;

}
