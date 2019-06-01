/*
 * file main.c
 * 
* C socket TCP server example, 
* handles multiple clients using threads.
* 
* Log amount of rx data to text file. 
* 
* Author: Aleksandr Barunin
* email: aabzel@yandex.ru
* 
* Data: 2019
*/

#include <stdio.h>
#include <string.h>	
#include <stdlib.h>	
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>	 
#include <unistd.h>	 
#include <pthread.h>  

#define TCP_SERVER_PORT 40402
#define SIZE_OF_RT_TX_ARRAY 2000

typedef unsigned int uint32_t;


struct xThreadArg_t {
	int sock ;
	unsigned int clientIp;
};
typedef struct xThreadArg_t ThreadArg_t; 
//the thread function
void *connection_handler(void *pThreadArg);

int save_statistic_to_file(int socket,
                           int amountOfRxBytes);

int unpack_ipv4 ( uint32_t ipAddr, char *outArray, int *outArrayLen);

int main (int argc , char *argv[])
{	
	int socket_desc , client_sock;
	socklen_t addrlen;
	ThreadArg_t *new_sock;
	struct sockaddr_in server , client;
	
	socket_desc = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP);
	if (socket_desc == -1) {
		printf("\nCould not create socket.");
	}
	printf("\nTCP server socket created.");
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( TCP_SERVER_PORT );
	
	printf("\nTCP server port: %d", TCP_SERVER_PORT);

    int ret;
    ret = bind(socket_desc, (struct sockaddr *)&server, sizeof(server));
	if( ret  < 0) {
		printf("\nbind failed. Error %d",ret);
		printf(" errno: %d", errno);
		return 1;
	}
	printf("\nBind done.");
	
	ret = listen(socket_desc , 4);
	if (ret < 0) {
		printf("\nlisten failed.");
	}
	
	//Accept and incoming connection
	puts("\nWaiting for incoming connections...");
	addrlen = sizeof(struct sockaddr_in);
	
	int loop = 1;
	while (loop) {
		client_sock = accept(socket_desc, 
		                    (struct sockaddr *)&client, 
		                    &addrlen);
		if(client_sock < 0) {
			loop = 0;
			printf("\n Accept failed.");
			return 1;
		}
		printf("\n Connection accepted.");
		
		pthread_t sniffer_thread;
		//printf("\n sizeof(ThreadArg_t) %d", sizeof(ThreadArg_t));
		new_sock = malloc (sizeof(ThreadArg_t));
		if (!new_sock) {
			return 2;
		}
		
	    printf ("\n TCP client port: %d ", client.sin_port);
	    
	    new_sock->sock = client_sock;
	    new_sock->clientIp =  client.sin_addr.s_addr;
	    
	    ret = pthread_create (&sniffer_thread , 
	                          NULL,  
	                          connection_handler, 
	                          (void*) new_sock);
	    if ( ret  < 0) {
			printf("\nCould not create thread.");
			return 1;
		}		
		//Now join the thread , so that we dont terminate before the thread
		//pthread_join( sniffer_thread , NULL);
		printf("\nHandler assigned.");
	}
	
	return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *pThreadArg)
{
	//Get the socket descriptor
	ThreadArg_t sockIP = *((ThreadArg_t*) pThreadArg);
	int read_size=0,ret=0;
	int amountOfRxBytes=0;
	
	char strClientIpAddr[20]={0};
	int ipAddrlen =0;
    ret = unpack_ipv4 (sockIP.clientIp, strClientIpAddr, &ipAddrlen);	
    if (!ret) {
		printf ("\n the IP of the TCP client: ");
        printf ("%s", strClientIpAddr);
	}
 
	char rxClientMessage[SIZE_OF_RT_TX_ARRAY]={0};
	char txClientMessage[SIZE_OF_RT_TX_ARRAY]={0};
	
	//Send some messages to the client
	strcpy(txClientMessage,"Hi! I am your connection handler\n");
	write(sockIP.sock, txClientMessage, strlen(txClientMessage));
	
	strcpy(txClientMessage,
	       "Now type something and I will repeat what you type \n");
	write(sockIP.sock, txClientMessage, strlen(txClientMessage));
	
	int loop = 1;
	while (loop) {
		//Receive a message from client
	    read_size = recv (sockIP.sock, rxClientMessage, 
                          sizeof(rxClientMessage), 0);
        if(0 == read_size) {
			loop = 0;
			puts("\nClient disconnected");
			fflush(stdout);
		} else if (read_size<0){
			loop=0;
			printf("r\necv failed");
		} else {
			amountOfRxBytes +=read_size;
			printf("\n   client IP: %s  msg: %s",strClientIpAddr, 
			                                     rxClientMessage);
            printf("\n");
			write(sockIP.sock, rxClientMessage , 
			                   strlen(rxClientMessage));
			printf("\n");                   
			memset(rxClientMessage, 0x00, sizeof(rxClientMessage));
		}
	}
	save_statistic_to_file (sockIP.clientIp, amountOfRxBytes);
	printf("\n sock %d", sockIP.sock);
	printf("\n IP %s", strClientIpAddr);
	printf("\n amountOfRxBytes from %d", amountOfRxBytes);
	printf("\n");
	
	//Free the pointer
	free (pThreadArg);
	pThreadArg = NULL;
	
	return 0;
}


int save_statistic_to_file(int ip4byte,
                           int amountOfRxBytes )
{
   int static busy=0;
   if (!busy) {
	   busy=1;
   } else {
	   return 1;
   }
   FILE *fp = NULL;
   char name[] = "rx_log.txt";
   fp = fopen(name, "a+");
   if (NULL == fp) {
     printf("\nunable to open file");
     busy = 0;
     return 1;
   }
   char outArray[20]={0};
   int outArrayLen = 0;
   unpack_ipv4 ( ip4byte, outArray, &outArrayLen);
   fprintf(fp, "client IP: %s ", outArray);
   fprintf(fp, "amount of rx bytes: %d \n", amountOfRxBytes);

   fclose(fp);
   busy=0;
   return 0;	
}


/*
 * outArray 1.1.1.1(7byte)....... 255.255.255.255 (15byte)
 * 7<*outArrayLen<15   
 * */
int unpack_ipv4 ( uint32_t ipAddr, char *outArray, int *outArrayLen)
{
	unsigned char *ipAddrBytes;
	char ipTempAddrArray[20] = {0};
	ipAddrBytes = (unsigned char *) &ipAddr;
	sprintf(ipTempAddrArray,"%d.%d.%d.%d", (int)ipAddrBytes[0], 
	                                   (int)ipAddrBytes[1],
	                                   (int)ipAddrBytes[2], 
	                                   (int)ipAddrBytes[3]);
    int len = strlen(ipTempAddrArray);
    
	if ((7<=len) && (len<=15)) {
		strcpy(outArray, ipTempAddrArray);
		*outArrayLen = len;
	} else {
        return 1;
    }

	return 0;
}
