#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <json.h>
#include "pubnub.h"
#include "app.h"
#include "pubnub-sync.h"

#define NUM_CHANNELS 5

char server[][25] = {"pubsub-emea.pubnub.com", 
                     "pubsub-eunor.pubnub.com", 
                     "pubsub-apac.pubnub.com", 
                     "pubsub-naatl.pubnub.com", 
                     "pubsub-napac.pubnub.com"};

int64_t get_pubnub_server_ts()
{
        struct pubnub_sync *sync = pubnub_sync_init();
        struct pubnub *p = pubnub_init(
                        /* publish_key */ "demo",
                        /* subscribe_key */ "demo",
                        /* pubnub_callbacks */ &pubnub_sync_callbacks,
                        /* pubnub_callbacks data */ sync);
        json_object *msg;


        pubnub_time( p,
                    -1,
                     NULL,
                     NULL);

        if (pubnub_sync_last_result(sync) != PNR_OK)
                return EXIT_FAILURE;

        msg = pubnub_sync_last_response(sync);
        int64_t ts = json_object_get_int64(msg);
        pubnub_done(p);
        
        return ts;
}

void *publish(void *data)
{
    thread_data  *thrd_data  = (thread_data *)data;
    char         buf[50];
    json_object  *msg;
    unsigned int i           = 0;
    int          cnt         = 0;
    char         chan[50];
    char         *local_time = NULL; 
    char         *channels   = NULL;
    node         *temp       = NULL;
    int          sockfd;

    struct pubnub_sync *sync = pubnub_sync_init();
    struct pubnub *p = pubnub_init("demo", 
                                   "demo",
                                   &pubnub_sync_callbacks,
                                   sync);    
    sockfd = connect_to_server(thrd_data->server_ip);

    local_time = (char *)calloc(50, sizeof(char));
    if (!local_time) {
        DUPPRINT("\nCalloc error\n");
    }
    snprintf(chan, 50, "demo_%s_channel",thrd_data->server_name);
    do {
        msg = json_object_new_object();
        json_object_object_add(msg, "num", json_object_new_int(42));
        i++;
        snprintf(buf, 50, "[%"PRId64"]:This is msg %d ",get_pubnub_server_ts(), i);
        json_object_object_add(msg, "str", json_object_new_string(buf));
        /* msg = { "num": 42, "str": "\"Hello, world!\" she said." } */

        pubnub_publish(
                        /* struct pubnub */ p,
                        /* channel */ chan,/*"demo_pubsub-emea.pubnub.com_channel",*/
                        /* message */ msg,
                        /* default timeout */ -1,
                        /* callback; sync needs NULL! */ NULL,
                        /* callback data */ NULL);

        json_object_put(msg);


        if (pubnub_sync_last_result(sync) != PNR_OK){
            cnt++;   
            continue;
        }

        msg = pubnub_sync_last_response(sync);
        get_local_time(&local_time);
        DUPPRINT("\n%s:publish (connected to %s) from %s to %s(%s): %s\n", local_time,
               thrd_data->server_ip, get_my_ip(), thrd_data->server_ip, thrd_data->server_name, buf);
        json_object_put(msg);
        if (!monitor_connection(sockfd, thrd_data->server_ip)) {
            DUPPRINT("\nReconnecting to server at %s\n",thrd_data->server_ip);
            sockfd = connect_to_server(thrd_data->server_ip);
        }
        cnt++;
        sleep(5);
        } while( cnt < 5);
       
        free(local_time);
        pubnub_done(p); 
        pthread_exit(NULL);
}

