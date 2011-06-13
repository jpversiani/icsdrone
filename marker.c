/*
icsdroneng, Copyright (c) 2008-2009, Michel Van den Bergh
All rights reserved

This version contains code from polyglot and links against readline. 
Hence it now falls under the GPL2+
*/
/* Utilities for implementing poorman's blockmode. Official blockmode
 * is not compatible with timeseal and seems to have other quirks as
 * well.
 */

#include "icsdrone.h"

static char * cookie=NULL;

void SendMarker(char * marker){
  if(!cookie){  
    cookie=malloc(20);
    sprintf(cookie,"%ld",(long int) random());
  }
  SendToIcs("%s_%s\n", marker,cookie);
}

Bool IsMarker(char* marker, char *line){
  char command[63+1];
  char dummy;
  if(!cookie){
      return FALSE;
  }
  if (2==sscanf(line, "%63[^:]: Command not found%c", command, &dummy) // FICS
   || 2==sscanf(line, "No such command (%63[^)])%c", command, &dummy)) { // ICC
    return !strncmp(command,marker,strlen(marker)) && strstr(command,cookie);
  } else {
    return FALSE;
  }
}

Bool IsAMarker(char *line){
  char command[63+1];
  char dummy;
  if(!cookie){
      return FALSE;
  }
  if (2==sscanf(line, "%63[^:]: Command not found%c", command, &dummy) // FICS
   || 2==sscanf(line, "No such command (%63[^)])%c", command, &dummy)) { // ICC
    return strstr(command,cookie)!=NULL;
  } else {
    return FALSE;
  }
}
