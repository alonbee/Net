//
// FILE: srt_client.c
//
// Description: this file contains client states' definition, some important data structures
// and the client SRT socket interface definitions. You need to implement all these interfaces
//
// Date: April 18, 2008
//       April 21, 2008 **Added more detailed description of prototypes fixed ambiguities** ATC
//       April 26, 2008 ** Added GBN and send buffer function descriptions **
//


// Notes: 1 Remember to seperate segment before appending dato to buffer 


#include "srt_client.h"
#include <sys/time.h>

//
//
//  SRT socket API for the client side application. 
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

client_tcb_t* client_tcb_table[MAX_TRANSPORT_CONNECTIONS];  /* number of allowed client connections*/
int tcp_socknum = -1;  // Socket number from TCP protocal
static struct timespec syn_wait_time;
static struct timespec fin_wait_time;


// This function initializes the TCB table marking all entries NULL. It also initializes 
// a global variable for the overlay TCP socket descriptor ``conn'' used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to 
// handle the incoming segments. There is only one seghandler for the client side which
// handles call connections for the client.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void srt_client_init(int conn)
{
  tcp_socknum = conn;
  // Create thread to receive data from socket
  pthread_t pid;
  int err = pthread_create(&pid, NULL, seghandler, (void *)NULL);


  syn_wait_time.tv_sec = 0;
  syn_wait_time.tv_nsec = SYNSEG_TIMEOUT_NS;
  fin_wait_time.tv_sec = 0;
  fin_wait_time.tv_nsec = FINSEG_TIMEOUT_NS;

  if (err < 0) 
    printf("Failed to init clien\n");
  else
    printf("Successful to init client\n");
  // pthread_join(pid, NULL);
  return;

}


// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized 
// e.g., TCB state is set to CLOSED and the client port set to the function call parameter 
// client port.  The TCB table entry index should be returned as the new socket ID to the client 
// and be used to identify the connection on the client side. If no entry in the TC table  
// is available the function returns -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_client_sock(unsigned int client_port)
{
	int sockfd = 0;
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i) {
	   if (client_tcb_table[i] == NULL) {
	      sockfd = i;
	      // Initilize tcb 
	      client_tcb_t *tp = (client_tcb_t *)malloc(sizeof(client_tcb_t));  // Very important to allocate space
	      client_tcb_table[i] = tp;
	      tp -> client_portNum = client_port;   /*Set original port*/
	      tp -> state = CLOSED;
	      tp -> unAck_segNum = 0;
	      tp -> next_seqNum = 0;

	      tp -> sendBufHead = NULL;
	      tp -> sendBufunSent = NULL;
	      tp -> sendBufTail = NULL;

	      // // Initialize buffers
	      // tp -> sendBufHead = malloc(sizeof(segBuf_t));
	      // tp -> sendBufunSent = malloc(sizeof(segBuf_t));
	      // tp -> sendBufTail = malloc(sizeof(segBuf_t));
  		 
	      // Mutex: Initial tp->bufMutex 
        pthread_mutex_t* lock;
        lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        assert(lock!=NULL);
  		  if (pthread_mutex_init(lock, NULL) != 0)
  		  {
	        printf("Pthread mutex init failed, sockfd = %d\n", sockfd);
	        return -1;
   		  }
        tp -> bufMutex = lock;

	      printf("TCB socket=%d created successfully\n",sockfd);
	      return sockfd;
	    }
	  }
	printf("SOCK: Can't create for client_port =%u \n", client_port);
	return -1;
}


client_tcb_t* gettcb(int sockfd) {
  client_tcb_t* tp; 
  tp = client_tcb_table[sockfd];
  if (!tp)
    printf("Error in getting client_tcb_t for sockefd = %d\n", sockfd);
  return tp;
}



