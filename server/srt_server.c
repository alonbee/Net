// FILE: srt_server.c
//
// Description: this file contains server states' definition, some important
// data structures and the server SRT socket interface definitions. You need 
// to implement all these interfaces
//
// Date: April 18, 2008
//       April 21, 2008 **Added more detailed description of prototypes fixed ambiguities** ATC
//       April 26, 2008 **Added GBN descriptions
//


#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "srt_server.h"
svr_tcb_t* svr_tcb_table[MAX_TRANSPORT_CONNECTIONS];  /* number of allowed client connections*/
int tcp_socknum = -1;  // Socket number from TCP protocal

//
//
//  SRT socket API for the server side application. 
//  ===================================
//
//  In what follows, we provide the prototype definition for each call and limited pseudo code representation
//  of the function. This is not meant to be comprehensive - more a guideline. 
// 
//  You are free to design the code as you wish.
//
//  NOTE: When designing all functions you should consider all possible states of the FSM using
//  a switch statement (see the Lab3 assignment for an example). Typically, the FSM has to be
// in a certain state determined by the design of the FSM to carry out a certain action. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// This function initializes the TCB table marking all entries NULL. It also initializes 
// a global variable for the overlay TCP socket descriptor ``conn'' used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to 
// handle the incoming segments. There is only one seghandler for the server side which
// handles call connections for the client.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void srt_server_init(int conn)
{
	// memset(&svr_tcb_table,NULL, sizeof(svr_tcb_table));
  
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i)
  {
  	svr_tcb_table[i] = NULL;
  }
  tcp_socknum = conn;
  pthread_t pid;
  int err = pthread_create(&pid, NULL, seghandler, (void *)NULL);
  if (err < 0) 
    printf("Failed to init server.\n");
   else
   	printf("Successful to init server\n");
	
  return;
}


void server_create_buf(svr_tcb_t* servertcb){
	svr_tcb_t* tp = servertcb;
	char* buf = (char*) malloc(RECEIVE_BUF_SIZE);
	assert(buf != NULL);
	tp -> recvBuf = buf;
  printf("server tcb buffer created successfully\n");
	return;
}

// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized 
// e.g., TCB state is set to CLOSED and the server port set to the function call parameter 
// server port.  The TCB table entry index should be returned as the new socket ID to the server 
// and be used to identify the connection on the server side. If no entry in the TCB table  
// is available the function returns -1.

//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_server_sock(unsigned int port)
{
  int sockfd = 0;
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i) {
    if (svr_tcb_table[i] == NULL) {
      sockfd = i;
      // Initilize tcb 
      svr_tcb_t *tp = (svr_tcb_t*) malloc(sizeof(svr_tcb_t));
      svr_tcb_table[i] = tp;
      tp -> svr_portNum = port;   /*Set original port*/
      tp -> state = CLOSED;
      tp -> expect_seqNum = 0;

      printf("Sevrer: TCB socket=%d created successfully\n",sockfd);
      
      // 1 Multiple thread lock for each; 
	  pthread_mutex_t* lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	  assert(lock != NULL);
	  pthread_mutex_init(lock, NULL);
	  tp -> bufMutex = lock;

	  // 2 Create buffer;   
      server_create_buf(tp);

      // 3 State CHANGE 
      tp -> state = CLOSED;
      tp -> usedBufLen = 0;
      return sockfd;
    }
  }
  printf("Sevrer: SOCK: Can't create for client_port = %d\n", port);
  return -1;
}

// Get tcb for server by socketfd
svr_tcb_t* gettcb(int sockfd) {
  svr_tcb_t* tp; 
  tp = svr_tcb_table[sockfd];
  if (!tp)
    printf("Sevrer: Error in getting svr_tcb_t for sockefd = %d\n", sockfd);
  return tp;
}


// This function gets the TCB pointer using the sockfd and changes the state of the connection to 
// LISTENING. It then starts a timer to ``busy wait'' until the TCB's state changes to CONNECTED 
// (seghandler does this when a SYN is received). It waits in an infinite loop for the state 
// transition before proceeding and to return 1 when the state change happens, dropping out of
// the busy wait loop. You can implement this blocking wait in different ways, if you wish.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_server_accept(int sockfd)
{
  svr_tcb_t* tp;

  tp = gettcb(sockfd);
  if (!tp) {
    printf("Sevrer: Connect:Can't get svr_tcb_t sockfd = %d\n", sockfd);
    return -1;
  }
  printf("Server: Ready to accept\n");
  switch (tp -> state) {
    case CLOSED: {
    	tp -> state = LISTENING;
    	
    	// Check tp state periodically and wait for ACCEPT_POLLING_INTERVAL
    	static struct timespec accept_wait_time;
    	accept_wait_time.tv_sec = 0;
    	accept_wait_time.tv_nsec = ACCEPT_POLLING_INTERVAL; 
    	while(tp ->state != CONNECTED) {
    		nanosleep(&accept_wait_time, NULL);
    	}	
    	printf("Server: server sockfd=%d accept successful\n", sockfd);
    	return 1;
    }
    case LISTENING:
      return -1;
    case CONNECTED:
      return -1;
    case CLOSEWAIT:
      return -1;
    default:
      return -1;
    }
}


