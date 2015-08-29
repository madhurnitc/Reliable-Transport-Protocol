#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <getopt.h>
#include <ctype.h>
/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for sr), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
                           /* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
  };

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
    };

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/* Statistics 
 * Do NOT change the name/declaration of these variables
 * You need to set the value of these variables appropriately within your code.
 * */
int A_application = 0;
int A_transport = 0;
int B_application = 0;
int B_transport = 0;

/* Globals 
 * Do NOT change the name/declaration of these variables
 * They are set to zero here. You will need to set them (except WINSIZE) to some proper values.
 * */
float TIMEOUT = 30.0;
int WINSIZE;         //This is supplied as cmd-line parameter; You will need to read this value but do NOT modify it; 
int SND_BUFSIZE = 0; //Sender's Buffer size
int RCV_BUFSIZE = 0; //Receiver's Buffer size

//declaring function prototypes
void tolayer3(int AorB,struct pkt packet);
void tolayer5(int AorB,char *datasent);
void starttimer(int AorB,float increment);
void stoptimer(int AorB);

int srSenderBase;
int srNextSeqNumber;
int srReceiverBase;
int srReceiverNextSeqNumber;
int outOfOrderBufferedPacket = 0;
int refusedPacketCount =0;
pkt srSenderBuffer[1000];
pkt srReceiverBuffer[1000];
pkt refusedPacketBuffer[1000];
pkt srReceiverOutOfOrderBuffer[1000];
pkt srACKPkt;
struct logicalTimers {
	int packetSeqNumber;  // packet number for which this timer is associated
	float startTime;      // start time of this logical timer
	float endTime;       // end time of this logical timer
	bool active;         // true or false, based on if this timer is still active or not.
};

logicalTimers packetTimer[1000];
float time_local = 0;


/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message) //ram's comment - students can change the return type of the function from struct to pointers if necessary
{
	int i;
	int checksum;
	//increase value of A_application
	A_application++;

	//set the checksum to zero
	checksum = 0;
	// check if next sequence number falls in the current window
	if ( srNextSeqNumber < srSenderBase+WINSIZE)
	{
		printf("srsenderbase= %d  packetwindow = %d \n",srSenderBase,srSenderBase+WINSIZE);
		// set the sequence number for the packet
		srSenderBuffer[srNextSeqNumber].seqnum = srNextSeqNumber;
		//set the ack value in the packet
		srSenderBuffer[srNextSeqNumber].acknum = 0;
		// calculate the checksum (data+ack+seq number) as well set the payload for packet and save it in the packet and in sender buffer
		for (i=0 ; i< 20; i++)
		{
			checksum = checksum + message.data[i];
			srSenderBuffer[srNextSeqNumber].payload[i] = message.data[i];

		}
		//add the sequence number and ack number to the checksum calculated above
		checksum = checksum + srSenderBuffer[srNextSeqNumber].acknum + srSenderBuffer[srNextSeqNumber].seqnum;
		//set the checksum in packet
		srSenderBuffer[srNextSeqNumber].checksum = checksum;

		// send the data to layer 3
		printf("Sending Packet with Sequence Number =  %d \n" ,srSenderBuffer[srNextSeqNumber].seqnum);
		tolayer3(0,srSenderBuffer[srNextSeqNumber]);
		//increase A_transport
		A_transport++;

		printf("Time_Local = %lf \n", time_local);
		//set the value in logical timer for this packet
		packetTimer[srNextSeqNumber].packetSeqNumber = srNextSeqNumber;
		packetTimer[srNextSeqNumber].startTime = time_local;
		packetTimer[srNextSeqNumber].endTime = time_local + TIMEOUT;
		packetTimer[srNextSeqNumber].active = true;
		//start the timer
		starttimer(0, TIMEOUT);
		// increase the next sequence number
		srNextSeqNumber++;
	}
	else
	{
		//buffer the packets which doesn't fall in the current window
        printf("Buffering the packet with Sequence Number = %d\n", srNextSeqNumber);
		// set the sequence number for the packet in refused packet buffer and global sr buffer
		refusedPacketBuffer[refusedPacketCount].seqnum = srNextSeqNumber;
		srSenderBuffer[srNextSeqNumber].seqnum = srNextSeqNumber;
		// set the ack number for the packet in refused packet buffer and global sr buffer
		refusedPacketBuffer[refusedPacketCount].acknum = 0;
		srSenderBuffer[srNextSeqNumber].acknum = 0;
		// calculate the checksum (data+ack+seq number) as well set the payload for packet and save it in the packet and in sender buffer
		for (i=0 ; i< 20; i++)
		{
			checksum = checksum + message.data[i];
			refusedPacketBuffer[refusedPacketCount].payload[i] = message.data[i];
			srSenderBuffer[srNextSeqNumber].payload[i] = message.data[i];

		}
		//add the sequence number and ack number to the checksum calculated above
		checksum = checksum + refusedPacketBuffer[refusedPacketCount].acknum + srSenderBuffer[refusedPacketCount].seqnum;
		//set the checksum in refused packet buffer and global sr buffer
		refusedPacketBuffer[refusedPacketCount].checksum = checksum;
		srSenderBuffer[srNextSeqNumber].checksum = checksum;

		//increase the next sequence number
		srNextSeqNumber++;
		//increase the refused packet count
		refusedPacketCount++;
	}

}

