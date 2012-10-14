/*
icsdroneng, Copyright (c) 2008-2009, Michel Van den Bergh
All rights reserved

This version contains code from polyglot and links against readline.
Hence it now falls under the GPL2+
*/
/*
icsDrone, Copyright (c) 1996-2001, Henrik Oesterlund Gram
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are 
met:

Redistributions of source code must retain the above copyright 
notice, this list of conditions and the following disclaimer. 

Redistributions in binary form must reproduce the above copyright 
notice, this list of conditions and the following disclaimer in 
the documentation and/or other materials provided with the 
distribution. 

The names of its contributors may not be used to endorse or 
promote products derived from this software without specific 
prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "icsdrone.h"

#define WHITE 0
#define BLACK 1

#define MAX_FORWARDING 200

/* Max line size of PGN file */
#define MAX_PGN_FILE_LINE	80



void InternalIcsCommand(char *command){
    SendMarker(STARTINTERNAL);
    SendToIcs("\n%s\n",command);
    SendMarker(STOPINTERNAL);
}

void CancelTimers(){
  delete_timer(&runData.idleTimeoutTimer);
  delete_timer(&runData.findChallengeTimer);
  delete_timer(&runData.flagTimer);
  delete_timer(&runData.abortTimer);
  delete_timer(&runData.courtesyAdjournTimer);
}

void HandleTellQueue(void *s){
    char *p;
    p=strchr(runData.tellQueue,'\n');
    if(p){
        *p='\0';
        SendToIcs("%s\n",runData.tellQueue);
        memmove(runData.tellQueue,p+1,strlen(p+1)+1);
        create_timer(&(runData.tellTimer),2000,HandleTellQueue,NULL);
    }else{
        logme(LOG_DEBUG,"No tells left. Stopping tell timer.");
        delete_timer(&runData.tellTimer);
        runData.tellQueue[0]='\0'; /* to be on the safe side */
    }
}

/* For sending timeout commands */
void IdleTimeout(void *data)
{
    char *p;
    if(appData.sendTimeout && 
       (!appData.acceptOnly || appData.acceptOnly[0]=='\0')){
        logme(LOG_DEBUG,"Sending timeout commands");
        p=ConvertNewlines(appData.sendTimeout);
        SendToIcs("%s\n",p);
        free(p);
    }
    CancelTimers();
    create_timer(&(runData.idleTimeoutTimer),
                 IDLETIMEOUT,
                 IdleTimeout,NULL);
}

/* For limitRematches code */
void FindChallenge(void *data){
    
    /* Get a list of pending challenges; much better than maintaining it ourselves */
    InternalIcsCommand("\npending\n");

    /* In case there are no pendings left, we'll want to have the timeout handler setup */
    if(!runData.inGame && !runData.inTourney){ /* for safety */
      if (appData.sendTimeout) {
	CancelTimers();
	create_timer(&(runData.idleTimeoutTimer),IDLETIMEOUT,
		     IdleTimeout,
		     NULL);
      }
    }
}

/* For auto-flagging */
void Flag(void *data)
{
    SendToIcs("flag\n");
}

void Abort(void *data){
    SendToIcs("abort\n");
}

/* For courtesyadjourning games after x minutes of waiting */
void CourtesyAdjourn(void *data)
{
    if(runData.registered){
    /* Adjourn */
      SendToIcs("adjourn\n");
    }else{
    /* Unregistered users cannot adjourn */
      SendToIcs("abort\n");
    }
}

void ProcessConsoleLineRed(char *line){
    ProcessConsoleLine(line, NULL);
}

void Prompt(mask){
    if((mask & PROXY) && !runData.inhibitPrompt){
	SendToProxy("%s", runData.lastIcsPrompt);
    }
#ifdef HAVE_LIBREADLINE
  if((mask & CONSOLE) && appData.console && !runData.inhibitPrompt){
    if(runData.inReadline){
      rl_reset_line_state();
      rl_redisplay();
    }else{
      rl_callback_handler_install(runData.prompt,ProcessConsoleLineRed);  
    }
    runData.promptOnLine=TRUE;
    runData.inReadline=TRUE;
    runData.blockConsole=FALSE;
  }
#else
      runData.blockConsole=FALSE;
#endif
}

/*
void AutoJoin(){
  SendToIcs("td gettourney lightning\n");
  SendToIcs("td gettourney blitz\n");
  SendToIcs("td gettourney standard\n");
}
*/

void StartMultiFeedback(mask){
  if(mask & (CONSOLE|PROXY)){
    
    runData.multiFeedbackDepth++;
  }
}

void StopMultiFeedback(mask){
  if(mask & (CONSOLE|PROXY)){
    runData.multiFeedbackDepth--;
    if(runData.multiFeedbackDepth==0){
      Prompt(mask);
    }
  }
}

void SetFeedbackColor(int color){
  runData.color=color;
}

void UnsetFeedbackColor(){
  runData.color=-1;
}

void Feedback(int mask,char *format,...){
  va_list ap;
  char buf[BUF_SIZE];
  int preamble;
  FILE *shortLogFile;
  if(((mask & OWNER) && appData.owner)){
    if((preamble=snprintf(buf,BUF_SIZE,"tell %s ",appData.owner))>=BUF_SIZE){
      logme(LOG_WARNING,"Feedback: feedback buffer too small");
      return;
    }
    va_start(ap,format); 
    if(vsnprintf(buf+preamble,BUF_SIZE-preamble,format,ap)<BUF_SIZE-preamble){
        if(runData.registered){
            SendToIcs("%s\n",buf);
        }else{
            if(strlen(buf)+
               1 /* "\n" */ +
               strlen(runData.tellQueue)+
               1 /* "\0" */
               >= TELLQUEUESIZE){
                logme(LOG_ERROR,"Tell queue overflow");
                SendToIcs("tell %s Tell queue overflow.\n",appData.owner);
            }else{
                strcat(runData.tellQueue,buf);
                strcat(runData.tellQueue,"\n");
            }
            if(!runData.tellTimer){
                logme(LOG_DEBUG,"Tell timer is not running. Starting it.");
                create_timer(&(runData.tellTimer),2000,HandleTellQueue,NULL);
            }
        }
    } else {
      logme(LOG_WARNING,"Feedback: feedback buffer too small");
    }
    va_end(ap);
  }
  if((mask & PROXY)){
       va_start(ap,format); 
       if(vsnprintf(buf,BUF_SIZE,format,ap)<BUF_SIZE){
	   SendToProxy("%s\r\n",buf);
	   if(runData.multiFeedbackDepth==0){
	       Prompt(PROXY);
	   }
       } else {
	   logme(LOG_WARNING,"Feedback: feedback buffer too small");
       }
       va_end(ap);
  }
  if((mask & SHORTLOG) && appData.shortLogging){
    if((preamble=snprintf(buf,BUF_SIZE,"%s: ",runData.timestring))>=BUF_SIZE){
      logme(LOG_WARNING,"Feedback: feedback buffer too small");
      return;
    }
    shortLogFile=fopen(appData.shortLogFile,"a");
    va_start(ap,format); 
    if(vsnprintf(buf+preamble,BUF_SIZE-preamble,format,ap)<BUF_SIZE-preamble){
      fprintf(shortLogFile,"%s\n",buf);
    } else {
      logme(LOG_WARNING,"Feedback: feedback buffer too small");
    }
    va_end(ap);
    fclose(shortLogFile);
  }
  if((mask & CONSOLE) && appData.console){
    if(runData.color!=-1){
      SetColor(runData.color);
    }else{
      SetColor(appData.colorDefault);
    }
    va_start(ap,format); 
    if(runData.promptOnLine){
      printf("\n");
    }
    runData.promptOnLine=0;
    vprintf(format,ap);
    printf("\n");
    va_end(ap);
    ResetColor();
    if(runData.multiFeedbackDepth==0){
      Prompt(CONSOLE);
    }
  }
}

void SendOptions(int all, int mask){
      int d;
      ArgTuple *a=argList;	
      StartMultiFeedback(mask);
      while(a->argName){
         if(!all && !(a->runTime)){
            a++;
            continue;
         }
         if((a->argType)==ArgString){
                    if(!(a->argValue) || !(*( (char **)(a->argValue)))  ){
		      Feedback(mask,"%s=",(a->argName)+1); 
                      a++;
	              continue;
                    }
		    Feedback(mask,"%s=%s",(a->argName)+1,     
                                       *( (char **)(a->argValue))); 
          } else if((a->argType)==ArgBool){
                    d=*((int *)(a->argValue));
                    if(d){
		      Feedback(mask,"%s=%s",(a->argName)+1,"on");
		    }else{
		      Feedback(mask,"%s=%s",(a->argName)+1,"off");
                    }
          } else if ((a->argType)==ArgInt){
                    d=*((int *)(a->argValue));
		       Feedback(mask,"%s=%d",(a->argName)+1,d);
          }
          a++;
      }
      StopMultiFeedback(mask);
}

int SetOption(char* name, int flags, int mask, char* format, ...){
  ArgTuple *a=argList;	
  int found=0;
  char value[8192];
  va_list ap;
  va_start(ap,format);
  vsnprintf(value, sizeof(value), format, ap);
  value[sizeof(value)-1]='\0';
  va_end(ap);
  //  logme(LOG_DEBUG,"SetOption with name=\"%s\" format=\"%s\" value=\"%\"s flags=\"%x\" mask=\"%x\"", name, format, value,flags,mask);
  if(!(flags & CLEAR)){
    Feedback(mask,"Setting option %s to %s.",name,value);
  }else if (flags & CLEAR){
    Feedback(mask,"Clearing option %s.",name);
  }
  while(a->argName){
      if(!strcasecmp((a->argName)+1,name) && (a->runTime || (flags & LOGIN))){
      found=1;
      switch(a->argType){
      case ArgString:
	if(*((char **) a->argValue)){
	  free(*((char **) a->argValue));
	}
    if(!(flags & CLEAR)){
	  *((char **) a->argValue) = strdup(value);
        }else{
          *((char **) a->argValue) = NULL;
        }
	break;
      case ArgInt:
          if(!(flags & CLEAR)){
	  *((int *) a->argValue) = atoi(value);
	}else{
	  *((int *) a->argValue) = 0;
	}
	break;
      case ArgBool:
          if(!(flags & CLEAR)){
	  if (!strcasecmp(value, "true") || 
			    !strcasecmp(value, "on"))
			    *((int *) a->argValue) = TRUE;
	  else if (!strcasecmp(value, "false") || 
				 !strcasecmp(value, "off"))
			    *((int *) a->argValue) = FALSE;
	}else{
	  *((int *) a->argValue) = FALSE;
	}
	break;
      case ArgNull:
	logme(LOG_ERROR,"Internal error");
	break;
      }
      break;
    }
    a++;
  }
  return found;
}

int GetOption(char* name, char * value){
  ArgTuple *a=argList;	
  int found=0;
  value[0]='\0';
  while(a->argName){
      if(!strcasecmp((a->argName)+1,name)){
      found=1;
      switch(a->argType){
      case ArgString:
	  // returning the empty string is not really right...
	  if(*((char **) a->argValue)){
	      strcat(value,*((char **) a->argValue));
	  }
	  break;
      case ArgInt:
	  snprintf(value,99,"%d",*((int *) a->argValue));
	  break;
      case ArgBool:
	  if( *((int *) a->argValue) == TRUE){
	      strcat(value,"true");
	  }else{
	      strcat(value,"false");
	  }
	break;
      case ArgNull:
	logme(LOG_ERROR,"Internal error");
	break;
      }
      break;
    }
    a++;
  }
  return found;
}


void ExecFile(char * filename, int mask, int inhibitSet){
  FILE *f;
  char buf[256];
  char filenameBuf[256];
  strncpy(filenameBuf,filename,256-9-1); /* 9=strlen(".icsdrone") */
  strcat(filenameBuf,".icsdrone");
  f=fopen(filenameBuf,"r");
  if(!f){
    Feedback(mask,"Could not open \"%s\".",filenameBuf);
    return;
  }
  while(myfgets(buf, sizeof(buf), f)) ExecCommand(buf,0,inhibitSet);
  fclose(f);
  Feedback(mask, "Done loading \"%s\".", filenameBuf);
}

