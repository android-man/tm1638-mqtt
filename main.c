#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <stdbool.h>
#include "MQTTClient.h"
#include <bcm2835.h>
#include <tm1638.h>

#define ADDRESS     "127.0.0.1:1883"
#define CLIENTID    "TM1638ClientSub"
#define QOS         0
#define TOPIC_LEDS       "8leds"
#define TOPIC_TEXT       "7segs"

// global pointer to the TM1638
static tm1638_p t;

volatile MQTTClient_deliveryToken deliveredtoken;

void delivered(void * context, MQTTClient_deliveryToken dt) {
   printf("Message with token value %d delivery confirmed\n", dt);
   deliveredtoken = dt;
}

void format_dots(char* m, char** s, int* d){
  *s=malloc( strlen(m) * sizeof(char)  );

  char c;
  int dots=0;
  int j=0;
  char last_c=' ';

  for (int i=0; m[i]!=0 && j<9;i++){
    c=m[i];
    if(c=='.' || c==',') {
      if(j==0 || last_c == '.' ) { // first char is a dot or we repeat a dot
        c=' ';
        (*s)[j++]=c;
      }
      dots |= 1<<(j-1) ;
      last_c='.';
    } else {
        (*s)[j++]=c;
        last_c=c;
    }
  }
  (*s)[j<8?j:8]=0;

  dots &= 255; // limit to 8 bits dots

  *d=dots;
}

int msgarrvd(void * context, char * topicName, int topicLen, MQTTClient_message * message) {
   uint8_t i;
   char * payloadptr;
   char *msg;
   char *s;
   int dots;

   msg=malloc( (message->payloadlen +1) * sizeof(char)  );

   printf("topic: %s, payload:", topicName);

   payloadptr = message->payload;

   for (i = 0; i < message->payloadlen; i++) {
      msg[i] = * payloadptr++; // don't forget ++
   }
   msg[i] = '\0'; // we finish the msg with \0
   printf("%s\n", msg);

   if (strcmp(topicName, TOPIC_LEDS) == 0) {
      int leds = atoi(msg);
      tm1638_set_8leds(t, leds, 0);
   } else if (strcmp(topicName, TOPIC_TEXT) == 0) {
      format_dots( msg, &s, &dots);
      tm1638_set_7seg_text(t, s, dots); //dots=0x00
   }

   MQTTClient_freeMessage( & message);
   MQTTClient_free(topicName);
   return 1;
}

void connlost(void * context, char * cause) {
   printf("\nConnection lost\n");
   printf("     cause: %s\n", cause);
}

int main(int argc, char * argv[]) {

   if (!bcm2835_init()) {
      printf("Unable to initialize BCM library\n");
      return -1;
   }

   t = tm1638_alloc(17, 27, 22);
   if (!t) {
      printf("Unable to allocate TM1638\n");
      return -2;
   }

   MQTTClient client;
   MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
   int rc;
   int ch;

   MQTTClient_create( & client, ADDRESS, CLIENTID,
      MQTTCLIENT_PERSISTENCE_NONE, NULL);
   conn_opts.keepAliveInterval = 20;
   conn_opts.cleansession = 1;

   MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

   if ((rc = MQTTClient_connect(client, & conn_opts)) != MQTTCLIENT_SUCCESS) {
      printf("Failed to connect, return code %d\n", rc);
      exit(-1);
   }

   //printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
   printf("Press Q<Enter> to quit\n");

   MQTTClient_subscribe(client, TOPIC_LEDS, QOS);
   MQTTClient_subscribe(client, TOPIC_TEXT, QOS);

   do {
      ch = getchar();
   } while (ch != 'Q' && ch != 'q');

   MQTTClient_disconnect(client, 10000);
   MQTTClient_destroy( & client);
   return rc;
}