void *subscribe(void *data)
{
    thread_data *thrd_data = (thread_data *)data;
    const char *channel;
    char *local_time = NULL;
    int  i = 0; 
    int  cnt = 0;
    int  sockfd; 
    node *temp = NULL;
    json_object *msg;
    int64_t epoch = 0;


    struct pubnub_sync *sync = pubnub_sync_init();
    struct pubnub *p = pubnub_init("demo", 
                                   "demo",
                                   &pubnub_sync_callbacks,
                                   sync);   

    sockfd = connect_to_server(thrd_data->server_ip);
    channel = (char *)calloc(50, sizeof(char *));
    snprintf((char *)channel, 50, "demo_%s_channel",thrd_data->server_name);

    local_time = (char *)calloc(50, sizeof(char));
    if (!local_time) {
        DUPPRINT("\nCalloc error\n");
    }

    do {
        pubnub_subscribe_multi(p,
                               &channel,
                               1,
                               -1,
                               NULL,
                               NULL);
        if (pubnub_sync_last_result(sync) == PNR_TIMEOUT) {
            printf("\nsubscribe (connected to %s):%s Didnt receive message after TIMEOUT sec\n", thrd_data->server_ip,
                    thrd_data->server_name);
            
            continue;
        }
        if (pubnub_sync_last_result(sync) != PNR_OK) {
            continue;
        }
  
        msg = pubnub_sync_last_response(sync);
        get_local_time(&local_time);

        if (json_object_array_length(msg) == 0) {
            DUPPRINT("\n%s:subcribe %s OK. No news \n",local_time,thrd_data->server_name);
        } else {
            char **msg_channels = pubnub_sync_last_channels(sync);
            for (i = 0; i < json_object_array_length(msg); i++) {
                 json_object *msg1 = json_object_array_get_idx(msg, i);
                 epoch = get_publisher_timestamp(json_object_get_string(msg1));
                 DUPPRINT("\n%s:subscribe (connected to %s)[%s]:%s %sRcvd msg in %"PRId64"ms\n", 
                          local_time, thrd_data->server_ip,msg_channels[i], thrd_data->server_name, 
                          json_object_get_string(msg1),(get_pubnub_server_ts() - epoch)/1000);
            }
        }

        json_object_put(msg);
        if (!monitor_connection(sockfd, thrd_data->server_ip)) {
            DUPPRINT("Reconnecting to server at %s\n",thrd_data->server_ip);
            sockfd = connect_to_server(thrd_data->server_ip);
        }
        cnt++;
        sleep(1);
    }while(1);

    free(local_time);
    free(thrd_data);
    free((void *)channel);
    pubnub_done(p);
    pthread_exit(NULL);
    
}

bool get_user_preference(channels my_channels[], user_pref *data)
{
    int  i              = 0;
    int  data_center    = 0;
    int  data_center_ip = 0;
    int  total_ip       = 0;
    char *usr_ip        = (char *)calloc(50, sizeof(char));
    node *temp          = NULL;
    
    printf("\n\n"); 
    printf("\n----------------------------------------------------------\n");
    printf("\n--------------Pubnub is changing the world----------------\n");
    printf("\n----------------------------------------------------------\n");
    printf("\n\n"); 

    printf("\nThese are the DataCenters across which the suite will run\n");
    for (i = 0; i <NUM_CHANNELS; i++) {
         printf("\n%d: %s\n",i+1, my_channels[i].server_name);
    }
    printf("\nDo you want to use these DataCenter's or specify one of your own?\n");
    if (!get_user_input()) {
        printf("\nPlease enter the new datacenter name\n");
        scanf("%s",usr_ip);
        getchar();
        data->server_name = usr_ip;
        return false;
    }
    printf("\nDo you want to publish/sucbscribe to all IP's across all DataCentres?\n");
    
    if (get_user_input()) {
        return true;
    } 
 
    printf("\nSelect a Datacenter from the below list to Publish/Subscribe\n");
    printf("\nEnter the corresponding number to choose the particular DataCenter\n");
    for (i = 0; i <NUM_CHANNELS; i++) {
         printf("\n%d: %s\n",i+1, my_channels[i].server_name);
    }
Again:
    scanf("%d",&data_center);
    getchar();
    if ( !data_center || data_center > NUM_CHANNELS) {
        printf("\nInvalid entry. Please enter a valid value between 1 & %d\n",NUM_CHANNELS);
        goto Again;
    }
    printf("\nDo you wish to Publish/Susbcribe to all IP's at %s datacenter?\n", 
            my_channels[data_center - 1].server_name);
    
    data->server_name = my_channels[data_center - 1].server_name;
    if (get_user_input()) {
        data->server_name = my_channels[data_center - 1].server_name;
        data->head        = my_channels[data_center - 1].head;
        return false;
    }

    printf("\nSelect which all IP's within %s datacenter you want to Publish/Subscribe\n", 
             my_channels[data_center -1].server_name);
    temp = my_channels[data_center - 1].head;
    i = 1;
    while (temp) {
           printf("\n%d: %s\n", i, temp->host);    
           i++;
           temp = temp->next;
    }
    total_ip = i - 1; 
Input:
        printf("\nPlease enter a value between 1 & %d\n",total_ip );
        fseek(stdin, 0, SEEK_END);
        scanf("%d",&data_center_ip);
        getchar();
        if (!data_center_ip || data_center_ip > total_ip) {
            printf("\nInvalid Entry. Please enter a valid value between 1 & %d\n",total_ip );
            goto Input;
        } else {
            if (!add_user_node(my_channels[data_center - 1].head, 
                               &data->head, data_center_ip)) {
                printf("\nYou have already added this ip. "
                        "Kindly select a different IP to Publish/Subscribe\n");
                goto Input;
            } else {
               printf("\nDo you want to add more IP's within %s datacenter\n",
                       my_channels[data_center -1].server_name);
               fseek(stdin, 0, SEEK_END);
               if (get_user_input()) {
                   goto Input;
               } else {
                   return false;
               }
               
            }
        }

}

