/*
icsdroneng, Copyright (c) 2008-2009, Michel Van den Bergh
All rights reserved

This version contains code from polyglot and links against readline. 
Hence it now falls under the GPL2+
*/
#include "icsdrone.h"


Bool ParseBoard(IcsBoard *icsBoard, char *line){
  char tmp[73];
  int r,f;
  if (!strncmp(line, "<12> ", 5)){
    if (sscanf(line, "<12> %72c%c%d%d%d%d%d%d%d%17s%17s%d%d%d%d%d%d%d%d%15s%17s%15s%d", 
	       tmp, 
	       &(icsBoard->onMove), 
               &(icsBoard->epFile),
               &(icsBoard->whiteCanCastleShort),
               &(icsBoard->whiteCanCastleLong),
               &(icsBoard->blackCanCastleShort),
               &(icsBoard->blackCanCastleLong),
               &(icsBoard->movesSinceLastIrreversible),
               &(icsBoard->gameNumber),
	       icsBoard->nameOfWhite, 
	       icsBoard->nameOfBlack, 
               &(icsBoard->status),
	       &(icsBoard->basetime), 
	       &(icsBoard->inctime), 
               &(icsBoard->whiteStrength),
               &(icsBoard->blackStrength),
	       &(icsBoard->whitetime), 
	       &(icsBoard->blacktime), 
	       &(icsBoard->nextMoveNum),
	       icsBoard->lanMove,
               icsBoard->lastTime,
               icsBoard->sanMove,
               &(icsBoard->boardFlipped)) == 23){
      tmp[72]='\0';
      if(strcmp(icsBoard->lanMove,"none")){
        ConvIcsSpecialToComp(icsBoard->onMove=='W'?'B':'W',icsBoard->lanMove);
        ConvIcsSanToComp(icsBoard->sanMove); 
      }
      // temporary fix for HGM's ICS
      if(icsBoard->epFile<-1 || icsBoard->epFile>7){
	  logme(LOG_ERROR,"Invalid ep file. Correcting it.");
	  icsBoard->epFile=1;
      }
      for(f=0;f<=7;f++){
	for(r=0;r<=7;r++){
	  (icsBoard->board)[f][r]=tmp[(7-r)*9+f];
	}
      }
      /*      fprintf(stderr,"icsDrone: %llx\n",hash_key(icsBoard));*/
      return TRUE;
    }else{
      ExitOn(EXIT_HARDQUIT,"Could not parse the board.");
    }
  }
  return FALSE;
}

void PrintBoard(IcsBoard *icsBoard){
  int r,f;
  for(r=0;r<=7;r++){
    for(f=0;f<=7;f++){
      fprintf(stderr,"%c",(icsBoard->board)[f][7-r]);
    }
    fprintf(stderr,"%c",'\n');
  }
}