void StartForwarding(int mask){
    if(mask){
#ifdef HAVE_LIBREADLINE
	if(mask & CONSOLE){
	    int rows,cols;
	    rl_get_screen_size (&rows, &cols);
	    SendToIcs("set width %d\n",cols);
	    SendToIcs("set height %d\n",rows);
	}
#endif
	runData.forwardingMask=mask;
	SendMarker(STARTFORWARDING);
    }
}
void StopForwarding(int mask){
    if(mask){
	SendMarker(STOPFORWARDING);
	if(mask & CONSOLE){
	    SendToIcs("set width 240\n");
	}
    }
}


void ExecCommand(char * command, int mask, int inhibitSet){
  int i;
  char key[256];
  value_t result[1];
  char value[8192];
  char *strip_command;
  /* delete white space */
  while(*command==' '){
      command++;
  }
  strip_command=strtok(command,"\r\n");
  if(strip_command){
      command=strip_command;
  }
  if (!strcmp(command, "restart")) {
    if (runData.gameID != -1) {
      Feedback(mask,"Ok, I will restart as soon as this game is over.");
      runData.quitPending = TRUE;
      runData.exitValue=EXIT_RESTART;
    } else {
      runData.prompt[0]='\0';
      Feedback(CONSOLE|OWNER|SHORTLOG|PROXY,"%s logged out.",runData.handle);
      ExitOn(EXIT_RESTART,"Restart command executed.");
    }
  } else if (!strcmp(command, "softquit")) {
    if (runData.gameID != -1) {
      Feedback(mask,"Ok, I will quit as soon as this game is over.");
      runData.quitPending = TRUE;
    } else {
      runData.prompt[0]='\0';
      Feedback(CONSOLE|OWNER|SHORTLOG|PROXY,"%s logged out.",runData.handle);
      ExitOn(EXIT_HARDQUIT,"SoftQuit command executed.");
    }
  } else if (!strcmp(command, "hardquit")||!strcmp(command, "quit") ) {
      runData.prompt[0]='\0';
    Feedback(CONSOLE|OWNER|SHORTLOG|PROXY,"%s logged out.",runData.handle);
    ExitOn(EXIT_HARDQUIT,"HardQuit command executed.");
  } else if(!strcmp(command,"options")){
    SendOptions(0,mask);
  } else if(!strcmp(command,"daemonize")){
      runData.prompt[0]='\0';
    Feedback(mask,"Going into background....");
    Daemonize();
  } else if(!strcmp(command,"coptions")){
    SendOptions(1,mask);
  } else if(sscanf(command,"load %8191[^\n\r]",value)==1){
      ExecFile(value,mask,inhibitSet); 
  } else if(sscanf(command,"exec %8191[^\n\r]",value)==1) {
      eval(result,"%s",value);
      // temporary
      if(0){
      }else if(result->type==V_STRING){
	  Feedback(mask,"\"%s\"",result->string_value->data);
      }else if(result->type==V_BOOLEAN){
	  Feedback(mask,"%s",result->value?"true":"false");
      }else if(result->type==V_CFUNC){
	  Feedback(mask,"<C function: %p>",result->cfunc_value);
      }else if(result->type==V_NONE){
	  Feedback(mask,"none");
      }else if(result->type==V_ERROR){
	  Feedback(mask,"<error:%d>",result->value);
      }else{
	  Feedback(mask,"%d",result->value);
      }
  }else if(((i=sscanf(command,"set %254[^=\n\r ] %8191[^\n\r]", key,value))==1)  || (i==2)){
      if(mask & (CONSOLE|PROXY)){
      runData.inhibitPrompt=TRUE;
    }
      if(!inhibitSet){
	  if(i==1){
	      if(!SetOption(key,CLEAR,mask,"%s",value)){
		  StartForwarding(mask);
		  SendToIcs("set %s\n",key);
		  StopForwarding(mask);
	      }else{
		  runData.inhibitPrompt=FALSE;
		  Prompt(mask);
	      }
	  }else{
	      if(!SetOption(key,0,mask,"%s",value)){
		  StartForwarding(mask);
		  SendToIcs("set %s %s\n",key,value);
		  StopForwarding(mask);
	      }else{
		  runData.inhibitPrompt=FALSE;
		  Prompt(mask);
	      }
	  }
      }else{
	  logme(LOG_DEBUG,"Set command \"%s\" inhibited\n",command);
      }
  }else if(!strncmp(command,"help",4) && strstr(command,PACKAGE_NAME)){ /*temporary */
    StartMultiFeedback(mask);
    Feedback(mask,"Commands : help set softquit hardquit restart options coptions");
    Feedback(mask,"         : daemonize load");
    Feedback(mask,"Options  : searchdepth easymode secpermove sendgameend limitrematches");
    Feedback(mask,"           issuerematch sendgamestart acceptonly feedback feedbackonlyfinal");
    Feedback(mask,"           feedbackcommand proxyfeedback pgnlogging scoreforwhite ");
    Feedback(mask,"           sendtimeout shortlogfile shortlogging sendlogin hardlimit");
    Feedback(mask,"           noplay autojoin autoreconnect resign acceptdraw scoreforwhite ");
    Feedback(mask,"           ownerquiet enginequiet colortell coloralert colordefault");
    Feedback(mask,"Read the manual page or README file for more information.");
    StopMultiFeedback(mask);
  }else if(!strncmp(command,"help",4)){
    StartForwarding(mask);
    if(mask & (CONSOLE|PROXY)){
      runData.inhibitPrompt=TRUE;
    }
    Feedback(mask,"===> This is ICS help. Use \"help icsdroneng\" for general icsdroneng help.");
    SendToIcs("%s\n",command);
    StopForwarding(mask);
  }else 

 if (command[0] == '*') {
    SendToComputer("%s\n", command+1);
    Feedback(mask,"Forwarded \"%s\" to the engine.",command+1);
  } else {
    StartForwarding(mask);
    SendToIcs("%s\n", command);
    StopForwarding(mask);
  }
}

void ExecCommandList(char * command, int mask, int inhibitSet){
    char *command_c=strdup(command);
    char *command_cc=command_c;
    char *p;
    while(1){
        p=strstr(command_c,"\\n");
        if(p){
            p[0]='\0';
            ExecCommand(command_c, mask,inhibitSet);
            command_c=p+2;
        }else{
            ExecCommand(command_c, mask,inhibitSet);
            break;
        }
    }
    free(command_cc);
}

Bool InList(char *list, char *item){
        /*
         * Hacky code. Wait for proper string handling.
         */
    char *p;
    Bool match=TRUE;
    char *delimiters=" ,";
    if((p=strstr(list,item))){
        if((p>list) && (!strchr(delimiters,*(p-1)))){
            match=FALSE;
        }else if((p+strlen(item)<list+strlen(list))&&
                 (!strchr(delimiters,*(p+strlen(item))))){
            match=FALSE;
        }
    }else{
        match=FALSE;
    }
    return match;
}

Bool IsNoPlay(char *s){
    if(appData.noplay){
        return InList(appData.noplay,s);
    }else{
        return FALSE;
    }
}

void GetTourney(){
  if(runData.parsingListTourneys){
    logme(LOG_DEBUG,"We have already a ListTourneys command running.");
    return;
  }
  if(runData.registered &&  appData.autoJoin &&
     (!appData.acceptOnly || appData.acceptOnly[0]=='\0')){
    runData.parsingListTourneys=TRUE;
    SendToIcs("td ListTourneys -j\n");
  }
}

Bool EngineToMove(IcsBoard * icsBoard){
  return (icsBoard->status)==1;
}


void HandleBoard(IcsBoard * icsBoard, char *moveList, Bool ignoreMove){
  move_t move;
  book_move_t bmove;
  int whitetime,blacktime;
  int flagtime;
  if (runData.sigint && (icsBoard->status != -1)) InterruptComputer();
  if(!EngineToMove(icsBoard) && (icsBoard->nextMoveNum==1)){
    logme(LOG_INFO,"Setting up automatic abort in 60 seconds.");
    CancelTimers();
    if(!runData.inTourney){
      create_timer(&(runData.abortTimer),ABORTTIMEOUT,Abort,NULL);
    }else{
      logme(LOG_DEBUG,"Not setting up abort timer as we are in a tourney.");
    }
  }
  if(!EngineToMove(icsBoard) && (icsBoard->nextMoveNum>1)){
    flagtime = runData.computerIsWhite?icsBoard->blacktime:icsBoard->whitetime;
    if (flagtime <= 0) {
      logme(LOG_INFO,"Calling flag");
      CancelTimers();
      Flag(NULL);
    } else {
      logme(LOG_INFO,"Setting up flag in %d seconds.", flagtime+1);
      CancelTimers();
      create_timer(&(runData.flagTimer),(flagtime+1)*1000,Flag,NULL);
    }
  }
  if(moveList){
    Force();
    SendMovesToComputer(moveList);
  }
  if(EngineToMove(icsBoard)){
    /* clear outstanding flag and abort alarms */
    CancelTimers();
    whitetime=100*icsBoard->whitetime;
    blacktime=100*icsBoard->blacktime;
    SendTimeToComputer(whitetime, blacktime);
    book_move(icsBoard,&bmove,TRUE);
    if(strcmp(bmove.move,"none")){
      Force();
    }
    if (runData.longAlgMoves) {
      strcpy(move, icsBoard->lanMove);
    } else {
      strcpy(move, icsBoard->sanMove);
    }
    if(strcmp(move,"none") && !moveList && !ignoreMove){
      logme(LOG_INFO, "Move from ICS: %s %s", 
                     icsBoard->lanMove,icsBoard->sanMove);
      SendMoveToComputer(move);
    }
    runData.waitingForFirstBoard=FALSE;
    if(strcmp(bmove.move,"none")){
      logme(LOG_INFO,"Bookmove: %s score=%d\n",bmove.move,bmove.score);
      if(appData.feedback && appData.feedbackCommand){
	  SendToIcs("%s Bookmove: %s score=%d\n",appData.feedbackCommand,bmove.move,bmove.score); 
      }
      if(appData.proxyFeedback){
	  Feedback(PROXY,"--> icsdrone: Bookmove: %s score=%d",bmove.move,bmove.score); 
      }
      SendToIcs("%s\n",bmove.move);
      SendMoveToComputer(bmove.move);
    }else{
      Go();
    }
  }
}

void SetupEngineOptions(IcsBoard *icsBoard){
  /* 
   *  Setup chessprogram with the right options.
   *  I think I finally got this right
   */
  EnsureComputerReady();
  if (!appData.secPerMove && !appData.searchDepth) {
    Level(icsBoard->basetime,icsBoard->inctime);
  } 
  SecondsPerMove(appData.secPerMove);
  Depth(appData.searchDepth);
  Easy(appData.easyMode);
  Random();
}

char * KillPrompts(char *line){
  /*  Get rid of those damn prompts  -- might be more on the same line even */
  /*  Eat both "fics%" and "aics%". */
  logcomm("ics","icsdrone",line);
  while (isalpha(line[0])&&!strncmp(line+1, "ics% ", 5)){
    if(runData.lastIcsPrompt[0]=='\0'){
	  strncpy(runData.lastIcsPrompt,line,6);
	  logme(LOG_DEBUG,"Saving prompt \"%s\"",runData.lastIcsPrompt);
    }
    line += 6;
  }
  return line;
}

Bool ProcessInternalMarkers(char *line){
    if(IsMarker(STARTINTERNAL,line)){
	runData.internalIcsCommand++;
	return TRUE;
    }else if(IsMarker(STOPINTERNAL,line)){
	runData.internalIcsCommand--;
	return TRUE;
    }
    return FALSE;
}

Bool ProcessPings(char *line){
  if(IsMarker(PING,line)){
    runData.pingCounter--;
    return TRUE;
  }
  return FALSE;
}

