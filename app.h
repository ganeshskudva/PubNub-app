#ifndef _app_h_
#define _app_h_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pubnub.h"
#include <netdb.h>		
#include <stdio.h>	
#include <stdlib.h>	
#include <string.h>

#define DUPPRINT(fmt...)        do {printf(fmt);fprintf(fp,fmt);} while(0)
#define SOCK_ADDR_IN_PTR(sa)    ((struct sockaddr_in *)(sa))
#define SOCK_ADDR_IN_ADDR(sa)   SOCK_ADDR_IN_PTR(sa)->sin_addr
#define PORT                    80
#define BUFSIZE                 1024
#define TIMEOUT                 5

FILE *fp;

/* 
 * Linked List node structure
 */
typedef struct _node_
{
    struct sockaddr *addr; 
    char            *host; /*This holds the ip address*/
    struct _node_   *next;
}node;

/* 
 * Info related to each data_center 
 * are stored in these structure 
 */
typedef struct _channels_
{
    char            *server_name; /*Data Center name (Ex:pubsub-emea.pubnub.com)*/
    node            *head;        /*Linked List of nodes containing ip addresses of this Data Center*/
}channels;

/* 
 * Data passed to individual threads are stored here 
 */
typedef struct _thread_data_
{
    char     *server_name; 
    char     *server_ip;
}thread_data;

/* 
 * This holds the data entered by user 
 */
typedef struct _user_pref_
{
    char   *server_name;
    node   *head;
}user_pref;

/*Linked List related api's*/
node *alloc_node();
bool add_node(node **, struct sockaddr *, char *);
bool add_user_node(node   *, node   **,int );
void disp(node *);
int count_node(node *); 
void free_list(node **head);

node *dns_lookup(const char **);
char *get_my_ip();
thread_data *get_thrd_data(channels *, int );
int64_t get_timestamp();
int64_t get_publisher_timestamp(const char *);
bool get_user_input();
int connect_to_server(char *);
bool monitor_connection(int , char *);
void get_logfile_name(char **);
void get_local_time(char **);
node *get_server_address(const char *);
channels get_channel_data(char *);
int sock_addr_cmp_addr(const struct sockaddr *,const struct sockaddr *);

/*
 * Function   : alloc_node
 * Description: Allocates a node to be added to Linked Listchannels 
 * Input      : NULL
 * Return     : Pointer to allocated node on success, NULL on failure  
 */
node *alloc_node()
{
    node *temp = NULL;

    temp = (node *)calloc(1, sizeof(node));
    if (!temp) {
        printf("\nNo memory\n");
        exit(0);
    }

    temp->host = (char *)calloc(NI_MAXHOST, sizeof(char));
    return temp;
}

/*
 * Function   : alloc_node
 * Description: Allocates a node to be added to Linked Listchannels 
 * Input      : NULL
 * Return     : Pointer to allocated node on success, NULL on failure  
 */
void free_list(node **head)
{
    node *temp = NULL;

    if (!(*head)) {
        return;
    }

    temp = *head;
    while (*head) {
           *head = (*head)->next;
           free(temp);
           temp = *head;
    }
}
/*
 * Function   : sock_addr_cmp_addr 
 * Description: Compares two ipv4 addresses 
 * Input      : Two sockaddr struct's to compare
 * Return     : 0 if two addresses are same    
 */
int sock_addr_cmp_addr(const struct sockaddr * sa,
                       const struct sockaddr * sb)
{
   if (sa->sa_family != sb->sa_family)
       return (sa->sa_family - sb->sa_family);

   if (sa->sa_family == AF_INET) {
       return (SOCK_ADDR_IN_ADDR(sa).s_addr - SOCK_ADDR_IN_ADDR(sb).s_addr);
   } else {
       printf("sock_addr_cmp_addr: unsupported address family %d",
                 sa->sa_family);
   }
}

/*
 * Function   : add_node
 * Description: Adds a node to the Linked List. Also ensures that
 *              there are no duplicates in the given list 
 * Input      : Pointer to list head & the ip address
 * Return     : true on success & false on failure  
 */
bool add_node(node            **head,
             struct sockaddr *in_addr,
             char            host[])
{
    node *temp     = NULL;
    node *prev     = NULL;
    node *new_node = NULL;

    if (!(*head)) {
        temp = alloc_node();
        temp->addr = in_addr;
        memcpy(temp->host, host, NI_MAXHOST);
        temp->next = NULL;
        (*head) = temp;
        return true;
    }

    temp = *head;
    while (temp) {
        if (!sock_addr_cmp_addr(temp->addr, in_addr)) {
            return false;
        }
        prev = temp;
        temp = temp->next;
    }

    new_node = alloc_node();
    new_node->addr = in_addr;
    memcpy(new_node->host, host, NI_MAXHOST);
    new_node->next = NULL;
    prev->next = new_node;

    return true;
}

/*
 * Function   : add_user_node
 * Description: Adds a node which user has specified to the Linked List. 
 *              Also ensures that there are no duplicates in the given list 
 * Input      : Pointer to search list, list head & the position of the node
 *              in the search linked list which needs to be added into user
 *              linked list
 * Return     : true on success & false on failure  
 */
