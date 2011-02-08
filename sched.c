/*
icsdroneng, Copyright (c) 2008-2009, Michel Van den Bergh
All rights reserved

This version contains code from polyglot and links against readline. 
Hence it now falls under the GPL2+
*/
#include "icsdrone.h"

#define SOME_BIG_NUMBER 1000000

typedef struct event_list_entry *event_list_t; 

struct event_list_entry{
    event_t event;
    event_list_t next_entry;
};

static event_list_t event_list=NULL;

event_t new_event(struct timeval time, void (*handler)(void *data), void *data){
    event_t event;
    event=(event_t) malloc(sizeof *event);
    event->time=time;
    event->handler=handler;
    event->data=data;
    event->has_fired=0;
    return event;
}

void log_event(event_t event){
    logme(LOG_DEBUG,"event=%p time=%d.%06d data=%p handler=%p\n",
	  event,
          (int) event->time.tv_sec,
          (int) event->time.tv_usec,
          event->data,
          event->handler);
}

void log_event_list(){
    event_list_t event_list_c=event_list;
    logme(LOG_DEBUG,"Begin eventlist");
    while(event_list_c){
        logme(LOG_DEBUG,"entry=%p next_entry=%p ",
              event_list_c,event_list_c->next_entry);
        log_event(event_list_c->event);
        event_list_c=event_list_c->next_entry;
    }
    logme(LOG_DEBUG,"End eventlist");
}

void insert_event(event_t event){
    event_list_t prev_event_list=NULL;
    event_list_t event_list_c=event_list;
    int found=0;
    while(!found && event_list_c){
        if(time_gt(event_list_c->event->time,event->time)){
            if(prev_event_list){
                prev_event_list->next_entry=
                    (event_list_t)malloc(sizeof *event_list_c);
                prev_event_list->next_entry->event=event;
                prev_event_list->next_entry->next_entry=event_list_c;
            }else{
                prev_event_list=event_list_c;
                event_list_c=(event_list_t) malloc(sizeof *event_list_c);
                event_list_c->event=event;
                event_list_c->next_entry=prev_event_list;
                event_list=event_list_c;
            }
            found=1;
            break;
        }
        prev_event_list=event_list_c;
        event_list_c=event_list_c->next_entry;
    }
    if(!found){
        if(prev_event_list){
            prev_event_list->next_entry=
                (event_list_t)malloc(sizeof *event_list_c);
            prev_event_list->next_entry->event=event;
            prev_event_list->next_entry->next_entry=event_list_c;
        }else{
            prev_event_list=event_list_c;
            event_list_c=(event_list_t) malloc(sizeof *event_list_c);
            event_list_c->event=event;
            event_list_c->next_entry=prev_event_list;
            event_list=event_list_c;
        }
    }
}

void remove_event(event_t event){
  event_list_t event_list_c=event_list;
  event_list_t prev_event_list=NULL;
  int found=0;
  while(!found && event_list_c){
    if(event_list_c->event==event){
      found=1;
      if(prev_event_list){
	prev_event_list->next_entry=event_list_c->next_entry;
	free(event_list_c);
      }else{
	prev_event_list=event_list_c->next_entry;
	free(event_list_c);
	event_list=prev_event_list;
      }
      break;
    }
    prev_event_list=event_list_c;
    event_list_c=event_list_c->next_entry;
  }
}





void create_timer(event_t *timer, 
                  int time, /* in msec */
                  void (*handler)(void *), 
                  void *data){
    event_t event;
    struct timeval tv;
    if(*timer){
        logme(LOG_DEBUG,"Creating timer over non-NULL pointer."
              "Calling \"delete_timer\".");
        delete_timer(timer);
    }
    gettimeofday(&tv,NULL);
    event=new_event(time_add(tv,time),handler,data);
    insert_event(event);
    *timer=event;
}

void delete_timer(event_t *timer){
    if(!(*timer)){
      logme(LOG_DEBUG,"Deleting NULL timer. Ignoring.");
      return;
    }
    remove_event(*timer);
    free(*timer);
    *timer=NULL;
}



void worker(){
  event_t event;
  event_list_t event_list_c=event_list;
  event=event_list->event;
  event_list=event_list->next_entry;
  free(event_list_c);
  event->has_fired=1;
  (event->handler)(event->data);
}

int process_timer_list(){
  struct timeval tv;
  int nr_of_events=0;
  gettimeofday(&tv,NULL);
  while(event_list && time_ge(tv,event_list->event->time)){
    worker();
    nr_of_events++;
  }
  return nr_of_events;
}

struct timeval sched_idle_time(){
  struct timeval tv;
  struct timeval timeout;
  gettimeofday(&tv,NULL);
  if(event_list){
      timeout=time_diff(event_list->event->time,tv);
      if (timeout.tv_sec<0){
          timeout.tv_sec=0;
          timeout.tv_usec=0;
      }
  }else{
      timeout.tv_sec=SOME_BIG_NUMBER;
      timeout.tv_usec=0;
  }
  return timeout;
}