Bool ProcessForwardingMarkers(char *line){
  static int forwardingDepth=0;
  if(IsMarker(STARTFORWARDING,line)){
    logme(LOG_DEBUG,"Starting forwarding.");
    runData.forwarding=1;
    StartMultiFeedback(runData.forwardingMask);
    forwardingDepth++;
    return TRUE;
  }
  if(IsMarker(STOPFORWARDING,line)){
    forwardingDepth--;
    if(forwardingDepth==0){
      logme(LOG_DEBUG,"Stopping forwarding.");
      runData.forwarding=0;
      runData.inhibitPrompt=FALSE;    /* not nice hack */
      StopMultiFeedback(runData.forwardingMask);
    }
    return TRUE;
  }
  return FALSE;
}

Bool IsShuffle(char *chessvariant){
    if(!strcmp(chessvariant,"normal")){
	return FALSE;
    }
    if(!strcmp(chessvariant,"suicide")){
	return FALSE;
    }
    if(!strcmp(chessvariant,"losers")){
	return FALSE;
    }
    if(!strcmp(chessvariant,"atomic")){
	return FALSE;
    }
    if(!strcmp(chessvariant,"crazyhouse")){
	return FALSE;
    }
    return TRUE;
}

Bool IsFicsShuffle(char *icsvariant){
    if(!strncmp(icsvariant,"wild",4)){
	return TRUE;
    }
    if(!strncmp(icsvariant,"odds",4)){
	return TRUE;
    }
    if(!strncmp(icsvariant,"eco",3)){
	return TRUE;
    }
    if(!strncmp(icsvariant,"nic",3)){
	return TRUE;
    }
    if(!strncmp(icsvariant,"uwild",5)){
	return TRUE;
    }    
    if(!strncmp(icsvariant,"misc",4)){
	return TRUE;
    }
    if(!strncmp(icsvariant,"pawns",5)){
	return TRUE;
    }
    return FALSE;
}


Bool UseMoveList(){
    int ret;
    ret=TRUE;
    if (runData.longAlgMoves && runData.icsType!=ICS_FICS) {
	/* only FICS has the (undocumented) command "moves l" */
	logme(LOG_WARNING,"Server doesn't support long algebraic move lists.\n\
Do not ask for movelist.\n");
	ret=FALSE;
    }else if(IsFicsShuffle(runData.icsVariant)){
	/* Getting the initial position in a shuffle game is tricky */
	/* For now ignore this problem */
	logme(LOG_DEBUG,"Do not ask for movelist since this is a shuffle game.\n");
	ret=FALSE;
    }
    return ret;
}

char * CheckChessVariantSupport(char * variant){
    Bool found=FALSE;
    char *variant1;
    char *chessVariant;
    int j;
    logme(LOG_DEBUG,"Testing variant \"%s\"\n",variant);
    if(!strcmp(variant,"nocastle")){
	variant="normal";
    }
    logme(LOG_DEBUG,"normalized as \"%s\"\n",variant);    
    for(j=0;j<runData.chessVariantCount;j++){
	chessVariant=runData.chessVariants[j];
	logme(LOG_DEBUG,"against chess variant \"%s\"\n",chessVariant);
	if(!strncmp(chessVariant,"8x8",3) && 
	   (variant1=strchr(chessVariant,'_'))){
	    chessVariant=variant1+1;
	}
	logme(LOG_DEBUG,"normalized as \"%s\"\n",chessVariant);
	if(!strcmp(variant,chessVariant)){
	    found=TRUE;
	    break;
	}
    }
    if(found){
	return runData.chessVariants[j];
    }else{
	return NULL;
    }
}

Bool IsVariant(char *icsvariant, char* category){
    int i=0;
    char ich,cch;
    while(TRUE){
	ich=icsvariant[i];
	cch=category[i];
	if(ich=='\0' || cch=='\0'){
	    return TRUE;
	}
	if(cch=='\0' && ich=='/'){
	    return TRUE;
	}
	if(cch!=ich){
	    return FALSE;
	}
	i++;
	
    }
}

void ParseVariantList(char *variants){
    char *saveptr1;
    char *saveptr2;
    char *pair;
    char *icsvariant;
    char *chessvariant;
    char *list;
    int vix;
    vix=0;

    if(!variants){
	runData.icsVariantCount=0;
	goto finish;
    }
    list=strdup(variants);
    pair=strtok_r(list," ,",&saveptr1);
    while(pair){
	if(vix>=MAXVARIANTS){
	    logme(LOG_DEBUG,"Maximal icsvariant count %d reached.",MAXVARIANTS);
	    return;
	}
	icsvariant=strtok_r(pair," =", &saveptr2);
	chessvariant=strtok_r(NULL," ", &saveptr2);
	if(!chessvariant){
	    chessvariant="normal";
	}
	logme(LOG_DEBUG,"ics=[%s], chess=[%s]\n",icsvariant,chessvariant);
	strncpy(runData.icsVariants[vix][0],icsvariant,30);
	runData.icsVariants[vix][0][30]='\0';
	strncpy(runData.icsVariants[vix][1],chessvariant,30);
	runData.icsVariants[vix++][1][30]='\0';
	pair=strtok_r(NULL," ,",&saveptr1);
    }
    free(list);
 finish:
    runData.icsVariantCount=vix;
    logme(LOG_DEBUG,"variant count=%d\n",runData.icsVariantCount);
}



Bool ProcessLogin(char *line){
  /*
   *  Detect our nickname - this is needed when 1) it was given in the 
   *  wrong case and 2) we login as guest where we are assigned a random
   *  guest nickname. 
   */
  char name[30+1];
  char dummy;
  value_t value[1];
  char *command;
  if(runData.loggedIn) return FALSE;
  if (strstr(line,"Invalid password")){
    ExitOn(EXIT_HARDQUIT,"Invalid password!");
  }
  if (strstr(line,"No such username")){
    ExitOn(EXIT_HARDQUIT,"Invalid username!");
  }
  if (strstr(line,"Sorry, names may be at most 17 characters long.")){
    ExitOn(EXIT_HARDQUIT,"Names can be at most 17 characters!");
  }
  if(!strncmp(line,"login:",6) && !runData.loggedIn){
    runData.loginCount++;
    if(runData.loginCount>1){
      ExitOn(EXIT_HARDQUIT,"Login failed.");
      return TRUE;
    }
  }
  if(strstr(line,"freechess.org")){
      runData.icsType=ICS_FICS;
      logme(LOG_DEBUG,"We are playing on FICS.");
  }
  if(strstr(line,"chessclub.com")){
      runData.icsType=ICS_ICC;
      logme(LOG_DEBUG,"We are playing on ICC.");
  }
  if(strstr(line,"Variant-ICS")){
      runData.icsType=ICS_VARIANT;
      logme(LOG_DEBUG,"We are playing on H.G. Muller's variant ICS.");
  }
  if (!runData.loggedIn &&
      (sscanf(line, "Statistics for %30[^( ]", name) == 1 ||
       (sscanf(line, "Finger of %30[^:( ]",name)==1)   )&&
      !strncasecmp(runData.handle, name, strlen(runData.handle))) {
      
      if (strcmp(runData.handle, name)) {
	  logme(LOG_WARNING, "Nickname was corrected to: %s", name);
	  strncpy(runData.handle,name,sizeof(runData.handle));
	  runData.handle[sizeof(runData.handle)-1]='\0';
	  snprintf(runData.prompt,30,"%s> ",runData.handle);
      }
      return TRUE;
  }
  if(!runData.loggedIn && IsMarker(ENDLOGIN,line)){
    logme(LOG_INFO,"It seems we are succesfully logged in!");
    if(appData.variants==NULL){
	char *variants;
	switch(runData.icsType){
	case ICS_FICS:
	    variants="lightning,blitz,standard,wild/0=wildcastle,wild/1=wildcastle,wild/fr=fischerandom,wild=nocastle,suicide=suicide,losers=losers,atomic=atomic,crazyhouse=crazyhouse,odds,eco,nic,uwild=nocastle,pawns,misc";
	    break;
	case ICS_ICC:
	    /* This is currently not tested! */
	    variants="Bullet,Blitz,Standard,wild/1=wildcastle,wild/2,wild/3,wild/4,wild/5,wild/7,wild/8,wild/9=twokings,wild/10,wild/11,wild/12,wild/13,wild/14,wild/15,wild/16=kriegspiel,wild/17,wild/18,wild/19,wild/22=fischerandom,wild/23=crazyhouse,wild/25=3check,wild/26=giveaway,wild/27=atomic,wild/28=shatranj";
	    SetOption("variants",LOGIN,0,variants);
	    logme(LOG_DEBUG,"Enabling variants: %s",variants);
	    break;
	case ICS_VARIANT:
	    variants="lightning,blitz,standard,wild/1=wildcastle,wild=nocastle";
	    break;
	default:
	    variants="lightning,blitz,standard,wild/1=wildcastle,wild=nocastle";
	    break;
	}
	SetOption("variants",LOGIN,0,variants);
    }
    logme(LOG_DEBUG,"Enabling variants: %s",appData.variants);
    ParseVariantList(appData.variants); 
    StartComputer();
    if(appData.proxy && StartProxy()){
	logme(LOG_INFO,"Proxy started.");
    }
    runData.loggedIn=1;
    persistentData.wasLoggedIn=TRUE;

    if(!appData.pgnFile){
      SetOption("pgnFile",LOGIN,0,"%s.pgn",runData.handle);
    }
    if(!appData.shortLogFile){
      SetOption("shortLogFile",LOGIN,0,"%s.txt",runData.handle);
    }
    if(appData.pgnLogging){
      printf("PGN logging to \"%s\".\n",appData.pgnFile);    
    }
    if(appData.shortLogging){
      printf("Short logging to \"%s\".\n",appData.shortLogFile);    
    }
    Feedback(CONSOLE|OWNER|SHORTLOG|PROXY,"%s logged in.",runData.handle);
    CancelTimers();
    if (appData.sendTimeout) {
      create_timer(&(runData.idleTimeoutTimer),
		   IDLETIMEOUT,
		   IdleTimeout,
		   NULL);
    }
    GetTourney();
    if(appData.sendLogin){
        ExecCommandList(appData.sendLogin,0,FALSE);
    }
    SendToIcs("resume\n");
    // injecting in interpreter
    eval_set("sys.handle",V_STRING,SY_RO,runData.handle);
    eval_set("sys.registered",V_BOOLEAN,SY_RO,runData.registered);
    switch(runData.icsType){
    case ICS_FICS:
	eval_set("sys.ics_type",V_STRING,SY_RO,"fics");
	break;
    case ICS_ICC:
	eval_set("sys.ics_type",V_STRING,SY_RO,"ics");
	break;
    case ICS_VARIANT:
	eval_set("sys.ics_type",V_STRING,SY_RO,"variant");
	break;
    default:
	eval_set("sys.ics_type",V_STRING,SY_RO,"generic");
	break;

    }
    command="log(\"sys.handle=\"+str(sys.handle)+\" sys.registered=\"+str(sys.registered)+\" sys.ics_type=\"+str(sys.ics_type))";
    eval(value,command);
    return TRUE;
  }
  if(!runData.loggedIn && (
	sscanf(line,"%*30s is not a registered name%c",&dummy)==1 || 
        sscanf(line,"Logging you in as \"Guest%c",&dummy)==1 ||
        sscanf(line,"%*30s is NOT a registered player%c",&dummy)==1)
     ){
    logme(LOG_INFO,"We are not registered.");
    runData.registered=FALSE;
    return TRUE;
  }
  return FALSE;
}

Bool ProcessNext(char *line){
  if(!runData.loggedIn) return FALSE;
  /* This gives problems with feedback now
  if(strstr(line,"Type [next] to see next page.")){
    SendToIcs("next \n");
    return TRUE;
  }
  */
  if(strstr(line,":Use \"td next\" to see the next page.")){
    SendToIcs("td next \n");
    return TRUE;
  }
  return FALSE;
}