// This function is used to connect to the server. It takes the socket ID and the 
// server's port number as input parameters. The socket ID is used to find the TCB entry.  
// This function sets up the TCB's server port number and a SYN segment to send to
// the server using snp_sendseg(). After the SYN segment is sent, a timer is started. 
// If no SYNACK is received after SYNSEG_TIMEOUT timeout, then the SYN is 
// retransmitted. If SYNACK is received, return 1. Otherwise, if the number of SYNs 
// sent > SYN_MAX_RETRY,  transition to CLOSED state and return -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_client_connect(int socked, unsigned int server_port)
{
  client_tcb_t *tp = malloc(sizeof(client_tcb_t));
  tp = gettcb(sockfd);
  if (!tp) {
    printf("Connect:Can't get client_tcb_t sockfd = %d\n", sockfd);
    return -1;
  }
  // Make server port number
  tp-> svr_portNum = server_port;
  seg_t* syn = malloc(sizeof(syn));      /*Can't put variable initialization in Switch statement*/ 

  printf("Client start connecting, state =%d\n",tp->state);
  switch (tp -> state) {
    case CLOSED:{
      // Set seg_t data structure
      // memset(&syn, 0, sizeof (syn));
      syn->header.type = SYN;
      syn->header.src_port = tp -> client_portNum; /*Todo: Must set src port before hand; in src_client_sock*/
      syn->header.dest_port = tp ->svr_portNum;
      syn->header.length = 0; /*No DATA sent*/
      syn->header.seq_num = 0;

      // Send seg_t and state transform; connetion is the TCP socket number;
      if (snp_sendseg(tcp_socknum, syn) > 0) {
        printf("SYN sent successfully for sockfd =%d\n",sockfd);
      }
      else {
        printf("SYN sent failed for sockfd =%d\n",sockfd);
        break;
      }

      tp -> state = SYNSENT;
      // Max retry and for timeout;  Select function
      // clock_t time_begin = clock();
      int entry = 0;
      while (tp -> state != CONNECTED) {
        // clock_t time_cur = clock();
        // clock_t delta = time_cur - time_begin;
        // if ( (delta * 1.0 / CLOCK_PER_SEC)*1000 > SYNSEG_TIMEOUT_NS /1e6) {
          if (entry > SYN_MAX_RETRY) {
            tp -> state = CLOSED;
            printf("Tp of sockfd =%d  failed to connect after a max try\n", sockfd);
            return -1;
          }
          else {
            // snp_sendseg(tcp_socknum,syn);
        if (snp_sendseg(tcp_socknum, syn) < 0) {
          printf("SYN sent failed for sockfd =%d\n",sockfd);
        }
          // time_begin = clock();
            entry ++;
            nanosleep(&syn_wait_time, NULL);
          }
        // }
      }
      printf("sockfd = %d Connected Successfully\n",sockfd);
      return 1;
    }
    case SYNSENT:
      return -1;
    case CONNECTED:
      return -1;
    case FINWAIT:
      return -1;
    default:
      return -1;
    }
  return -1;
}


segBuf_t* create_buf(client_tcb_t* tp, void* data, unsigned int length) {
	 segBuf_t* buf;
	 buf -> seg = malloc(sizeof(seg_t));
	 buf -> seg.header.type = DATA;
	 buf -> seg.header.src_port = tp -> client_portNum;
	 buf -> seg.header.dest_port = tp -> svr_portNum;
	 buf -> seg.header.length = length;
	 // TODO: is this assignment right ?
	 buf -> seg.header.seq_num = tp -> next_seqNum;
	 buf -> seg.header.ack_num = tp -> next_seqNum + length;
	 buf -> seg.header.checksum = // TODO: How to calculate checksum?  

	 strncpy(buf -> seg.data, data, length);
	 buf -> next = NULL;
	 return buf;
}