void B_output(struct msg message)  /* need be completed only for extra credit */
// ram's comment - students can change the return type of this function from struct to pointers if needed  
{

}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
	int i,j;
	int checksum = 0;
	int smallestSeqNumber;
	//recalculate the checksum from the received packet
	for ( i=0; i<20; i++)
	{
		checksum = checksum + packet.payload[i];
	}
	//add ack and sequence number to above calculated checksum
    checksum = checksum + packet.acknum + packet.seqnum;
    //check if the above calculated checksum is same as what is stored in the packet
    if ( (checksum == packet.checksum ) && (packet.acknum >= srSenderBase) && (packet.acknum < srSenderBase+WINSIZE))
    {
    	printf("ACK recieved for packet with Seq Number = %d \n", packet.acknum);
         // stop the logical timer of this packet by making it inactive
    	packetTimer[packet.acknum].active = false;
    	//move the sr sender base to smallest unacked sequence number
    	//for this we can searh the in each packet's timer and
    	//find the first timer which has sequence number greater than the packet for which we just got ACK and its timer is still active
    	// then we compare the if
    	for(i=srSenderBase+1;i<srNextSeqNumber;i++)
    	{
    	   if( !packetTimer[i].active )
    	   {
    		   srSenderBase = packetTimer[i].packetSeqNumber+1;
    		   break;
    	   }

    	}
    	printf("NEw srsenderbase = %d",srSenderBase);
    	// send all the packets lies in new current window and which we didn't send because they didn't fall in the previous window
    	for(i=0;i<refusedPacketCount;i++)
    	{
    		if((refusedPacketBuffer[i].seqnum > srSenderBase) && (refusedPacketBuffer[i].seqnum < srSenderBase+WINSIZE) )
    		{
                //send the packet to layer 3
    	        printf("Sending refused packet with sequence Number = %d \n",refusedPacketBuffer[i].seqnum);
    	        tolayer3(0,refusedPacketBuffer[i]);
                //increase A_transport
    		    A_transport++;
    			//set the value in logical timer for this packet
    			packetTimer[i].packetSeqNumber = srNextSeqNumber;
    			packetTimer[i].startTime = time_local;
    			packetTimer[i].endTime = time_local + TIMEOUT;
    			packetTimer[i].active = true;
    			//start the timer
    			starttimer(0, TIMEOUT);
    	    }
    	}
    }



}