Bool ProcessOwnerNotify(char *line){
    char dummy;
    char name[30+1];
    if(!runData.loggedIn) return FALSE;
    if(!appData.owner) return FALSE;
    if(sscanf(line,"Notification: %s has arrived%c",name,&dummy)==2 &&
       !strcasecmp(name,appData.owner)){
      Feedback(OWNER|CONSOLE|PROXY,"%s welcomes his owner %s!",runData.handle,appData.owner);
        return TRUE;
    }
    return FALSE;
}

Bool ProcessPending(char *line){
  char name[30+1];
  char dummy;
  int offer;
  static int backupoffer;
  static int parsingPending;
  if(!runData.loggedIn) return FALSE;
  /*
   *  Pending list; find the first offer made not by the last player
   *  (limitRematches code)
   */
  if (appData.limitRematches &&
      !strncmp(line, "Offers from other players", 25)) {
    backupoffer = -1;
    parsingPending=TRUE;
    logme(LOG_DEBUG, "Detected start of pending offers list");
    return TRUE;
  }
  if(parsingPending){
      if (sscanf(line, " %d: %30s is offering a challeng%c", 
                 &offer, name, &dummy) == 3) {
          if(!IsNoPlay(name)){
              if (strcmp(name, runData.lastPlayer)) { // offer from other player
                  SendToIcs("accept %d\n", offer);
              } else { // offer from last player
                  backupoffer = offer;
              }
          }else{
              logme(LOG_DEBUG,"Not retaining offer from %s. He is on our noplay list.",name);
              SendToIcs("decline %d\n",offer);
              
          }
          return TRUE;
      }
      if (!strncmp(line, "If you wish to accept any of these offers type", 46)) {
          if (backupoffer != -1){
              SendToIcs("accept %d\n", backupoffer);
          }
          logme(LOG_DEBUG, "Detected end of pending offers list.");
          parsingPending=FALSE;
          return TRUE;
      }
  }
  return FALSE;
}

Bool ProcessOffers(char *line){
  char name[30+1];
  char dummy;
  int mask;
  if(!runData.loggedIn) return FALSE;
  /*
   *  Respond to offers
   */
  /* 
   * Offers clutter the shortlog. 
   * Besides iscsdrone now handles draw offers internally.
   */
  mask=CONSOLE;
  if(!appData.ownerQuiet){
      mask|=OWNER;
  }
  if ((sscanf(line, "%30s offers you a draw%c", name, &dummy) == 2)  // FICS
      ||
      (sscanf(line, "Your opponent offers you a draw%c", &dummy) == 1) //ICC
  ) {
      Feedback(mask,"Your opponent offers a draw.");
      
    if(!appData.acceptDraw){
      if(runData.engineDrawFeature){
        SendToComputer("draw\n");
      }else{
        SendToIcs("say Sorry, this computer does not handle draw offers.\n");
      }
    }else{
      logme(LOG_DEBUG,"Saving draw offer for later examination.");
      runData.registeredDrawOffer=TRUE;
    }
    return TRUE;
  } 
  if (sscanf(line, "%30s would like to abor%c", name, &dummy) == 2) {
    Feedback(mask,"%s would like to abort.",name);
        /*
         * We will add an options here to convert abort offers into
         * adjourn offers so that the game can be adjudicated.
         * For now we just ignore the offer. 
         */
    return TRUE;
  }
      /*
       * Add an option here to accept adjourn offers automatically.
       * The new FICS adjucation system makes adjudication very efficient.
       * So stored games are no longer a problem. 
       */
  return FALSE;
}

Bool ProcessAutoMatches(char *line){
  char name[30+1];
  if(!runData.loggedIn) return FALSE;
  /* Avoid Fics' automatic matching of seeks for non-computer accounts.
   *  This is important if we are logged in as unregistered. We do not
   *  want to annoy people. 
   *  Note that this only works if we issue manual seeks.
   */
  if(sscanf(line, "Your seek matches one posted by%30s", name) == 1){
    name[strlen(name)-1]='\0'; /* kill trailing dot */
    SendToIcs("decline %s\n",name);
    return TRUE;
  }	  
  if(sscanf(line, "Your seek matches one already posted by%30s", name) == 1){
    name[strlen(name)-1]='\0'; /* kill trailing dot */
    SendToIcs("decline %s\n",name);
    return TRUE;
  }	  
  if(sscanf(line, "Your seek intercepts %30s getgame.", name) == 1){
    name[strlen(name)-2]='\0'; /* kill trailing "'s" */
    SendToIcs("decline %s\n",name);
    return TRUE;
  }
  if(sscanf(line, "Your seek qualifies for %30s getgame.", name) == 1){
    name[strlen(name)-2]='\0'; /* kill trailing "'s" */
    SendToIcs("decline %s\n",name);
    return TRUE;
  }
  return FALSE;
}


/* forward declaration to be able to process tells from mamer */
Bool ProcessTourneyNotifications(char *line);

Bool ProcessTells(char *line){
  static char name[30+1];
  static char tmp[8192+1];
  
  if(!runData.loggedIn) return FALSE;
  /*
   *  Respond to player tells
   */
  if (!runData.processingTells &&((sscanf(line, "%30s tells you: %8192[^\n\r]s", name, tmp) == 2 || 
	sscanf(line, "%30s says: %8192[^\n\r]s", name, tmp) == 2)) ) {
    /*  Cut off possible () and [] from the name */
    if (strchr(name, '(')) {
      name[strchr(name, '(') - name] = '\0';
    } else if (strchr(name, '[')) {
      name[strchr(name, '[') - name] = '\0';
    }
    runData.processingTells=TRUE;
    /* To make sure something comes in when the tell is complete. */
    SendMarker(ENDTELL);
    return TRUE;
  }
  if(!runData.processingTells){
    return FALSE;
  }
  if(line[0]=='\\'){
    strtok(line,"\r\n");
    strncat(tmp,line+4,8192-strlen(tmp));
    return TRUE;
  }
  if(line[0]!='\\'){
    if (appData.owner && !strcmp(appData.owner, name)){ 
	ExecCommand(tmp,1,FALSE);
    } else if (strstr(name,"mamer")){
      /* send away for processing */
      if(strlen(tmp)<8192){
	memmove(tmp+1,tmp,strlen(tmp)+1);
	tmp[0]=':';
	logme(LOG_DEBUG,"Processing mamer tell \"%s\"",tmp);
	ProcessTourneyNotifications(tmp);
      }
    } else{
      if(appData.console){
#ifdef HAVE_LIBREADLINE
	rl_ding();
#endif
      }
      SetFeedbackColor(appData.colorTell);
      Feedback(CONSOLE|OWNER|SHORTLOG,"%s: %s",name,tmp);
      UnsetFeedbackColor();
      if (strcmp(runData.last_talked_to, name)) {
	/*  SendToIcs("tell %s (auto-response) Hi, I'm an automated computer.  I forwarded your comment to my owner.\n", name); */
      }
      strcpy(runData.last_talked_to, name);
    }
    runData.processingTells=FALSE;
    tmp[0]='\0';
    return FALSE; /* the line we are processing is NOT part of the tell! */
  }
  return FALSE;  
}

Bool ProcessNotifications(char *line){
  char name[30+1];
  char dummy;
  if(!runData.loggedIn) return FALSE;
  /*
   *  Remind a player of an unfinished game...
   */
  if (sscanf(line, "Notification: %30s who has an adjourned game%c", 
	     name, &dummy) == 2) {
    name[strlen(name)-1] = '\0';
    SendToIcs("match %s\ntell %s Hi %s.  Let's finish our adjourned game, please.\n", name, name, name);
    return TRUE;
  }
  return FALSE;
}

void InjectChallenge(char *name, 
		     char *rating, 
		     int has_color, 
		     char *color, 
		     char *name2, 
		     char *rating2, 
		     char *rated, 
		     char *variant,
		     char *time_, 
		     char *inc,
		     int win,
		     int draw,
		     int loss,
		     int computer){
    char *command;
    value_t value[1];
    // Inject in interpreter
    eval_set("co.name",V_STRING,SY_RO,name);
    eval_set("co.rating",V_NUMERIC,SY_RO,atoi(rating));
    if(!strncmp(rating,"----",4)){
	eval_set("co.registered",V_BOOLEAN,SY_RO,0);
    }else{
	eval_set("co.registered",V_BOOLEAN,SY_RO,1);
    }
    eval_set("co.myrating",V_NUMERIC,SY_RO,atoi(rating2));
    if(atoi(rating)!=0 && atoi(rating2)!=0){
	eval_set("co.ratingdiff",V_NUMERIC,SY_RO,atoi(rating)-atoi(rating2));
    }else{
	eval_set("co.ratingdiff",V_NUMERIC,SY_RO,0);
    }
    if(!strcmp(rated,"rated")){
	eval_set("co.rated",V_BOOLEAN,SY_RO,1);
	eval_set("co.unrated",V_BOOLEAN,SY_RO,0);
    }else{
	eval_set("co.rated",V_BOOLEAN,SY_RO,0);
	eval_set("co.unrated",V_BOOLEAN,SY_RO,1);
    }
    eval_set("co.variant",V_STRING,SY_RO,variant);
    
    eval_set("co.white",V_BOOLEAN,SY_RO,0);
    eval_set("co.black",V_BOOLEAN,SY_RO,0);
    eval_set("co.nocolor",V_BOOLEAN,SY_RO,0);
    if(has_color){
	eval_set("co.color",V_STRING,SY_RO,color);
	if(!strcasecmp(color,"white")){
	    eval_set("co.white",V_BOOLEAN,SY_RO,1);
	}else{
	    eval_set("co.black",V_BOOLEAN,SY_RO,1);
	}
    }else{
	eval_set("co.color",V_NONE,SY_RO);
	eval_set("co.nocolor",V_BOOLEAN,SY_RO,1);
    }
    eval_set("co.time",V_NUMERIC,SY_RO,atoi(time_));
    eval_set("co.inc",V_NUMERIC,SY_RO,atoi(inc));
    eval_set("co.etime",V_NUMERIC,SY_RO,atoi(time_)+40*atoi(inc));

    eval_set("co.assesswin",V_NUMERIC,SY_RO,win);
    eval_set("co.assessdraw",V_NUMERIC,SY_RO,draw);
    eval_set("co.assessloss",V_NUMERIC,SY_RO,loss);
    eval_set("co.computer",V_BOOLEAN,SY_RO,computer);

    eval_set("co.lightning",V_BOOLEAN,SY_RO,0);
    eval_set("co.blitz",V_BOOLEAN,SY_RO,0);
    eval_set("co.standard",V_BOOLEAN,SY_RO,0);
    eval_set("co.chess",V_BOOLEAN,SY_RO,0);
    eval_set("co.atomic",V_BOOLEAN,SY_RO,0);
    eval_set("co.suicide",V_BOOLEAN,SY_RO,0);
    eval_set("co.losers",V_BOOLEAN,SY_RO,0);
    eval_set("co.crazyhouse",V_BOOLEAN,SY_RO,0);
    eval_set("co.wild",V_BOOLEAN,SY_RO,0);

    if(FALSE){
    }else if(!strcasecmp(variant,"losers")){
	eval_set("co.losers",V_BOOLEAN,SY_RO,1);
    }else if(!strcasecmp(variant,"suicide")){
	eval_set("co.suicide",V_BOOLEAN,SY_RO,1);
    }else if(!strcasecmp(variant,"atom")){
	eval_set("co.atomic",V_BOOLEAN,SY_RO,1);
    }else if(!strcasecmp(variant,"crazyhouse")){
	eval_set("co.crazyhouse",V_BOOLEAN,SY_RO,1);
    }else if(!strncmp(variant,"wild",4)){
	eval_set("co.wild",V_BOOLEAN,SY_RO,1);
    }else if(!strcasecmp(variant,"lightning")){
	eval_set("co.lightning",V_BOOLEAN,SY_RO,1);
	eval_set("co.chess",V_BOOLEAN,SY_RO,1);
    }else if(!strcasecmp(variant,"blitz")){
	eval_set("co.blitz",V_BOOLEAN,SY_RO,1);
	eval_set("co.chess",V_BOOLEAN,SY_RO,1);
    }else if(!strcasecmp(variant,"standard")){
	eval_set("co.standard",V_BOOLEAN,SY_RO,1);
	eval_set("co.chess",V_BOOLEAN,SY_RO,1);
    }

    command="log(\"co.name=\"+str(co.name)+\" co.rating=\"+str(co.rating)+\" co.myrating=\"+str(co.myrating)+\" co.ratingdiff=\"+str(co.ratingdiff)+\" co.registered=\"+str(co.registered)+\" co.rated=\"+str(co.rated)+\" co.unrated=\"+str(co.unrated)+\" co.variant=\"+str(co.variant)+\" co.color=\"+str(co.color)+\" co.nocolor=\"+str(co.nocolor)+\" co.time=\"+str(co.time)+\" co.inc=\"+str(co.inc)+\" co.etime=\"+str(co.etime)+\" co.white=\"+str(co.white)+\" co.black=\"+str(co.black))";
    eval(value,command);
   command="log(\"co.lightning=\"+str(co.lightning)+\" co.blitz=\"+str(co.blitz)+\" co.standard=\"+str(co.standard)+\" co.chess=\"+str(co.chess)+\" co.atomic=\"+str(co.atomic)+\" co.suicide=\"+str(co.suicide)+\" co.losers=\"+str(co.losers)+\" co.crazyhouse=\"+str(co.crazyhouse)+\" co.wild=\"+str(co.wild))";
    eval(value,command);
    command="log(\"co.assesswin=\"+str(co.assesswin)+\" co.assessdraw=\"+str(co.assessdraw)+\" co.assessloss=\"+str(co.assessloss)+\" co.computer=\"+str(co.computer))";
    eval(value,command);
}