// Send data to a srt server. This function should use the socket ID to find the TCP entry. 
// Then It should create segBufs using the given data and append them to send buffer linked list. 
// If the send buffer was empty before insertion, a thread called sendbuf_timer 
// should be started to poll the send buffer every SENDBUF_POLLING_INTERVAL time
// to check if a timeout event should occur. If the function completes successfully, 
// it returns 1. Otherwise, it returns -1.
// 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_client_send(int sockfd, void* data, unsigned int length)
{
  client_tcb_t* tp = gettcb(sockfd);
  if (!tp){
  	printf("can't find client_tcb_t for sockfd = %d\n",sockfd);
  	return -1;
  }
  // Prevent calling srt_client_send before srt_client_connect
  if (tp -> client_portNum <= 0 || tp -> svr_portNum <= 0 || length == 0) {
  	printf("Fail to send, client and server port Unkown, sockfd = %d\n", sockfd);
  	return -1;
  }
  if (tp -> state != CONNECTED) {
  	printf("sockfd =%d is not in CONNECTED state\n",sockfd);
  	return -1;
  }
  int seg_num = length / MAX_SEG_LEN;
  // Append new buffers & Create a sendbuf_timer for the first time

	for (int i = 0; i <= seg_num; ++i) {
	  segBuf_t* buf;
    pthread_mutex_lock(tp -> bufMutex);
	  if (tp -> next_seqNum == 0) {
      buf = create_buf(tp, data + i * MAX_SEG_LEN, max(min(length - i * MAX_SEG_LEN, MAX_SEG_LEN),0));
  	  tp -> next_seqNum += length;

    	tp -> sendBufHead = buf;
  		tp -> sendBufunSent = buf;
  		tp -> sendBufTail = buf;
      tp -> sentTime = 0;
      // TODO: Risks ???
	  	// pthread_t pid;
	  	// int err = pthread_create(&pid, NULL, sendBuf_timer, tp);
	   }
	  else {
      // Create buffer and set next_seqNum
      buf = create_buf(tp, data + i * MAX_SEG_LEN, max(min(length - i * MAX_SEG_LEN, MAX_SEG_LEN),0));
      tp -> next_seqNum += length;


  		tp -> sendBufTail -> next = buf;
  		tp -> sendBufTail = buf; 
      tp -> sentTime = 0;
  		if (tp -> sendBufunSent == NULL) {
  			tp -> sendBufunSent = buf;
		  }
	  }
    pthread_mutex_unlock(tp -> bufMutex);
	}
// Send n buffers smaller than window size
  if (sendBuf(tp) < 0) {
    printf("fun sendBuf failed\n");
    return -1;
  }
  return 1;
}

int sendBuf(client_tcb_t* clienttcb) {
  client_tcb_t* tp = clienttcb;
  pthread_mutex_lock(tp -> bufMutex);
  while (tp -> unAck_segNum < GBN_WINDOW && tp -> sendBufunSent != NULL) {
    // Get current time and write in package 

    // struct timespec tm;
    // tp -> sendBufunSent -> sentTime = current_utc_time_ns(&tm) / NS_TO_MICROSECONDS;
    // tp -> sendBufunSent -> sentTime = time(0);
    struct timeval tm;
    gettimeofday(&tm, NULL);
    tp -> sendBufunSent -> sentTime = tm.tv_sec * 1000000 +tm.tv_usec;    // sec to us
    if (snp_sendseg(tcp_socknum, tp -> sendBufunSent -> seg) < 0) {
          printf("Sending data failed sockfd = %d\n",sockfd);
          return -1;
        }
      else {
           printf("Sending data successfully sockfd =%d, seq_num =%d\n",sockfd, tp -> sendBufunSent -> seg.header.seq_num);
      }
      // Start monitering timeout event
      if (tp -> unAck_segNum == 0) {
              pthread_t pid;
              int err = pthread_create(&pid, NULL, sendBuf_timer, tp);
      }
        tp -> unAck_segNum ++;
        tp -> sendBufunSent = tp -> sendBufunSent -> next;
  }
  pthread_mutex_unlock(tp -> bufMutex);
  return 1;
}

void sendBuf_free(client_tcb_t* clienttcb) {
  client_tcb_t* tp = client_tcb;
    pthread_mutex_lock(tp -> bufMutex);

    while (tp -> sendBufHead && tp -> sendBufHead != tp -> sendBufTail) {
      segBuf_t* buf = tp -> sendBufHead;
      tp -> sendBufHead = tp -> sendBufHead -> next;
      free(buf);
    }
    free(tp -> sendBufTail);
    tp -> sendBufHead = NULL;
    tp -> sendBufunSent = NULL;
    tp -> sendBufTail = NULL;
    tp -> unAck_segNum = 0;
    tp -> next_seqNum = 0;
    tp -> svr_portNum = -1;
    pthread_mutex_unlock(tp -> bufMutex);
}