/* called when A's timer goes off */
void A_timerinterrupt() //ram's comment - changed the return type to void.
{
	int i;
	printf("Time_Local = %lf \n",time_local);
	//search in array of packet timers for packet whose endTime value is less than time_local and which is still active
	// then we can say this packet has timed-out and we need to resend this packet
	for (i=1;i<srNextSeqNumber;i++)
	{
		if ( (time_local >= packetTimer[i].endTime) && packetTimer[i].active )
		{
            //resend this packet to B and update its timer values
			printf("A_timeinterrupt : Sending the packet with sequence number = %d \n",packetTimer[i].packetSeqNumber);
			tolayer3(0,srSenderBuffer[packetTimer[i].packetSeqNumber]);
			//increase A_transport
			A_transport++;
			packetTimer[i].startTime = time_local;
			packetTimer[i].endTime = time_local + TIMEOUT;
			break;
		}

	}
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() //ram's comment - changed the return type to void.
{
	//initialize SR base and next sequence number
	srSenderBase = 1;
	srNextSeqNumber = 1;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
	int i;
    int checksum,ackchecksum;

    //set the checksums to 0
    checksum = 0;
    ackchecksum = 0;
    //increase B_transport
    B_transport++;
    //recalculate the checksum
    for(i=0 ; i<20 ; i++)
    {
    	checksum = checksum + packet.payload[i];
    }
	//add the sequence number and ack number to it
    checksum = checksum + packet.acknum + packet.seqnum;
    //check if the above calculated checksum is same as what is stored in the packet and
    //sequence number falls in window [ srReceiverBase ..  to .. srreceiverBase+WINSIZE-1 ]
    // we extract the data and send the ACK to A
    if ( ( checksum == packet.checksum ) && (packet.seqnum >= srReceiverBase) && (packet.seqnum < srReceiverBase+WINSIZE) )
    {
    	printf("Packet received %d without any corruption \n",packet.seqnum);
    	printf("srreceiver base = %d srrecievernextSeqnumber = %d \n", srReceiverBase,srReceiverNextSeqNumber);
    	//packet which comes out of order are buffered and selective ACK is sent to A
    	if (packet.seqnum > srReceiverBase)
    	{
    		//send the ACK to the receiver
    		srACKPkt.seqnum = 0;
    		srACKPkt.acknum = packet.seqnum;
        	for ( i=0 ; i<20 ;i++)
        	{
        		srACKPkt.payload[i] = '1';
        		ackchecksum = ackchecksum + srACKPkt.payload[i];
        	}
        	//add the acknum and sequence number to ackchecksum
        	ackchecksum = ackchecksum + srACKPkt.acknum + srACKPkt.seqnum;
        	// set this checksum in ACK packet
        	srACKPkt.checksum = ackchecksum;
        	//send the ACK packet to A
        	tolayer3(1,srACKPkt);
        	printf("ACK sent for the packet with Seq Number = %d \n" , packet.seqnum);
        	printf("buffering out of order packet with Seq Number = %d \n",packet.seqnum);
    		srReceiverBuffer[packet.seqnum].acknum = packet.acknum;
    		srReceiverBuffer[packet.seqnum].seqnum = packet.seqnum;
    		srReceiverBuffer[packet.seqnum].checksum = packet.checksum;
    		for (i=0; i<20;i++)
    		{
    		   srReceiverBuffer[packet.seqnum].payload[i] = packet.payload[i];
    		}

    		//increase out of order but buffered packet count
    		outOfOrderBufferedPacket++;

    	}
    	else if( packet.seqnum == srReceiverBase)
    	{
    		//fill the ACK packet details
    		srACKPkt.seqnum = 0;
    		srACKPkt.acknum = packet.seqnum;
        	for ( i=0 ; i<20 ;i++)
        	{
        		srACKPkt.payload[i] = '1';
        		ackchecksum = ackchecksum + srACKPkt.payload[i];
        	}
        	//add the acknum and sequence number to ackchecksum
        	ackchecksum = ackchecksum + srACKPkt.acknum + srACKPkt.seqnum;
        	// set this checksum in ACK packet
        	srACKPkt.checksum = ackchecksum;
        	//send the ACK packet to A
        	tolayer3(1,srACKPkt);
        	printf("ACK sent for the packet with Seq Number = %d \n",packet.seqnum);
        	//send this packet and previously buffered packet to layer 5
        	for (i= srReceiverBase; i<=srReceiverBase+outOfOrderBufferedPacket;i++)
        	{
        		//send the data to B_application
        		tolayer5(1,srReceiverBuffer[i].payload);
        		//increase B_application
        		B_application++;
        	}
        	// update the receiver base
        	srReceiverBase = srReceiverBase + outOfOrderBufferedPacket + 1;
        	//update reciever's next sequence number
        	//srReceiverNextSeqNumber = srReceiverNextSeqNumber + outOfOrderBufferedPacket;
        	//reset the out of order packets count to 0
        	outOfOrderBufferedPacket =0;
    	}
    }
    //acknowledge the packets which are received correctly in the window [ srReceiverBase-Winsize .. to .. srReceiverBase-1]
    else if ( ( checksum == packet.checksum ) && (packet.seqnum < srReceiverBase) && (packet.seqnum >= srReceiverBase-WINSIZE) )
	{
		//fill the ACK packet details
    	printf("Acknowledging the packet %d which falls in previous window \n",packet.seqnum);
		srACKPkt.seqnum = 0;
		srACKPkt.acknum = packet.seqnum;
    	for ( i=0 ; i<20 ;i++)
    	{
    		srACKPkt.payload[i] = '1';
    		ackchecksum = ackchecksum + srACKPkt.payload[i];
    	}
    	//add the acknum and sequence number to ackchecksum
    	ackchecksum = ackchecksum + srACKPkt.acknum + srACKPkt.seqnum;
    	// set this checksum in ACK packet
    	srACKPkt.checksum = ackchecksum;
    	//send the ACK packet to A
    	tolayer3(1,srACKPkt);
    	printf("ACK sent for the packet with Seq Number = %d\n",packet.seqnum);
	}
}

/* called when B's timer goes off */
void B_timerinterrupt() //ram's comment - changed the return type to void.
{
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() //ram's comment - changed the return type to void.
{
	int ackchecksum = 0;
	int i;
	//initialize SR receiver base and next sequence number
	srReceiverBase = 1;
	srReceiverNextSeqNumber = 1;

	//make ACK packet with Sequence and Ack number zero
	srReceiverBuffer[0].seqnum = 0;
	srReceiverBuffer[0].acknum = 0;

	// fill the pay-load with 1s
	//calculate the value for pay-load
	for ( i=0 ; i<20 ;i++)
	{
		srReceiverBuffer[0].payload[i] = '1';
		ackchecksum = ackchecksum + srReceiverBuffer[0].payload[i];
	}
	//add the acknum and sequence number to ackchecksum
	ackchecksum = ackchecksum + srReceiverBuffer[0].acknum + srReceiverBuffer[0].seqnum;
	// set this checksum in ACK packet
	srReceiverBuffer[0].checksum = ackchecksum;


}

int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand() 
{
  double mmm = 2147483647;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  float x;                   /* individual students may need to change mmm */ 
  x = rand()/mmm;            /* x should be uniform in [0,1] */
  return(x);
}  


/*****************************************************************
***************** NETWORK EMULATION CODE IS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/



/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1


struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */


void insertevent(struct event *p)
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",time_local);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime); 
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q; 
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}





/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival()
{
   double x,log(),ceil();
   struct event *evptr;
//    //char *malloc();
   float ttime;
   int tempint;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

   x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */

   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time_local + x;
   evptr->evtype =  FROM_LAYER5;
   if (BIDIRECTIONAL && (jimsrand()>0.5) )
      evptr->eventity = B;
    else
      evptr->eventity = A;
   insertevent(evptr);
}





void init(int seed)                         /* initialize the simulator */
{
  int i;
  float sum, avg;
  float jimsrand();
  
  
   printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
   printf("Enter the number of messages to simulate: ");
   scanf("%d",&nsimmax);
   printf("Enter  packet loss probability [enter 0.0 for no loss]:");
   scanf("%f",&lossprob);
   printf("Enter packet corruption probability [0.0 for no corruption]:");
   scanf("%f",&corruptprob);
   printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
   scanf("%f",&lambda);
   printf("Enter TRACE:");
   scanf("%d",&TRACE);

   srand(seed);              /* init random number generator */
   sum = 0.0;                /* test random number generator for students */
   for (i=0; i<1000; i++)
      sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
   avg = sum/1000.0;
   if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n" ); 
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(0);
    }

   ntolayer3 = 0;
   nlost = 0;
   ncorrupt = 0;

   time_local=0;                    /* initialize time to 0.0 */
   generate_next_arrival();     /* initialize event list */
}