// Receive data from a srt client. Recall this is a unidirectional transport
// where DATA flows from the client to the server. Signaling/control messages
// such as SYN, SYNACK, etc.flow in both directions. 
// This function keeps polling the receive buffer every RECVBUF_POLLING_INTERVAL
// until the requested data is available, then it stores the data and returns 1
// If the function fails, return -1 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_server_recv(int sockfd, void* buf, unsigned int length)
{
  svr_tcb_t *tp;
  tp = gettcb(sockfd);
  if (!tp) {
    printf("Server: Can't find svrtcb in server_recv sockfd=%d\n", sockfd);
      return -1;
  }
  switch(tp -> state) {
  	case CLOSED:
  		return -1;
  	case LISTENING:
  		return -1;
  	case CONNECTED: {
  		// Wait for getting enough data in buffer and store them into the recBuf
  		// Get fragment by fragment
  		while(1) {
  			if (tp -> usedBufLen < length) {
  				sleep(RECVBUF_POLLING_INTERVAL);
  			}
  			else {
  				pthread_mutex_lock(tp -> bufMutex);
  				// Copy length bytes
  				char* temp = (char*) buf;
  				memcpy(temp, tp -> recvBuf, length);  // Send data to the target buffer
  				memcpy(tp -> recvBuf, tp -> recvBuf + length, tp-> usedBufLen - length);  // Put the unsent char intot the recvBuf
  				tp -> usedBufLen  -= length;
  				pthread_mutex_unlock(tp -> bufMutex);
  			}
  		}
  		return 1;
  	}
  	case CLOSEWAIT:
  		return -1;
  	default:
  		return -1;
  }
  return 0;
}


// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1 
// if fails (i.e., in the wrong state).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_server_close(int sockfd)
{

  svr_tcb_t *tp;
  tp = gettcb(sockfd);
  if (!tp) {
    printf("Server: Can't close sockfd = %d\n", sockfd);
      return -1;
  }
  switch(tp -> state) {
    case CLOSED:
      free(tp -> recvBuf);
      free(tp -> bufMutex);
      free(svr_tcb_table[sockfd]);
      svr_tcb_table[sockfd] = NULL;
      return 1;
    case LISTENING:
      return -1;
    case CONNECTED:
      return -1;
    case CLOSEWAIT:
      return -1;
    default:
      return -1;
  }
}


void* closewait(void* tcb) {
  svr_tcb_t* tp = (svr_tcb_t*)tcb;
  sleep(CLOSEWAIT_TIMEOUT);

  tp->state = CLOSED;
  pthread_exit(NULL);
}