bool add_user_node(node   *search_head,
                   node   **head,
                   int    val) 
{
    int  i     = 0;
    node *temp = NULL;

    temp = search_head;
    for (i = 0; i < (val - 1); i++) {
         temp = temp->next;
    }
    
    if (temp) {
        return add_node(head, temp->addr, temp->host);
    } else {
        return false;
    }
    
}

/*
 * Function   : disp
 * Description: Display the contents of the nodes of a linked list 
 * Input      : Pointer to list head 
 * Return     : void  
 */
void disp(node *head)
{
    node *temp = NULL;

    if (!head) {
        printf("\nllist empty\n");
        return;
    }

    temp = head;

    while (temp) {
           printf("\n%s\n",temp->host);
           temp = temp->next;
    }
}

/*
 * Function   : count_node 
 * Description: Counts the number of nodes in the given linked list 
 * Input      : Pointer to list head
 * Return     : Total number of nodes
 */
int count_node(node *head)
{
    node *temp = NULL;
    int  count = 0;
 
    if (!head) {
        return count;
    }

    temp = head;
    while (temp) {
           count++;
           temp = temp->next;
    }

    return count;
}

/*
 * Function   : dns_lookup 
 * Description: Does a dns lookup of the given name 
 * Input      : Name of the server to be looked up
 * Return     : Head node of the list containing ip adddresses 
 *              resolving that name
 */
node *dns_lookup(const char **server)
{
    struct addrinfo hints, *res, *res0;
    int error, i;
    char host[NI_MAXHOST];
    node *head = NULL;
 
    /*
     * Request only one socket type from getaddrinfo(). Else we
     * would get both SOCK_DGRAM and SOCK_STREAM, and print two
     * copies of each numeric address.
     */
     memset(&hints, 0, sizeof hints);
     hints.ai_family = PF_UNSPEC;     /* IPv4, IPv6, or anything */
     hints.ai_socktype = SOCK_DGRAM;  /* Dummy socket type */
 
      for (i = 0; i < 15; i++) {
           error = getaddrinfo(*server, NULL, &hints, &res0);
           if (error) {
               fprintf(stderr, "%s\n", gai_strerror(error));
               return NULL;
           }
 
           for (res = res0; res; res = res->ai_next) {
		/*
		 * Use getnameinfo() to convert res->ai_addr to a
		 * printable string.
		 *
		 * NI_NUMERICHOST means to present the numeric address
		 * without doing reverse DNS to get a domain name.
		 */
		error = getnameinfo(res->ai_addr, res->ai_addrlen,
		    host, sizeof host, NULL, 0, NI_NUMERICHOST);
		if (error) {
			fprintf(stderr, "%s\n", gai_strerror(error));
		} else {
                        if (!add_node(&head, res->ai_addr, host)) {
                            break;
                        }
		}
           }
      }
 
      freeaddrinfo(res0);
 
      return head;
}

/*
 * Function   : get_my_ip 
 * Description: Get the ip address of the localhost  
 * Input      : void
 * Return     : Pointer to ip address of localhost
 */
char *get_my_ip()
{
    int sock, i;
    struct ifreq ifreqs[20];
    struct ifconf ic;

    ic.ifc_len = sizeof ifreqs;
    ic.ifc_req = ifreqs;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    if (ioctl(sock, SIOCGIFCONF, &ic) < 0) {
        perror("SIOCGIFCONF");
        exit(1);
    }

    for (i = 0; i < ic.ifc_len/sizeof(struct ifreq); ++i) {
         if (!strcmp(ifreqs[i].ifr_name, "lo")) {
             continue;
         }
         return (inet_ntoa(((struct sockaddr_in*)&ifreqs[i].ifr_addr)->sin_addr));
    }

    return NULL;
}

/*
 * Function   : get_thrs_data 
 * Description: Fills the data which is passed on to 
 *              individual threads  
 * Input      : Pointer to channels struct & the index 
 *              of the node in the linked list whose ip 
 *              address needs to be copied
 * Return     : Pointer to thread_data struct
 */
thread_data *get_thrd_data(channels *chan, int val)
{
    int  i     = 0;
    node *temp = NULL;
    thread_data *data;

    data = (thread_data *)calloc(1, sizeof(thread_data));
    if (!data) {
        printf("\nCalloc error\n");
        exit(0);
    }

    data->server_name = chan->server_name;
    data->server_ip   = NULL;
    temp = chan->head;
    for (i = 0; i < val; i++) {
         temp = temp->next;
    }

    if (temp) {
        data->server_ip = temp->host;
    }
    
    return data; 
}

/*
 * Function   : get_timestamp 
 * Description: Computes the UNIX epoch, based on localhost time  
 * Input      : void
 * Return     : Calculated epoch
 */
int64_t get_timestamp()
{
    struct timeval tv;
    int64_t epoch = 0;

    gettimeofday(&tv, NULL);
    epoch = (uint64_t)10000000 * tv.tv_sec + tv.tv_usec;

    return epoch;
}