//int TRACE = 1;             /* for my debugging */
//int nsim = 0;              /* number of messages from 5 to 4 so far */ 
//int nsimmax = 0;           /* number of msgs to generate, then stop */
//float time = 0.000;
//float lossprob;            /* probability that a packet is dropped  */
//float corruptprob;         /* probability that one bit is packet is flipped */
//float lambda;              /* arrival rate of messages from layer 5 */   
//int   ntolayer3;           /* number sent into layer 3 */
//int   nlost;               /* number lost in media */
//int ncorrupt;              /* number corrupted by media*/

/**
 * Checks if the array pointed to by input holds a valid number.
 *
 * @param  input char* to the array holding the value.
 * @return TRUE or FALSE
 */
int isNumber(char *input)
{
    while (*input){
        if (!isdigit(*input))
            return 0;
        else
            input += 1;
    }

    return 1;
}

int main(int argc, char **argv)
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;
   
   int i,j;
   char c; 
  
   int opt;
   int seed;

   //Check for number of arguments
   if(argc != 5){
    fprintf(stderr, "Missing arguments\n");
  printf("Usage: %s -s SEED -w WINDOWSIZE\n", argv[0]);
    return -1;
   }

   /* 
    * Parse the arguments 
    * http://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html 
    */
   while((opt = getopt(argc, argv,"s:w:")) != -1){
       switch (opt){
           case 's':   if(!isNumber(optarg)){
                           fprintf(stderr, "Invalid value for -s\n");
                           return -1;
                       }
                       seed = atoi(optarg);
                       break;

           case 'w':   if(!isNumber(optarg)){
                           fprintf(stderr, "Invalid value for -w\n");
                           return -1;
                       }
                       WINSIZE = atoi(optarg);
                       break;

           case '?':   break;

           default:    printf("Usage: %s -s SEED -w WINDOWSIZE\n", argv[0]);
                       return -1;
       }
   }
  
   init(seed);
   A_init();
   B_init();
   
   while (1) {
        eventptr = evlist;            /* get next event to simulate */
        if (eventptr==NULL)
           goto terminate;
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist!=NULL)
           evlist->prev=NULL;
        if (TRACE>=2) {
           printf("\nEVENT time: %f,",eventptr->evtime);
           printf("  type: %d",eventptr->evtype);
           if (eventptr->evtype==0)
	       printf(", timerinterrupt  ");
             else if (eventptr->evtype==1)
               printf(", fromlayer5 ");
             else
	     printf(", fromlayer3 ");
           printf(" entity: %d\n",eventptr->eventity);
           }
        time_local = eventptr->evtime;        /* update time to next event time */
        if (nsim==nsimmax)
	  break;                        /* all done with simulation */
        if (eventptr->evtype == FROM_LAYER5 ) {
            generate_next_arrival();   /* set up future arrival */
            /* fill in msg to give with string of same letter */    
            j = nsim % 26; 
            for (i=0; i<20; i++)  
               msg2give.data[i] = 97 + j;
            if (TRACE>2) {
               printf("          MAINLOOP: data given to student: ");
                 for (i=0; i<20; i++) 
                  printf("%c", msg2give.data[i]);
               printf("\n");
	     }
            nsim++;
            if (eventptr->eventity == A) 
               A_output(msg2give);  
             else
               B_output(msg2give);  
            }
          else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i=0; i<20; i++)  
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
	    if (eventptr->eventity ==A)      /* deliver packet by calling */
   	       A_input(pkt2give);            /* appropriate entity */
            else
   	       B_input(pkt2give);
	    free(eventptr->pktptr);          /* free the memory for packet */
            }
          else if (eventptr->evtype ==  TIMER_INTERRUPT) {
            if (eventptr->eventity == A) 
	       A_timerinterrupt();
             else
	       B_timerinterrupt();
             }
          else  {
	     printf("INTERNAL PANIC: unknown event type \n");
             }
        free(eventptr);
        }