Bool ProcessIncomingMatches(char *line){
  static char name[30+1];
  static char name2[30+1];
  static char rating[30+1];
  static char rating2[30+1];
  static char rated[30+1];
  static char variant[30+1];
  static char color[30+1];
  static char time_[30+1];
  static char inc[30+1];
  static int has_color=FALSE;
  static int win;
  static int draw;
  static int loss;
  static int computer;
  static int parsingIncoming=FALSE; 
  char *line1;
  int ret;
  value_t value[1];

  if(!runData.loggedIn) return FALSE;

  if(!parsingIncoming){
      has_color=FALSE;
      win=0;
      draw=0;
      loss=0;
      computer=FALSE;
  }
  /*
   *  Accept incoming matches
   */

  if ((sscanf(line, "Challenge: %30s (%30[^)])%30s (%30[^)])%30s %30s %30s %30s", 
	      name,rating,name2,rating2,rated,variant,time_,inc) == 8)
      ||
      (has_color=(sscanf(line,"Challenge: %30s (%30[^)]) [%30[^]]] %30s (%30[^)])%30s%30s %30s %30s",
			 name,rating,color,name2,rating2,rated,variant,time_,inc)==9))){
      int i;
      parsingIncoming=TRUE;
      /* More specific variant */
      char *line1;
      int found;
      if((line1=strstr(line,"Loaded"))){
	  sscanf(line1,"Loaded from %30[^. ]",variant);
      }

      found=FALSE;
      for(i=0;i<runData.icsVariantCount;i++){
	  if(IsVariant(variant,runData.icsVariants[i][0])){
	      logme(LOG_DEBUG,"ICS variant=\"%s\" Engine variant=\"%s\"\n",
		    runData.icsVariants[i][0],
		    runData.icsVariants[i][1]);
	      if(!CheckChessVariantSupport(runData.icsVariants[i][1])){
		  logme(LOG_DEBUG,"Variant rejected by engine");
	      }else{
		  found=TRUE;
	      }
	      break;
	  }
      }
      if(!found){
	  SendToIcs("tell %s Sorry I do not play variant \"%s\".\n",name,variant);
	  SendToIcs("decline %s\n",name);
	  parsingIncoming=FALSE;
	  return TRUE;
      }

      if(!runData.inTourney){
	  logme(LOG_DEBUG,"Executing matchFilter command: \"%s\"",appData.matchFilter);
	  ret=eval(value,"%s",appData.matchFilter);
	  logme(LOG_DEBUG,"Error code=%d\n",ret);
	  if(!ret){
	      if(value->type!=V_BOOLEAN){
		  logme(LOG_ERROR,"matchFilter does not return a boolean value...");
	      }else{
		  logme(LOG_DEBUG,"Return value=%s\n",value->value?"true":"false");
		  if(!value->value){
		      logme(LOG_DEBUG,"Rejecting match as matchFilter=false\n");
		      SendToIcs("decline %s",name);
		      parsingIncoming=FALSE;
		      return TRUE;
		  }
	      }
	  }
      }

    if(!runData.inTourney && IsNoPlay(name)){
        logme(LOG_DEBUG,"Ignoring %s as he is on our noplay list.",name);
        SendToIcs("decline %s\n",name);
	parsingIncoming=FALSE;
        return TRUE;
    }
    //    if(runData.inTourney && strcmp(runData.tournamentOppName,name)){
    //  SendToIcs("tell %s Sorry I am playing a tourney\n",name);
    //  SendToIcs("decline %s\n",name);
    //  return TRUE;
    //}
    if (appData.acceptOnly && strcasecmp(name,appData.acceptOnly)){
      SendToIcs("tell %s Sorry I am busy right now.\n",name);
      SendToIcs("decline %s\n", name);
      parsingIncoming=FALSE;
      return TRUE;
    }

    if (runData.quitPending) {
      SendToIcs("tell %s Sorry I have to go.\n", name);
      SendToIcs("decline %s\n", name);
      parsingIncoming=FALSE;
      return TRUE;
    }
    if (!runData.inTourney && appData.hardLimit && 
	(runData.numGamesInSeries >= appData.hardLimit) && 
	!strcasecmp(runData.lastPlayer,name)){
      logme(LOG_DEBUG,"Hard limit reached with %s.",name);
      SendToIcs("tell %s You have played me %d times in a row;  I need to take a rest.\n", name, runData.numGamesInSeries);
      SendToIcs("decline %s\n", name);
      parsingIncoming=FALSE;
      return TRUE;
    }
    if (!runData.inTourney && appData.limitRematches &&
	!strcmp(runData.lastPlayer, name) &&
	runData.numGamesInSeries > appData.limitRematches &&
	time(0) - runData.timeOfLastGame < 60) {
      SendToIcs("tell %s You have played me %d times in a row;  I'll wait a minute for other players to get a chance to challenge me before I accept your challenge.\n", name, runData.numGamesInSeries);
      CancelTimers();
      create_timer(&(runData.findChallengeTimer),FINDCHALLENGETIMEOUT,FindChallenge,NULL);
      return TRUE;
    } 



    return TRUE;
  }
  if(parsingIncoming){
      line1=strstr(line,"Win");
      if(line1 && sscanf(line,"Win: %d,  Draw: %d,  Loss: %d",&win,&draw,&loss)==3){
	  return TRUE;
      }
      if(strstr(line,"computer")){
	  computer=TRUE;
	  return TRUE;
      }
      if(strstr(line,"accept") && strstr(line,"decline")){
	  parsingIncoming=FALSE;
	  InjectChallenge(name, 
			  rating, 
			  has_color, 
			  color, 
			  name2, 
			  rating2, 
			  rated,
			  variant,
			  time_, 
			  inc,
			  win,
			  draw,
			  loss,
			  computer);
	  if(!runData.inTourney){
	      logme(LOG_DEBUG,"Executing matchFilter command: \"%s\"",appData.matchFilter);
	      ret=eval(value,"%s",appData.matchFilter);
	      logme(LOG_DEBUG,"Error code=%d\n",ret);
	      if(!ret){
		  if(value->type!=V_BOOLEAN){
		      logme(LOG_ERROR,"matchFilter does not return a boolean value...");
		  }else{
		      logme(LOG_DEBUG,"Return value=%s\n",value->value?"true":"false");
		      if(!value->value){
			  logme(LOG_DEBUG,"Rejecting match as matchFilter=false\n");
			  SendToIcs("decline %s",name);
			  return TRUE;
		      }
		  }
	      }
	  }
	  SendToIcs("accept %s\n", name);
	  return TRUE;
      }
  }

  return FALSE;
}

Bool ProcessStandings(char *line){
    int tournamentId;
    static Bool parsingStandings=FALSE;
    char c[2];
    if(!runData.loggedIn) return FALSE;
    if(!runData.parsingStandings) return FALSE;

    // first line of standings
    if(sscanf(line,":Tourney #%d's standings%1[:]",&tournamentId,c)==2 && 
       tournamentId==runData.lastTourney){
        StartMultiFeedback(CONSOLE|OWNER|SHORTLOG);
        logme(LOG_DEBUG,"Detected start of standings of #%d",tournamentId);
	parsingStandings=TRUE;
        Feedback(CONSOLE|SHORTLOG,"Tourney #%d standings:",tournamentId);
        return TRUE;
    }
    // last line of standings
    if(parsingStandings &&  (!strncmp(line,":*: Paired.",11) ||
			     !strncmp(line,":Total:",7)) 
       ){
        logme(LOG_DEBUG,"Detected end of standings");
        runData.parsingStandings=FALSE;
	parsingStandings=FALSE;
        StopMultiFeedback(CONSOLE|OWNER|SHORTLOG);
        return TRUE;
    }
    if(parsingStandings &&
       (
           !strncmp(line,":+-",3) ||
           !strncmp(line,":| ",3) ||
           !strncmp(line,":|-",3)
        )){
        line=strtok(line,"\r\n");
	/* 
	 *  Owner feedback of the standigns looks ugly
	 *  due to the limited possibilities of tells.
	 *  Later we will make the standings more compact.
	 */
        Feedback(CONSOLE|SHORTLOG,line);
        return TRUE;
    }
    return FALSE;
 }