/*
 * Function   : get_publisher_timestamp 
 * Description: Fetches publisher's timestamp from the received message  
 * Input      : Pointer to the received message
 * Return     : Publisher's timestamp
 */
int64_t get_publisher_timestamp(const char *str)
{
    int64_t epoch = 0;
    int     i     = 22;

    while (i < 39) {
           epoch = epoch*10 + str[i] - '0';
           i++;
    }

    return epoch;
}

/*
 * Function   : get_user_input 
 * Description: Read user's input keys  
 * Input      : void
 * Return     : true on (Yes/Y/y) & false on (No/N/n)
 */
bool get_user_input ()
{
   char ch[3];
   int  c = 0;
   int  i = 0;

   printf("\nPlease enter (Y/y)/(N/n)\n");
Input:
   i = 0;
   while ((c = getchar()) != '\n') {
           ch[i] = c;
           i++;
    }
    ch[i] = '\0';
    while (1) {
           if (i == 1) {
               if (ch[0] == 'Y' || ch[0] == 'y') {
                   return true;
               } else if (ch[0] == 'N' || ch[0] == 'n') {
                   return false;
               } else {
                   printf("\nUnexpected Input. Please Enter (Yes/Y/y)\(No,N,n)\n");
                   goto Input;
               }
           } else if (i == 3) {
               if (!strncmp(ch, "Yes", 3)) {
                   return true;
               } else if (!strncmp(ch, "No", 2)) {
                   return false;
               } else {
                   printf("\nUnexpected Input. Please Enter (Yes/Y/y)\(No,N,n)\n");
                   goto Input;
               }
           } else {
               printf("\nUnexpected Input. Please Enter (Yes/Y/y)\(No,N,n)\n");
               goto Input;
           }
    }
    
    return false;
}

/*
 * Function   : connect_to_server 
 * Description: Establishes a tcp connection to the given server
 * Input      : Name of the server to connect to 
 * Return     : 0 on failure & sockd on success
 */
int connect_to_server(char *srvr)
{
    int    sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char   *hostname;
    int    cnt = 0;

    hostname = srvr;
    portno = PORT;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        error("ERROR connecting");
        close(sockfd);
        return 0;
    }
 
    return sockfd;
}

/*
 * Function   : monitor_connection 
 * Description: Checks the tcp connection between client & server
 * Input      : sockfd & server name
 * Return     : true on healthy connection & false on closed connection
 */
bool monitor_connection(int sockfd, char *server)
{
    char buf[BUFSIZE] = "GET / HTTP/1.1\r\n\r\n";
    int  n     = 0;    
    int  maxfd = 0;    
    int  res   = 0;
    fd_set rset;
    struct timeval timeout;

    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    maxfd = sockfd + 1;
    bzero(buf, BUFSIZE);
    snprintf(buf,1024,"GET / HTTP/1.1\r\n\r\n");
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) { 
      DUPPRINT("ERROR writing to socket");
      close(sockfd);
      return false;
    }

    bzero(buf, BUFSIZE);
    res = select(maxfd, &rset, NULL, NULL, &timeout);
    if (res == 0) {
        DUPPRINT("No response from %s server\n",server);
        DUPPRINT("Closing the connection\n");
        close(sockfd);
        return false;
    } else if (res == 1){
        if (FD_ISSET(sockfd, &rset)) {
            n = read(sockfd, buf, BUFSIZE);
            if (n <= 0) { 
                DUPPRINT("Connection terminated abnormally by %s server\n",server);
                close(sockfd);
                return false;
            }
        }
    } else if (res == -1) {
        DUPPRINT("\nSelect ret ERROR\n");
        close(sockfd);
        return false;
    }

    return true;
}

/*
 * Function   : get_logfile_name 
 * Description: Creates a unique string name by appending the current system time
 * Input      : The buffer where the created string will be copied
 * Return     : void
 */
void get_logfile_name(char **buf)
{   
    time_t t     = time(NULL);
    struct tm tm = *localtime(&t);

    if (!*buf) {
        return;
    }
    snprintf(*buf, 50, "log_%d-%d-%d_%d:%d:%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

}

/*
 * Function   : get_local_time 
 * Description: Calculates the current date & time
 * Input      : The buffer where the created string will be copied
 * Return     : void
 */
void get_local_time(char **buf)
{   
    time_t t     = time(NULL);
    struct tm tm = *localtime(&t);

    if (!*buf) {
        return;
    }
    snprintf(*buf, 50, "%d-%d-%d_%d:%d:%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

}

/*
 * Function   : get_server_address 
 * Description: This is a wrapper function for performing dns query
 * Input      : Name of the server to be looked up
 * Return     : Pointer to head node of the list conatining all the
 *              ip addresses resolving the given server
 */
node *get_server_address(const char server[])
{
    node *head = NULL;

    head = dns_lookup(&server);
    return head;
}

/*
 * Function   : get_channel_data 
 * Description: Fills in the server name & list node
 * Input      : Name of the server 
 * Return     : Filled struct 
 */
channels get_channel_data(char *server)
{
    channels my_channel;

    my_channel.server_name = server;
    my_channel.head = get_server_address(server);
    return my_channel;
}

#endif