terminate:
   //Do NOT change any of the following printfs
   printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time_local,nsim);

   printf("\n");
   printf("Protocol: ABT\n");
   printf("[PA2]%d packets sent from the Application Layer of Sender A[/PA2]\n", A_application);
   printf("[PA2]%d packets sent from the Transport Layer of Sender A[/PA2]\n", A_transport);
   printf("[PA2]%d packets received at the Transport layer of Receiver B[/PA2]\n", B_transport);
   printf("[PA2]%d packets received at the Application layer of Receiver B[/PA2]\n", B_application);
   printf("[PA2]Total time: %f time units[/PA2]\n", time_local);
   printf("[PA2]Throughput: %f packets/time units[/PA2]\n", B_application/time_local);
   return 0;
}


/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
 
/*void generate_next_arrival()
{
   double x,log(),ceil();
   struct event *evptr;
    //char *malloc();
   float ttime;
   int tempint;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
 
   x = lambda*jimsrand()*2;  // x is uniform on [0,2*lambda] 
                             // having mean of lambda       
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + x;
   evptr->evtype =  FROM_LAYER5;
   if (BIDIRECTIONAL && (jimsrand()>0.5) )
      evptr->eventity = B;
    else
      evptr->eventity = A;
   insertevent(evptr);
} */