void InjectCurrentTourney(char *TM, char* type){
    value_t value[1];
    char *token;
    int inc;
    int time_;
    int etime;
    /* inject things in interpreter */
    //eval_debug=1;

    token=strtok(TM," ");
    eval_set("ct.tm",V_STRING,SY_RO,token);

    token=strtok(type," (\\)");
    eval_set("ct.time",V_NUMERIC,SY_RO,time_=atoi(token));

    token=strtok(NULL," (\\)");
    eval_set("ct.inc",V_NUMERIC,SY_RO,inc=atoi(token));

    // compute a dependent variable
    etime=60*time_+40*inc;
    eval_set("ct.etime",V_NUMERIC,SY_RO,etime);

    token=strtok(NULL," (\\)");
    if(!strcmp(token,"r")){
	eval_set("ct.rated",V_BOOLEAN,SY_RO,1);
    }else{
	eval_set("ct.rated",V_BOOLEAN,SY_RO,0);
    }

    eval_set("ct.lightning",V_BOOLEAN,SY_RO,0);
    eval_set("ct.blitz",V_BOOLEAN,SY_RO,0);
    eval_set("ct.standard",V_BOOLEAN,SY_RO,0);
    eval_set("ct.chess",V_BOOLEAN,SY_RO,0);
    eval_set("ct.atomic",V_BOOLEAN,SY_RO,0);
    eval_set("ct.suicide",V_BOOLEAN,SY_RO,0);
    eval_set("ct.losers",V_BOOLEAN,SY_RO,0);
    eval_set("ct.crazyhouse",V_BOOLEAN,SY_RO,0);
    eval_set("ct.wild",V_BOOLEAN,SY_RO,0);

    token=strtok(NULL," (\\)");
    if(!strcmp(token,"wild") || !strcmp(token,"los") || !strcmp(token,"sui") 
       || !strcmp(token,"atom") || !strcmp(token,"zh")){
	logme(LOG_DEBUG,"This is a variant tourney.");
	if(!strcmp(token,"los")){
	    eval_set("ct.variant",V_STRING,SY_RO,"losers");
	    eval_set("ct.losers",V_BOOLEAN,SY_RO,1);
	}else if(!strcmp(token,"sui")){
	    eval_set("ct.variant",V_STRING,SY_RO,"suicide");
	    eval_set("ct.suicide",V_BOOLEAN,SY_RO,1);
	}else if(!strcmp(token,"atom")){
	    eval_set("ct.variant",V_STRING,SY_RO,"atomic");
	    eval_set("ct.atomic",V_BOOLEAN,SY_RO,1);
	}else if(!strcmp(token,"zh")){
	    eval_set("ct.variant",V_STRING,SY_RO,"crazyhouse");
	    eval_set("ct.crazyhouse",V_BOOLEAN,SY_RO,1);
	}else if(!strcmp(token,"wild")){
	    char buffer[256];
	    int i;
	    // wild needs special treatment
	    token=strtok(NULL," ");
	    snprintf(buffer,255,"wild/%s",token);
	    buffer[255]='\0';
	    for(i=0;i<strlen(buffer);i++){
		if(isupper(buffer[i])){
		    buffer[i]=tolower(buffer[i]);
		}
	    }
	    eval_set("ct.variant",V_STRING,SY_RO,buffer);
	    eval_set("ct.wild",V_BOOLEAN,SY_RO,1);
	}
	token=strtok(NULL," (\\)");
    }else{
	eval_set("ct.chess",V_BOOLEAN,SY_RO,1);
	if(etime<180){
	    eval_set("ct.variant",V_STRING,SY_RO,"lightning");
	    eval_set("ct.lightning",V_BOOLEAN,SY_RO,1);
	}else if(etime<900){
	    eval_set("ct.variant",V_STRING,SY_RO,"blitz");
	    eval_set("ct.blitz",V_BOOLEAN,SY_RO,1);
	}else{
	    eval_set("ct.variant",V_STRING,SY_RO,"standard");
	    eval_set("ct.standard",V_BOOLEAN,SY_RO,1);
	}
    }
    eval_set("ct.type",V_STRING,SY_RO,token);

    if(!(!strcasecmp(token,"drr") || !strcasecmp(token,"rr"))){
	// non RR or DDR have rounds
	token=strtok(NULL," (\\)");
	if(token){
	    eval_set("ct.rounds",V_NUMERIC,SY_RO,atoi(token));
	}else{
	    eval_set("ct.rounds",V_NUMERIC,SY_RO,0);
	}
    }else{
	eval_set("ct.rounds",V_NUMERIC,SY_RO,0);
    }

    char *command="log(\"ct.tm=\"+str(ct.tm)+\" ct.time=\"+str(ct.time)+\" ct.inc=\"+str(ct.inc)+\" ct.etime=\"+str(ct.etime)+\" ct.rated=\"+str(ct.rated)+\" ct.variant=\"+str(ct.variant)+\" ct.type=\"+str(ct.type)+\" ct.rounds=\"+str(ct.rounds))";
    char *command1="log(\"ct.lightning=\"+str(ct.lightning)+\" ct.blitz=\"+str(ct.blitz)+\" ct.standard=\"+str(ct.standard)+\" ct.chess=\"+str(ct.chess)+\" ct.atomic=\"+str(ct.atomic)+\" ct.suicide=\"+str(ct.suicide)+\" ct.losers=\"+str(ct.losers)+\" ct.crazyhouse=\"+str(ct.crazyhouse)+\" ct.wild=\"+str(ct.wild))";

    eval(value,"%s",command);
    eval(value,"%s",command1);
}

Bool ProcessTourneyNotifications(char *line){
  int tournamentId;
  Bool endOfTournament=FALSE;
  static Bool hideFromProxy=FALSE;
  /* dummies for now */
  char c[2];
  char open[30+1];
  char TM[30+1];
  char type[30+1];
  char date[30+1];
  char color[30+1];
  char name[30+1];
  value_t value[1];
  int ret;

  if(hideFromProxy){
      runData.hideFromProxy=TRUE;
  }

  if(!runData.loggedIn) return FALSE;
  /* join messages */
  if(strstr(line,"You have joined") && !strstr(line,"has been changed") &&
     (sscanf(line,"%*[^#]#%d",&tournamentId)==1)){
    /* does this work? */
    if(runData.inTourney || runData.inGame){
      logme(LOG_DEBUG,"It seems we have joined more than one tourney."
	              "Or else we are playing a game."
	              "Attempting to withdraw from this tourney.");
      SendToIcs("td WithdrawFromTourney %d\n",tournamentId);
    }else{
      logme(LOG_DEBUG,"Yippee! We have joined tourney #%d",tournamentId);
      Feedback(OWNER|CONSOLE|SHORTLOG|PROXY,"%s has joined tourney #%d.",
	runData.handle,tournamentId);
      runData.currentTourney=tournamentId;
      strcpy(runData.tournamentOppName,"");
      runData.inTourney=TRUE;
    }
    return TRUE;
  }
  /* end of tournament */
  if(runData.inTourney &&
     sscanf(line,":Tourney #%d has been aborted%1[!]",
	    &tournamentId,c)==2 &&
     tournamentId==runData.currentTourney){
    logme(LOG_DEBUG,"Tourney #%d has been aborted.",tournamentId);
    Feedback(OWNER|CONSOLE|SHORTLOG|PROXY,
	     "Tourney #%d has been aborted.",
	     tournamentId);
    endOfTournament=TRUE;
  }else if(runData.inTourney && 
	   sscanf(line,":You have been forfeited from tourney #%d",
		  &tournamentId)==1
	   ){
    logme(LOG_DEBUG,"%s has been forfeited from tourney #%d.",
	  runData.handle,
	  tournamentId);
    Feedback(OWNER|CONSOLE|SHORTLOG|PROXY,"%s has been forfeited from tourney #%d.",
	  runData.handle,
	  tournamentId);
    endOfTournament=TRUE;
  }else if(runData.inTourney &&
	   strstr(line,"mamer has set your tourney variable to OFF.")){
    logme(LOG_DEBUG,
	  "%s has played its last game in tourney #%d.",
	  runData.handle, runData.currentTourney);
    Feedback(OWNER|CONSOLE|SHORTLOG|PROXY,
	     "%s has played its last game in tourney #%d.",
	     runData.handle,runData.currentTourney);
    endOfTournament=TRUE;
  }
  if(endOfTournament){
    runData.inTourney=FALSE;
    runData.lastTourney=runData.currentTourney;
    runData.parsingStandings=TRUE;
    runData.currentTourney=-1;
    GetTourney();
    /* Should be sendTourneyEnd... */
    if (appData.sendGameEnd){
	ExecCommandList(appData.sendGameEnd,0,FALSE);
    }
    if(!runData.inGame){  /* for safety */
      CancelTimers();
      if (appData.sendTimeout) {
          create_timer(&(runData.idleTimeoutTimer),
		     IDLETIMEOUT,
		     IdleTimeout,
		     NULL);
      }
    }
    return TRUE;
  }
  /* Various kinds of tourney announcements. There are more. */
  if(strstr(line,"is open")||
     strstr(line,"has been opened")||
     strstr(line,"reopened")||
     strstr(line,"to join")||
     strstr(line,"to late-join")){
    /* for debugging; too verbose */
    if(runData.inGame){
      logme(LOG_DEBUG,"Not joining tourney since we are in a game.");
      return TRUE;
    }
    if(runData.inTourney){
      logme(LOG_DEBUG,"Not joining tourney since we are already in a tourney.");
      return TRUE;
    }
    GetTourney();
    return FALSE;  /* our tests are rudimentary so we give other classifiers a chance to look */
  }
  /* First line of result of "tdListTourneys -j" */
  if(runData.parsingListTourneys && !strncmp(line,":mamer's tourney list:",22)){
      hideFromProxy=TRUE;
      runData.hideFromProxy=TRUE;
  }
  /* Last line of result of "td ListTourneys -j" */
  if(runData.parsingListTourneys && !strncmp(line,":Listed:",8)){
    runData.parsingListTourneys=FALSE;
    hideFromProxy=FALSE;
    runData.hideFromProxy=TRUE;
    return TRUE;
  }
  /* result of "td ListTourneys -j" */
  if(
     runData.parsingListTourneys &&
     sscanf(line,":| %d | %30[^|] | %30[^|] | %30[^|] | %30[^|] |",&tournamentId,open,TM,type,date)==5
     ){
      logme(LOG_DEBUG,"It seems we qualify for tourney [#%d] [%s] [%s] [%s] [%s]\n",
	    tournamentId,
	    open,
	    TM,
	    type,
	    date);
    if(runData.inGame){
      logme(LOG_DEBUG,"Not joining tourney since we are in a game.");
      return TRUE;
    }
    if(runData.inTourney){
      logme(LOG_DEBUG,"Not joining tourney since we are already in a tourney.");
      return TRUE;
    }
    
    InjectCurrentTourney(TM,type);

    logme(LOG_DEBUG,"Executing tourneyFilter command: \"%s\"",appData.tourneyFilter);
    ret=eval(value,"%s",appData.tourneyFilter);
    logme(LOG_DEBUG,"Error code=%d\n",ret);
    if(!ret){
	if(value->type!=V_BOOLEAN){
	    logme(LOG_ERROR,"tourneyFilter does not return a boolean value...");
	}else{
	    logme(LOG_DEBUG,"Return value=%s\n",value->value?"true":"false");
	    if(value->value){
		logme(LOG_DEBUG,"Trying to join tourney %d\n",tournamentId);
		SendToIcs("td join %d\n",tournamentId);
	    }else{
		logme(LOG_DEBUG,"Not joining tourney as tourneyFilter=false\n");
	    }
	}
    }
    return TRUE;
  }
  
  if(!runData.inTourney){
    return FALSE;
  }
  if((sscanf(line,":You play %s against %s in this round of tourney #%d.",
	    color,name,&tournamentId)==3) && 
     tournamentId==runData.currentTourney){
    logme(LOG_DEBUG,"[oppname=%s] [color=%s] [tournamentId=%d]\n",
	  name,color,tournamentId);
    strncpy(runData.tournamentOppName,name,30);
    SendToIcs("td play %d\n",tournamentId);
    return TRUE;
  }
  return FALSE;
}


Bool ProcessFlaggedOpponent(char *line){
  if(!runData.loggedIn) return FALSE;
  /*
   *  If we flagged opponent and he is not out of time, setup alarm for
   *  courtesyadjourning after x minutes
   */
  if (!strncasecmp(line, "Opponent is not out of time.", 28) || 
     !strncasecmp(line,  "Your opponent is not out of time.",33)) {
    CancelTimers();
    create_timer(&(runData.courtesyAdjournTimer),
                 COURTESYADJOURNINTERVAL,
                 CourtesyAdjourn,
                 NULL);
    logme(LOG_INFO,"Setting up courtesy adjourn/abort in %d seconds.",
          COURTESYADJOURNINTERVAL);
    return TRUE;
  }
  return FALSE;
}