int
main(void)
{
        int         i            = 0;
        int         j            = 0;
        int         thrd_id      = 0;
        int         rc           = 0;
        int         num_address  = 0;
        node        *head        = NULL;
        char        *logfile     = NULL;
        int         num_threads  = 0;
        int         num_chans    = 0;
        pthread_t   *threads     = NULL;
        channels    my_channels[NUM_CHANNELS];
        thread_data *data        = NULL;
        user_pref   user_data    = {NULL, NULL};
        int         total_ip     = 0;
        bool        no_user_data = true;
        void        *status;

        printf("\n----------------------------------------------------------\n");
        printf("\n---Welcome to PubNub Automation Test Suite----\n");
        printf("\n----------------------------------------------------------\n");
         
        logfile = (char *)calloc(50, sizeof(char));
        if (!logfile) {
            DUPPRINT("\nCalloc error\n");
            exit(0);
        }
        get_logfile_name(&logfile);
        fp = fopen(logfile, "w");
        if (!fp) {
            perror(logfile);
            perror("fopen");
            exit(0);
        }
        fp = freopen(logfile, "a+", stderr);

        for (i = 0; i < NUM_CHANNELS; i++) {
             my_channels[i] = get_channel_data(server[i]);
             num_threads += count_node(my_channels[i].head);
        }

        no_user_data = get_user_preference(my_channels, &user_data);
        if (!no_user_data) {
            if (!user_data.head) {
                user_data.head = get_server_address(user_data.server_name);
                if (!user_data.head) {
                    DUPPRINT("\nERROR: Datacenter input by user is unresolvable. Aborting suite\n");
                    exit(0);
                }
            }
            num_threads = count_node(user_data.head);
            num_chans = 1;
        } else {
            num_chans = NUM_CHANNELS;
        }

        num_threads *= 2;
        threads = (pthread_t *)malloc(sizeof(pthread_t)*num_threads);
        if (!threads) {
            printf("\nMalloc error\n");
            exit(0);
        }
 
        for (i = 0; i < num_chans; i++) {
             if (no_user_data) {
                 num_address = count_node(my_channels[i].head);
             } else {
                 num_address = count_node(user_data.head);
             }
             for (j = 0; j < (num_address); j++) {
                  if (no_user_data) {
                      data = get_thrd_data(&my_channels[i], j);
                  } else {
                      data = get_thrd_data((channels *)&user_data, j);
                  }
                  rc = pthread_create(&threads[thrd_id], NULL, publish, (void *)data);
                  if (rc) {
                      printf("\nError in pthread_create %d\n", rc);
                      exit(0);
                  }
                  thrd_id++;
                  rc = pthread_create(&threads[thrd_id], NULL, subscribe, (void *)data);
                  if (rc) {
                      printf("\nError in pthread_create %d\n", rc);
                      exit(0);
                  }
                  thrd_id++;
             }
        }

        for (i = 0; i < num_threads ; i++) {
             rc = pthread_join(threads[i], &status);
             if (rc) {
                 DUPPRINT("ERROR; return code thread %d from pthread_join() is %d\n", i, rc);
                 exit(-1);
             }
             DUPPRINT("Main: completed join with thread %d having a status of %ld\n",i,(long)status);
        }

        for (i = 0; i < NUM_CHANNELS; i++) {
             free_list(&my_channels[i].head);
        }

        if (!no_user_data && user_data.head) {
            free_list(&user_data.head);
        }

        free(threads);
        free(logfile);

        fclose(fp);
	return EXIT_SUCCESS;
}