void printevlist()
{
  struct event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB)
 //AorB;  /* A or B is trying to stop timer */
{
 struct event *q,*qold;

 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time_local);
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
       /* remove this event */
       if (q->next==NULL && q->prev==NULL)
             evlist=NULL;         /* remove first and only event on list */
          else if (q->next==NULL) /* end of list - there is one in front */
             q->prev->next = NULL;
          else if (q==evlist) { /* front of list - there must be event after */
             q->next->prev=NULL;
             evlist = q->next;
             }
           else {     /* middle of list */
             q->next->prev = q->prev;
             q->prev->next =  q->next;
             }
       free(q);
       return;
     }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


void starttimer(int AorB,float increment)
// AorB;  /* A or B is trying to stop timer */

{

 struct event *q;
 struct event *evptr;
 ////char *malloc();

 if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",time_local);
 /* be nice: check to see if timer is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
   for (q=evlist; q!=NULL ; q = q->next)  
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }
 
/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time_local + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
} 


/************************** TOLAYER3 ***************/
void tolayer3(int AorB,struct pkt packet)
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 ////char *malloc();
 float lastime, x, jimsrand();
 int i;


 ntolayer3++;

 /* simulate losses: */
 if (jimsrand() < lossprob)  {
      nlost++;
      if (TRACE>0)    
	printf("          TOLAYER3: packet being lost\n");
      return;
    }  

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */ 
 mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
 mypktptr->seqnum = packet.seqnum;
 mypktptr->acknum = packet.acknum;
 mypktptr->checksum = packet.checksum;
 for (i=0; i<20; i++)
    mypktptr->payload[i] = packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
	  mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<20; i++)
        printf("%c",mypktptr->payload[i]);
    printf("\n");
   }

/* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
 lastime = time_local;
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) ) 
      lastime = q->evtime;
 evptr->evtime =  lastime + 1 + 9*jimsrand();
 


 /* simulate corruption: */
 if (jimsrand() < corruptprob)  {
    ncorrupt++;
    if ( (x = jimsrand()) < .75)
       mypktptr->payload[0]='Z';   /* corrupt payload */
      else if (x < .875)
       mypktptr->seqnum = 999999;
      else
       mypktptr->acknum = 999999;
    if (TRACE>0)    
	printf("          TOLAYER3: packet being corrupted\n");
    }  

  if (TRACE>2)  
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
} 

void tolayer5(int AorB,char *datasent)
{
  
  int i;  
  if (TRACE>2) {
     printf("          TOLAYER5: data received: ");
     for (i=0; i<20; i++)  
        printf("%c",datasent[i]);
     printf("\n");
   }
  
}