void restore_data(svr_tcb_t* tp, seg_t* seg) {
	// Data amount can't exceed the buffer size
	if (tp -> usedBufLen < RECEIVE_BUF_SIZE - seg -> header.length){
		pthread_mutex_lock(tp -> bufMutex);
		memcpy(tp -> recvBuf + tp -> usedBufLen, seg -> data, seg -> header.length);
		tp -> usedBufLen += seg -> header.length;
		//Update tp -> expect_seqNum 
		tp -> expect_seqNum =  seg->header.seq_num + seg->header.length;
		pthread_mutex_unlock(tp -> bufMutex);
	}
	printf("Receive Buf SIZE NOT ENOUGH, DATA Saving to recvBuf failed\n");
}
// This is a thread  started by srt_server_init(). It handles all the incoming 
// segments from the client. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* seghandler(void* arg)
{
  seg_t* seg = (seg_t*) malloc(sizeof(seg_t));
  seg_t* ack = (seg_t*) malloc(sizeof(seg_t));
  // memset(&seg, 0, sizeof(seg));
  svr_tcb_t* tp = (svr_tcb_t*) malloc(sizeof(svr_tcb_t));

  while (1) {
    // Find the right server
    tp = NULL;
    if (snp_recvseg(tcp_socknum,seg) != 1) {
        printf("Can't receive data in server, ready to close\n");
      	close(tcp_socknum);
        pthread_exit(NULL);
    }

    // Get socketfd by port
    int sockfd = -1;
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i)
      {
        if (svr_tcb_table[i] != NULL && svr_tcb_table[i] -> svr_portNum == seg->header.dest_port) {
          // Get the right tcb based on dest port
            sockfd = i; /*get socket number in srt for client port*/
            // tp -> client_port = seg.header.src_port;
            tp = svr_tcb_table[i];
          }
        }

      printf("Server sockfd =%d,Received seg header type =%d\n",sockfd,seg->header.type);

      if (!tp){
      printf("Server: Can't get server_tcp for the seg\n");
      continue;
        }

      printf("seghandler sockfd=%d , state=%d \n", sockfd, tp->state);
    switch(tp->state) {
      case CLOSED:
          printf("Server: sockfd=%d received seg in CLOSED\n", sockfd);
        break;
      case LISTENING: {
        if(seg->header.type == SYN) {
          pthread_mutex_lock(tp -> bufMutex);
          tp->client_portNum = seg->header.src_port;
          tp->state=CONNECTED;
          printf("Server:sockfd = %d Got SYN from client\n", sockfd);
          
          // Set tcb expect_seqNum
          tp -> expect_seqNum = seg->header.seq_num;
          pthread_mutex_unlock(tp -> bufMutex);

          // memset(&ack,0, sizeof(ack));
          // Received SYN and send SYNACK back
          ack->header.type = SYNACK;
          ack->header.src_port = tp -> svr_portNum;
          ack->header.dest_port = tp -> client_portNum;
          ack->header.length = 0;
          snp_sendseg(tcp_socknum, ack);
          printf("Server:sockfd = %d Sent SYNACK to client\n", sockfd);
        }
        else
          printf("Server: Listening received SYN\n");
        break;
   	 }
      case CONNECTED: 
      {
        switch (seg->header.type) {
          printf("Server: tp %d: receive %d\n", sockfd, seg->header.type);
          case SYN:
          {
            // Received SYN and send SYNACK back
            // seg_t ack;
            // memset(&ack,0, sizeof(ack));
            pthread_mutex_lock(tp -> bufMutex);
          	tp -> expect_seqNum = seg->header.seq_num;
            pthread_mutex_unlock(tp -> bufMutex);

            ack->header.type = SYNACK;
            ack->header.src_port = tp -> svr_portNum;
            ack->header.dest_port = tp -> client_portNum;
            ack->header.length = 0;
            snp_sendseg(tcp_socknum, ack);
            break;            
          }
          case DATA: {
          	// tp -> expect_seqNum will be updated if right; Otherwise, same old expect_seqNum
          	if (seg -> header.seq_num == tp -> expect_seqNum){
          		// Update tp -> expect_seqNum and store data in tp -> recvBuf 
          		restore_data(tp, seg);
          	}
          		// Send ack back anayway
          		ack->header.type = DATAACK;
            	ack->header.src_port = tp -> svr_portNum;
            	ack->header.dest_port = tp -> client_portNum;
            	ack->header.length = 0;
            	ack->header.ack_num = tp -> expect_seqNum;  // Send the new expect_seqNum(added by data length) if the expect_seqNum is right 
            	snp_sendseg(tcp_socknum, ack);
         
          	break;
          }
          case FIN:
          {
            time_t timer = time(NULL);
            printf("Server: tp %d: receive FIN %s", sockfd, ctime(&timer));

            // send FINACK
            // seg_t ack;
            // memset(&ack,0, sizeof(ack));
            ack->header.type = FINACK;
            ack->header.src_port = tp -> svr_portNum;
            ack->header.dest_port = tp -> client_portNum;
            ack->header.length = 0;
            snp_sendseg(tcp_socknum, ack);

            tp->state = CLOSEWAIT;

            // start a thread for CLOSE_WAIT_TIMEOUT; If time expires, Set tp state as CLOSED  
            pthread_t closetimer;
            pthread_create(&closetimer,NULL,closewait, (void*)tp);
            break;                    
          }
          default:
            break;
          }
        break;
      }
      case CLOSEWAIT:
        // receive FIN
        if (seg->header.type == FIN) {

          // send FINACK
          // seg_t ack;
          // memset(&ack,0, sizeof(ack));
          ack->header.type = FINACK;
          ack->header.src_port = tp -> svr_portNum;
          ack->header.dest_port = tp -> client_portNum;
          ack->header.length = 0;
          snp_sendseg(tcp_socknum, ack);
          printf("Server: sent FINACK in closewait\n");
          }
        else {
          printf("Server: Received not fin in CLOSEWAIT\n");
        }
        break;
      default:
        printf("Server: sockfd= %d: wrong state\n", sockfd);
        break;
    }
  }
}