// This function is used to disconnect from the server. It takes the socket ID as 
// an input parameter. The socket ID is used to find the TCB entry in the TCB table.  
// This function sends a FIN segment to the server. After the FIN segment is sent
// the state should transition to FINWAIT and a timer started. If the 
// state == CLOSED after the timeout the FINACK was successfully received. Else,
// if after a number of retries FIN_MAX_RETRY the state is still FINWAIT then
// the state transitions to CLOSED and -1 is returned.


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_client_disconnect(int sockfd)
{
  client_tcb_t  *tp;
  tp = gettcb(sockfd);  /*Todo*/
  if (!tp) {
    printf("Disconenct: Can't get client_tcb_t sockfd = %d\n", sockfd);
    return -1;
  }
  seg_t* fin = malloc(sizeof(fin)); 
  switch (tp -> state) {
    case CLOSED:
      return -1;
    case SYNSENT:
      return -1;       
    case CONNECTED: {
      // memset(&fin,0, sizeof(fin));
      fin->header.type = FIN;
      fin->header.src_port = tp -> client_portNum;
      fin->header.dest_port = tp -> svr_portNum;
      fin->header.length = 0;
      snp_sendseg(tcp_socknum, fin);
      tp->state = FINWAIT;
      // Max retry and for timeout;  Select function
      clock_t time_begin = clock();
      int entry = 0;
      while (tp -> state != CLOSED) {
        clock_t time_cur = clock();
        clock_t delta = time_cur - time_begin;
          if (entry > FIN_MAX_RETRY) {
            tp -> state = CLOSED;
            printf("Tp of sockfd =  failed to connect after a max try%d\n", sockfd);
            return -1;
          }
          else {
            snp_sendseg(tcp_socknum,fin);
            time_begin = clock();
            entry ++;
            nanosleep(&fin_wait_time, NULL);
          }
      }
      // Do some clean when tp -> state == CLOSED, free all buffers
      if (tp -> state == CLOSED) {
        sendBuf_free(tp);
      }
      printf("sockfd = %d Disonnected Successfully\n",sockfd);
      return 1;
      break;
    }
    case FINWAIT:
      return -1;
    default:
      printf("Invalid state = %d\n", tp -> state);
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
int srt_client_close(int sockfd)
{
  client_tcb_t* tp;
  tp = gettcb(sockfd);
  if (!tp) {
    printf("CLOSE: Can't get client_tcb_t sockfd = %d\n", sockfd);
  }
  switch(tp -> state) {
    case CLOSED: {
    	   // Destroy mutex lock
    	  pthread_mutex_destroy(tp -> bufMutex);
    	   // Free struct 
        free(client_tcb_table[sockfd]);
        client_tcb_table[sockfd] = NULL;
        printf("CLOSE: sockfd = %d closed siccessfully\n", sockfd);
        return 1;
    }
    case SYNSENT:
        return -1;
    case CONNECTED:
        return -1;
    case FINWAIT:
        return -1;
      default:
        return -1;
  }
	return -1;
}

// Get a client_tcb by segment dest port
client_tcb_t* gettcb_by_port(int port) {
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i) {
		if (client_tcb_table[i] != NULL){
			if (client_tcb_table[i] -> client_portNum == port) {
				client_tcb_t* tp = client_tcb_table[i];
				return tp;
			}
		}
	}
	return NULL;
} 

// ** This functions receive acks and update buffers
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void sendBuf_recvACK(client_tcb_t* clienttcb, unsigned int ack_num) {
  client_tcb_t* tp = (client_tcb_t*) clienttcb;
  pthread_mutex_lock(tp -> bufMutex);
  // Acked buf's seq_num must be smaller than received ack  
  // TODO: Clear the tail
  if (tp -> sendBufTail -> seg.header.seq_num < ack_num)
    tp -> sendBufTail = 0;

  //Move acked head and free previous head
  while(tp -> sendBufHead && tp -> sendBufHead -> seg.header.seq_num < ack_num) {
    segBuf_t* temp = tp -> sendBufHead;
    tp -> sendBufHead = tp -> sendBufHead -> next;
    tp -> unAck_segNum--;
    free(temp);
  }
  pthread_mutex_unlock(tp -> bufMutex);
}



// This is a thread  started by srt_client_init(). It handles all the incoming 
// segments from the server. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void *seghandler(void* arg)
{
  // Get ACK to right sockfd
  seg_t* seg = malloc(sizeof(seg));
  // memset(&seg, 0, sizeof(seg));
  client_tcb_t* tp;

  while(1) {
    tp = NULL;
    if (snp_recvseg(tcp_socknum, seg) != 1) {
      close(tcp_socknum);
      pthread_exit(NULL);
    }
    int sockfd = -1;
    if ((tp = gettcb_by_port(seg -> header.dest_port)) == NULL) {
    	printf("can't find the client for seg dest_port = %d\n", seg -> header.dest_port);
    	continue;
    }
    // For debug
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i)
    {
    	if (tp == client_tcb_table[i]) {
    		sockfd = i;
    		break;
    	}
    }
    // // Find socket fd for giving dest_port
    // for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i)
    // {
    //   if (client_tcb_table[i] != NULL) {
    //     tp = client_tcb_table[i];
    //     // Get the right tcb based on dest port
    //     if (tp -> client_portNum == seg->header.dest_port){
    //       sockfd = i; /*get socket number in srt for client port*/
    //       break;
    //     }
    //   }
    // }
    if (sockfd < 0) {
      printf("Wrong seg destination\n");
      continue;
    }

    switch(tp->state) {
      case CLOSED:
        printf("sockfd=%d received expired seg\n", sockfd);
        break;
      case SYNSENT:
        if (seg->header.type == SYNACK) {
          printf("sockfd=%d received SYNACK\n",sockfd);
          tp -> state = CONNECTED;
        }
        else {
          printf("sockfd=%d received=%d in SYNSENT\n",sockfd,seg->header.type);
        }
        break;
      case CONNECTED: {
        printf("sockfd=%d received=%d in CONNECTED\n",sockfd,seg->header.type);
        if (seg.header.type == DATAACK) {
          if (tp -> sendBufHead != NULL && seg.header.ack_num >= tp -> sendBufHead-> seg.header.seq_num) {
            // Receive seg ack larger than head seq_num, remove acked buffer from linked list
            sendBuf_recvACK(tp,seg.header.ack_num);
            sendBuf(tp);
          }
        }
        break;
    }
      case FINWAIT:
          if (seg->header.type == FINACK) {
          printf("sockfd=%d received FINACK\n",sockfd);
          tp -> state = CLOSED;
        }
        else {
          printf("sockfd=%d received=%d in FINWAIT\n",sockfd,seg->header.type);
        }
        break;
        default:
          printf("sockfd=%d received unknown state\n", sockfd);
    }
  }
  printf("client seghandler exit\n");
  return 0;
}