Bool ProcessStartOfGame(char *line){
  char name[30+1],name2[30+1],rating[30+1],rating2[30+1];
  char rated[30+1];
  char color[30+1];
  int time, inc;
  int i;
  if(!runData.loggedIn) return FALSE;
  /*
   *  Detect start of game, find gametype and rating of opponent
   */
  if ((sscanf(line, 
	     "Creating: %30s (%30[^)]) %30s (%30[^)]) %30s %30s %d %d", 
	     name, 
	     rating, 
	     name2, 
	     rating2, 
	     rated, 
	     runData.icsVariant, 
	     &time, 
	     &inc) == 8)
   || (sscanf(line, 
	     "Creating: %30s (%30[^)]) [%30[^]]] %30s (%30[^)]) %30s %30s %d %d", 
	     name, 
	     rating, 
	     color, 
	     name2, 
	     rating2, 
	     rated, 
	     runData.icsVariant, 
	     &time, 
	     &inc) == 9)
   ) {
    logme(LOG_DEBUG, "Detected start of game: [%s] (%s) vs [%s] (%s) [%s] [%s] %d %d", 
	  name, rating, name2, rating2,rated,runData.icsVariant,time,inc);
    runData.hideFromProxy=TRUE;
    if(appData.sendGameStart){
        ExecCommandList(appData.sendGameStart,0,FALSE);
    }
    {
        int mask=CONSOLE|SHORTLOG|PROXY;
        if(!appData.ownerQuiet){
            mask|=OWNER;
        }
        if(!runData.inTourney){
            Feedback(mask,"%s %s vs %s %s %s %s %d %d started.", 
                     name, rating, name2, rating2,rated,runData.icsVariant,time,inc);
        }else{
            Feedback(mask,
                     "%s %s vs %s %s %s %s %d %d started (tourney #%d).", 
                     name, rating, name2, rating2,rated,runData.icsVariant,time,inc,
                     runData.currentTourney);
        }
    }
    /* update our state */
    runData.inGame=TRUE;
    /* send opponent name and ratings to computer */
    if (!strcmp(name, runData.handle)) {
	SendToComputer("name %s\n", name2);
	SendToComputer("rating %d %d\n", atoi(rating), atoi(rating2));
    }
    /* send variant to computer */
    strcpy(runData.chessVariant,"normal");
    runData.noCastle=FALSE;
    for(i=0;i<runData.icsVariantCount;i++){
	if(IsVariant(runData.icsVariant,runData.icsVariants[i][0])){
	   char *variant1;
	   variant1=CheckChessVariantSupport(runData.icsVariants[i][1]);
	   if(variant1){
	       if(!strcmp(runData.icsVariants[i][1],"nocastle")){
		   runData.noCastle=TRUE;
	       }
	       strcpy(runData.chessVariant,runData.icsVariants[i][1]);	
	       if(runData.noCastle){
		   SendToComputer("variant normal\n");
	       }else{
		   SendToComputer("variant %s\n",variant1);
	       }
	       if(!strcmp(runData.chessVariant,"fischerandom")){
		   logme(LOG_DEBUG,"Enabling FRC castling.");
	       }
	       break;
	   }else{
	       logme(LOG_DEBUG,"Something went badly wrong...\n");
	       SendToIcs("abort\n");
	       return TRUE;
	   }
    	}
    }
    logme(LOG_DEBUG,"Chess variant is now set to \"%s\"",runData.chessVariant);
    runData.useMoveList=UseMoveList();
    if (!strcmp(name2, runData.handle)) {
	SendToComputer("name %s\n", name);
	SendToComputer("rating %d %d\n", atoi(rating2), atoi(rating));
    }
    if(runData.useMoveList){
	/* reset movelist and ask for the moves */
	runData.moveList[0] = '\0';
	SendMarker(ASKSTARTMOVES);
	if (runData.longAlgMoves)
	    InternalIcsCommand("moves l\n");
	else
	    InternalIcsCommand("moves\n");
	runData.waitingForMoveList =TRUE;
    }
    /* update our state */
    runData.waitingForFirstBoard = TRUE;
    runData.engineMovesPlayed=0;
    return TRUE;
  }
  /*
   *  Fetch the game ID when a game is started.
   */
  if (!strncmp(line, "{Game ", 6) &&
      strstr(line, runData.handle) &&
      (strstr(line, ") Creating ") || strstr(line, ") Continuing "))) {
      runData.hideFromProxy=TRUE;
    sscanf(line, "{Game %d", &runData.gameID);
    logme(LOG_INFO, "Current game has ID: %d", runData.gameID);
    return TRUE;
  } 
  return FALSE;
}

void ProcessFeedback(char *line){
  char *p;
  if(!runData.loggedIn) return ;
  /* anything else might be feedback */
  if(IsWhiteSpace(line)){
      p="";
  }else{
      p = strtok(line,"\r\n");
  }
  if(p && runData.forwarding){
    if(strstr(line,"Your communication has been queued for")) return;
    if(strstr(line,"You have reached the communication")) return;
    Feedback(runData.forwardingMask,"%s",p);
    runData.forwarding++;
  }
}

Bool ProcessMoveList(char *line){
    move_t move,move2;
    char dummy;
    if(!runData.loggedIn) return FALSE;
    if(!runData.waitingForMoveList) return FALSE;
    if(IsMarker(ASKSTARTMOVES,line)){
        logme(LOG_DEBUG,"Start of move list detected.");
        runData.parsingMoveList=TRUE;
        return TRUE;
    }    
    if(runData.parsingMoveList){
            /*
             *  Retrieve the movelist
             */
        if (sscanf(line, "%*3d. %15s (%*s %15s (%*s", move, move2) == 2) {
            if (runData.longAlgMoves) {
                ConvIcsLanToComp('W',move);
                ConvIcsLanToComp('B',move2);
            } else {
                ConvIcsSanToComp(move);
                ConvIcsSanToComp(move2);
            }
            sprintf(runData.moveList+strlen(runData.moveList),"%s\n%s\n",move,move2);
            return TRUE;
        }
        if (sscanf(line, "%*3d. %15s (%*s %15s (%*s", move, move2) == 1) {
            if (runData.longAlgMoves)
                ConvIcsLanToComp('W',move);
            else
                ConvIcsSanToComp(move);
            sprintf(runData.moveList+strlen(runData.moveList),"%s\n", move);
            return TRUE;
        }     
            /*
             *  Write movelist to program and prepare it to play
             */
        if (sscanf(line, " {%*s in progress} %c", &dummy) == 1) {
            logme(LOG_INFO, "Found end of movelist; %c on move", 
                  runData.icsBoard.onMove);
            SetupEngineOptions(&(runData.icsBoard));
            HandleBoard(&(runData.icsBoard),runData.moveList,FALSE);
            runData.parsingMoveList=FALSE;
            runData.waitingForMoveList=FALSE;
            return TRUE;
        }
    }
    return FALSE;
}

Bool ProcessBoard(char *line){
  char *oppname;
  IcsBoard icsBoardCopy;
  if(!runData.loggedIn) return FALSE;
  if(!ParseBoard(&(runData.icsBoard),line)) return FALSE;
  memcpy(&icsBoardCopy,&runData.icsBoard,sizeof(IcsBoard));
  if(runData.icsBoard.status==1 /* my move */ || runData.icsBoard.status==-1 /* your move */){      icsBoardCopy.status=0; // observing
  }      
  BoardToString(runData.lineBoard, &icsBoardCopy);
  if(!runData.inGame) return TRUE;
  if(runData.icsBoard.status!=1 &&  runData.icsBoard.status!=-1){
      return TRUE;
  }
  if(runData.waitingForFirstBoard){
    if (!strcmp(runData.icsBoard.nameOfWhite, runData.handle)) {
      logme(LOG_INFO, "I'm playing white.");
      runData.computerIsWhite = TRUE;
      oppname = runData.icsBoard.nameOfBlack;
      
    } else {
      logme(LOG_INFO, "I'm playing black.");
      runData.computerIsWhite = FALSE;
      oppname = runData.icsBoard.nameOfWhite;
    }
    if (!strcmp(runData.lastPlayer, oppname)) {
      runData.numGamesInSeries++;
    } else {
      runData.numGamesInSeries = 1;
      strcpy(runData.lastPlayer, oppname);
    }
    if(!runData.useMoveList){
	Bool ignoreStartBoard;
	ignoreStartBoard=FALSE;
	if(!IsShuffle(runData.chessVariant)){
	    ignoreStartBoard=TRUE;
	}
	SendBoardToComputer(&runData.icsBoard,ignoreStartBoard);
	SetupEngineOptions(&(runData.icsBoard));
	HandleBoard(&(runData.icsBoard),NULL,TRUE);
    }
    runData.waitingForFirstBoard = FALSE;

    return TRUE;
  }
  if(runData.waitingForMoveList){
    /* 
     * Avoid prematurely sending moves to the engine. They will be sent as part
     * of the movelist. 
     */
      return TRUE;
  }
  HandleBoard(&(runData.icsBoard),NULL,FALSE);
  return TRUE;
}

Bool ProcessGameEnd(char *line){
  char resultString[100+1];
  char result[30+1];
  char handle1[30];
  char handle2[30];
  if(!runData.loggedIn) return FALSE;  
  /* 
   *  Check for game-end messages.
   *  We dont care how the game ended, just that it ended.
   */
  memset(resultString,0,sizeof(resultString));
  if(sscanf(line,"{Game %*d (%17s vs. %18s%*[ ]%100[^}]} %30s",
	    handle1,
	    handle2,
	    resultString,
	    result)==4){
    resultString[sizeof(resultString)-1]='\0';
    handle1[sizeof(handle1)-1]='\0';
    handle2[sizeof(handle2)-1]='\0';
    // handle2 contains an extra ) but we don't care.
    logme(LOG_DEBUG, "[resultString=%s] [handle1=%s] [handle2=%s] [result=%s]",
	  resultString,handle1,handle2,result);
    if(strncasecmp(handle1,runData.handle,strlen(runData.handle)) &&
       strncasecmp(handle2,runData.handle,strlen(runData.handle))){
		       return TRUE;
       }		 
    logme(LOG_INFO, "Detected end of game: %s", line);
    {
        int mask=CONSOLE|SHORTLOG|PROXY;
        if(!appData.ownerQuiet){
            mask|=OWNER;
        }
        Feedback(mask,"%s (%s).",resultString, result);
    }
    /* update our state */
    runData.inGame=FALSE;
    /* cancel abort or flag timers */
    CancelTimers();
    if(!strstr(resultString,"abort") && !strstr(resultString,"adjourn") ){
        runData.lastGameWasAbortOrAdjourn=FALSE;
      /* This does not work if a stored game is adjudicated right at 
       * this point... We will get the moves of the adjudicated game rather 
       * than from the last game. Chances of this happening are slim though.
       */
        if(appData.pgnLogging){
	    char tmp[100];
            SendMarker(ASKLASTMOVES);
	    snprintf(tmp,sizeof(tmp)-1,"smoves %s -1\n",runData.handle);
	    tmp[sizeof(tmp)-1]='\0';
            InternalIcsCommand(tmp);
        }
        persistentData.games++;
        SetInterface();
    }else{
        runData.lastGameWasAbortOrAdjourn=TRUE;
        logme(LOG_INFO,"The last game was aborted or adjourned. Not recording.");
    }
    /* 
     * By the time time that the Cleanups are done inTourney may have become
     * false. 
     */
    if(runData.inTourney){
      runData.lastGameWasInTourney++;
    }
    SendMarker(DOCLEANUPS);
    runData.gameID = -1;
    runData.moveNum = -1;
    runData.timeOfLastGame = time(0);
    if (runData.sigint) {
      InterruptComputer();
    }
    if (runData.haveCmdResult && !strcmp(result,"*")) {
      Result(result);
    }
    NewGame();
    return TRUE;
  }
  return FALSE;
}

Bool ProcessMiscFilters(char *line){
  /* Filter out some inconvienient lines.
   */
  if(strstr(line,"You can \"accept\" or \"decline\", or propose different parameters.")) return TRUE;
  return FALSE;   
}

Bool ProcessMoreTime(char *line){
  int seconds;
  if(!runData.loggedIn) return FALSE;  
  if(sscanf(line,"Your opponent has added %d",&seconds)==1){
    SendToIcs("say Thanks for the extra time but I don't need it.\n");
    /*if(runData.waitingForFirstBoard || runData.icsBoard.nextMoveNum==1){
      CancelTimers();
      SendToIcs("abort\n");
      }*/
    return TRUE;
  }
  return FALSE;
}