// This thread continuously polls send buffer to trigger timeout events
// It should always be running when the send buffer is not empty
// If the current time -  first sent-but-unAcked segment's sent time > DATA_TIMEOUT, a timeout event occurs
// When timeout, resend all sent-but-unAcked segments
// When the send buffer is empty, this thread terminates
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void* sendBuf_timer(void* clienttcb)
{
  client_tcb_t* tp = (client_tcb_t*) clienttcb;
  struct timespec t1,t2;
  t1.tv_sec = 0;
  t1.tv_nsec = SENDBUF_POLLING_INTERVAL;
  nanosleep(&t1,&t2);
  struct timeval tm;
  unsigned int cur_time;

while (1) {
  segBuf_t* head = tp -> sendBufHead;
  if (head == NULL) {
    printf("Buffer head is NULL,client port num=%d\n",tp -> client_portNum);
    return;
  }
  // Get current time(us) and compare with that of packets
  gettimeofday(&tm,NULL);
  cur_time = tm.tv_sec * 1000000 + tm.tv_usec;
  if (tp -> unAck_segNum == 0) {
    printf("unAck_segNum is 0,client port num=%d, sendBuf_timer exit\n",tp ->client_portNum);
    pthread_exit(NULL);
    }
  else if (tp -> sendBufHead -> sentTime > 0 && (cur_time - tp ->sendBufHead->sentTime) > DATA_TIMEOUT) { // Head time out
      sendBuf_timeout(tp);
    }
  }
}
// This function resend all sent but un ack buffers
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void sendBuf_timeout(void* clienttcb){
  client_tcb_t* tp = (client_tcb_t*) clienttcb;
  pthread_mutex_lock(tp -> bufMutex);
  segBuf_t* buf = tp -> sendBufHead;
  for (int i = 0; i < tp -> unAck_segNum; ++i)
  {
    if (buf == NULL){
      printf("NULL Head in timeout, client port num=%d,i=%d,unAck_segNum=%d\n", tp->client_portNum,i,tp -> unAck_segNum);
      break;
    }
    snp_sendseg(tcp_socknum, buf->seg);
    struct timeval tm;
    gettimeofday(&tm, NULL);
    buf -> sentTime = tm.tv_sec*1000000 + tm.tv_usec;
    buf = buf -> next;
  }
  printf("Timeout resend successfully\n");
  pthread_mutex_unlock(tp -> bufMutex);
}