Bool ProcessCreatePGN(char *line){
  static int moveNumber=1;
  static char pgnDesc[BUF_SIZE]="";
  char miniPgnDesc[20]="";
  /*  static char *lastPgnDesc=NULL;*/
  move_t move,move2;
  char result[30+1];
  char resultString[60+1];
  FILE *pgnFile;
  static char nameWhite[30+1]="";
  static char nameBlack[30+1]="";
  static char ratingWhite[30+1]="";
  static char ratingBlack[30+1]="";
  int eloWhite;
  int eloBlack;
  static char pgnDate[11]="";
  static char pgnTime[9]="";
  static char dayOfTheWeek[30+1]="";
  static char month[30+1]="";
  static int day=0;
  static char hoursSecs[30+1]="";
  static char year[30+1]="";
  static char timezone[30+1]="";
  static char rated[30+1]="";
  static char variant[30+1]="";
  static int initial=0;
  static int increment=0;
  static int state=0;
  static int colNo=0;

  /*  static char *lastLine=NULL;*/
  char dummy;
  if(!runData.loggedIn) return FALSE;  
  if(IsMarker(ASKLASTMOVES,line)){
    logme(LOG_DEBUG,"Start of last move list detected.");
    runData.processingLastMoves=TRUE;
    state=0;
    colNo=0;
    return TRUE;
  }
  if(!runData.processingLastMoves) return FALSE;
  if(sscanf(line,"%*s has no history games%c",&dummy)==1){
    logme(LOG_DEBUG,"No history.");
    runData.processingLastMoves=FALSE;
    return TRUE;
  }
  // FICS start of movelist
  if(state==0 && sscanf(line,"%30s (%[^)]) vs. %30s (%[^)]) --- %30s %30s %d, %30s %30s %30s",
			nameWhite,
			ratingWhite,
			nameBlack,
			ratingBlack,
			dayOfTheWeek,
			month,
			&day,
			hoursSecs,
			timezone,
			year)==10){
    snprintf(pgnDate, sizeof(pgnDate), "%s.%02d.%02d", year, MonthNumber(month), day);
    snprintf(pgnTime, sizeof(pgnTime), "%s:00", hoursSecs);
    state=-1;
  }
  
  // ICS start of movelist
  if(state==0 && sscanf(line,"%30s (%[^)]) vs. %30s (%[^)]) --- %10s %8s",
			nameWhite,
			ratingWhite,
			nameBlack,
			ratingBlack,
			pgnDate,
			pgnTime)==6 && pgnDate[4]=='.' && pgnTime[2]==':') {
      state=-1; // slight abuse of state
  }
  
  if (state==-1) {
      logme(LOG_DEBUG,"[nameWhite=%s]  [ratingWhite=%s] [nameBlack=%s] [ratingBlack=%s] [pgnDate=%s] [pgnTime=%s]",
	    nameWhite,
	    ratingWhite,
	    nameBlack,
	    ratingBlack,
	    pgnDate,
	    pgnTime);
      state=1;
      return TRUE;
  }
  
  if(state==1 && sscanf(line,"%30s%30s match, initial time: %d minute%*2s increment: %d",
			rated,
			variant,
			&initial,
			&increment)==4){
      state=2;
      rated[0]=tolower(rated[0]);
      logme(LOG_DEBUG,"[rated=%s]  [variant=%s] [initial=%d] [increment=%d]",
	    rated,
	    variant,
	    initial,
	    increment);
      return TRUE;
  }
  if (state==2 && sscanf(line, "%3d. %15s (%*s %15s (%*s", &moveNumber, move, move2) == 3) {
    ConvIcsSanToComp(move);
    ConvIcsSanToComp(move2);
    snprintf(miniPgnDesc, sizeof(miniPgnDesc), "%d. %s %s ", moveNumber, move, move2);
    if ((colNo + strlen(miniPgnDesc)) > MAX_PGN_FILE_LINE) {
      snprintf(pgnDesc+strlen(pgnDesc), sizeof(pgnDesc)-strlen(pgnDesc), "\n%s", miniPgnDesc);
      colNo = 0;
    }
    else {
      snprintf(pgnDesc+strlen(pgnDesc), sizeof(pgnDesc)-strlen(pgnDesc), "%s", miniPgnDesc);
    }
    colNo+=strlen(miniPgnDesc);
    return TRUE;
  }
  if (state==2 && sscanf(line, "%3d. %15s (%*s %15s (%*s", &moveNumber, move, move2) == 2) {
    ConvIcsSanToComp(move);
    snprintf(miniPgnDesc, sizeof(miniPgnDesc), "%d. %s ", moveNumber, move);
    if ((colNo + strlen(miniPgnDesc)) > MAX_PGN_FILE_LINE) {
      snprintf(pgnDesc+strlen(pgnDesc), sizeof(pgnDesc)-strlen(pgnDesc), "\n%s", miniPgnDesc);
      colNo = 0;
    }
    else {
      snprintf(pgnDesc+strlen(pgnDesc), sizeof(pgnDesc)-strlen(pgnDesc), "%s", miniPgnDesc);
    }
    colNo+=strlen(miniPgnDesc);
 
    return TRUE;
  }  
  memset(result,0,30+1);
  memset(resultString,0,60+1);
  if(state==2 && sscanf(line,"%*[ ]{%60[^}]}%*[ ]%30[^ \r\n]",resultString,result) ==2){
    state=0;
    logme(LOG_DEBUG,"[ResultString=%s] [Result=%s]",resultString,result);
    runData.processingLastMoves=FALSE;
    if (appData.pgnFile[0] == '|') {
      pgnFile=popen(appData.pgnFile+1,"w");
    } else {
      pgnFile=fopen(appData.pgnFile,"a");
    }
    fprintf(pgnFile,"[Event \"ICS %s %s game\"]\n",rated,variant);
    fprintf(pgnFile,"[Site \"%s %d\"]\n", appData.icsHost, appData.icsPort);
    fprintf(pgnFile,"[Date \"%s\"]\n",pgnDate);
    fprintf(pgnFile,"[Time \"%s\"]\n",pgnTime);
    fprintf(pgnFile,"[Round \"-\"]\n");
    fprintf(pgnFile,"[White \"%s\"]\n",nameWhite);
    fprintf(pgnFile,"[Black \"%s\"]\n",nameBlack);
    eloWhite=atoi(ratingWhite);
    eloBlack=atoi(ratingBlack);
    if(eloWhite==0){
      fprintf(pgnFile,"[WhiteElo \"-\"]\n");
    }else{
      fprintf(pgnFile,"[WhiteElo \"%d\"]\n",eloWhite);
    }
    if(eloBlack==0){
      fprintf(pgnFile,"[BlackElo \"-\"]\n");
    }else{
      fprintf(pgnFile,"[BlackElo \"%d\"]\n",eloBlack);
    }
    fprintf(pgnFile,"[TimeControl \"%d+%d\"]\n",initial,increment);
    fprintf(pgnFile,"[Mode \"ICS\"]\n");
    fprintf(pgnFile,"[Result \"%s\"]\n",result);
    fprintf(pgnFile,"\n");
    fprintf(pgnFile,"%s",pgnDesc);
    if((colNo+strlen(resultString)) > MAX_PGN_FILE_LINE) {
      fprintf(pgnFile,"\n{%s} %s",resultString,result);
    }
    else {
      fprintf(pgnFile,"{%s} %s",resultString,result);
    }
    fprintf(pgnFile,"\n\n");
    if (appData.pgnFile[0] == '|') {
      pclose(pgnFile);
    } else {
      fclose(pgnFile);
    }
    pgnDesc[0]='\0';
    return TRUE;
  }
  if(runData.processingLastMoves && IsMarker(DOCLEANUPS,line)){
    logme(LOG_ERROR,"runData.processingLastMoves=TRUE while receiving \
DOCLEANUPS marker.");
    logme(LOG_INFO,"Putting runData.processingLastMoves=FALSE.");
    runData.processingLastMoves=FALSE;
    pgnDesc[0]='\0';
    state=0;
    colNo=0;
  }
  return FALSE;
}

Bool ProcessCleanUps(char *line){
  if(!runData.loggedIn) return FALSE;  
  if(IsMarker(DOCLEANUPS,line)){
    logme(LOG_DEBUG,"Start of extra cleanups at end of game.");
    if (runData.quitPending) {
      Feedback(CONSOLE|OWNER|SHORTLOG|PROXY,"%s logged out.",runData.handle);
      if (runData.exitValue == EXIT_RESTART)
	ExitOn(EXIT_RESTART,"Restart command executed (part of cleanups).");
      else		
	ExitOn(EXIT_HARDQUIT,"SoftQuit command executed.");
    }
    if(runData.lastGameWasInTourney==0){
      GetTourney();
      if (appData.sendGameEnd){
	  ExecCommandList(appData.sendGameEnd,0,FALSE);
      }
      if (appData.issueRematch && (!appData.acceptOnly || 
				   !strcasecmp(appData.acceptOnly,
					       runData.lastPlayer)) &&
	  (!appData.limitRematches || 
	   (runData.numGamesInSeries <= appData.limitRematches))){
              /*
               * A rematch after an aborted game is directed to some
               * earlier player which might be on our no play list.
               */
        if(!IsNoPlay(runData.lastPlayer) && !runData.lastGameWasAbortOrAdjourn){
          SendToIcs("rematch\n");
        }else{
           logme(LOG_DEBUG,"No rematch. Last player is on our noplay list");
        }
	if(!runData.inGame && !runData.inTourney){ /* for safety */
	  CancelTimers();
	  if (appData.sendTimeout) {
	    create_timer(&(runData.idleTimeoutTimer),
			 IDLETIMEOUT,
			 IdleTimeout,
			 NULL);
	  } 
	}
      }
    }else{
      runData.numGamesInSeries=0;
      runData.lastPlayer[0]='\0';
      runData.lastGameWasInTourney--;
      logme(LOG_DEBUG,"Not doing standard cleanups as we are in a tourney.");
    }
    return TRUE;
  }
  return FALSE;
}

Bool ProcessIllegalMove(char *line){
    if(!runData.loggedIn) return FALSE;  

    if(!strncasecmp(line,"Illegal move",12)){
      /* 
       * We have the problem that the illegal move might be from a previous game.
       */
	if(runData.engineMovesPlayed>0){
	    logme(LOG_DEBUG,"Something bad happened. Bailing out.");
	    SendToIcs("resign\n");
	}else{
	    logme(LOG_DEBUG,"Not bailing out since we are at the start of the game.");
	}
	return TRUE;
    }
    return FALSE;
}

void ProcessIcsLine(char *line, char *queue){
  char *old_line;
  old_line=line;
  line=KillPrompts(line);
  // Eol's are killed somewhere below. Debug!
  // For now we just keep a copy of line.
  old_line=strdup(line);
  runData.hideFromProxy=FALSE;
  if(ProcessInternalMarkers(line))goto finish;
  if(ProcessForwardingMarkers(line))goto finish;
  if(ProcessPings(line))goto finish;
  if(ProcessLogin(line))goto finish;
  if(ProcessTells(line))goto finish;
  if(ProcessNext(line))goto finish;
  if(ProcessPending(line))goto finish;
  if(ProcessOffers(line))goto finish;
  if(ProcessAutoMatches(line))goto finish;
  if(ProcessNotifications(line))goto finish;
  if(ProcessOwnerNotify(line))goto finish;
  if(ProcessIncomingMatches(line))goto finish;
  if(ProcessTourneyNotifications(line)) goto finish;
  if(ProcessStandings(line)) goto finish;
  if(ProcessFlaggedOpponent(line))goto finish;
  if(ProcessStartOfGame(line))goto finish;
  if(ProcessIllegalMove(line))goto finish;
  if(ProcessMoreTime(line))goto finish;
  if(ProcessMoveList(line))goto finish;
  if(ProcessBoard(line))goto finish;
  if(ProcessGameEnd(line))goto finish;
  if(ProcessCreatePGN(line))goto finish;
  if(ProcessCleanUps(line))goto finish;
  if(ProcessMiscFilters(line))goto finish;
  ProcessFeedback(line);

finish:
  if(!strcmp(old_line,"\012\015\015")){
      // This is a spurious character sequence inserted by FICS
      // timeseal. Since it is unlikely to occur normally
      // we just drop it. 
  }else if(IsMarker(PROXYPROMPT,old_line)){
      SendToProxy("%s", runData.lastIcsPrompt);
  }else if(!strncmp(old_line,"<12>",4)){
      SendToProxy("%s\r\n%s",runData.lineBoard,runData.lastIcsPrompt);
  }else if(runData.proxyLoginState==PROXY_LOGGED_IN &&
	   !runData.hideFromProxy &&
	   !runData.forwarding && 
	    !IsAMarker(old_line) && 
	   !runData.internalIcsCommand){
      if(!strncmp(runData.lastIcsPrompt,queue,6)){
	  SendToProxy("%s%s",old_line,runData.lastIcsPrompt);
      }else{
	  SendToProxy("%s",old_line);
      }
  }

  free(old_line);
}



