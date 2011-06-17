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

void Prompt(mask){
#ifdef HAVE_LIBREADLINE
  if((mask & CONSOLE) && appData.console && !runData.inhibitPrompt){
    if(runData.inReadline){
      rl_reset_line_state();
      rl_redisplay();
    }else{
      rl_callback_handler_install(runData.prompt,ProcessConsoleLine);  
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
  if(mask & CONSOLE){
    
    runData.multiFeedbackDepth++;
  }
}

void StopMultiFeedback(mask){
  if(mask & CONSOLE){
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
	   SendToProxy("%s\n",buf);
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
      Prompt(mask);
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
  /* if(!found && appData.owner){
    SendToIcs("tell %s No such option %s.\n",appData.owner,name);
  }*/
  return found;
}

void ExecFile(char * filename, int mask){
  FILE *f;
  char buf[256];
  char filenameBuf[256];
  /*  if(strpbrk(filename, " \r\n.\\/")){
    Feedback(mask,"Only ordinary characters are allowed in script name.");
    return;    
    }*/
  strncpy(filenameBuf,filename,256-9-1); /* 9=strlen(".icsdrone") */
  strcat(filenameBuf,".icsdrone");
  f=fopen(filenameBuf,"r");
  if(!f){
    Feedback(mask,"Could not open \"%s\".",filenameBuf);
    return;
  }
  while(myfgets(buf, sizeof(buf), f)) ExecCommand(buf,0);
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


void ExecCommand(char * command, int mask){
  int i;
  char key[256];
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
    ExecFile(value,mask); 
  } else if(((i=sscanf(command,"set %254[^=\n\r ] %8191[^\n\r]", key,value))==1)  || (i==2)){
    if(mask & CONSOLE){
      runData.inhibitPrompt=TRUE;
    }
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
  }else if(!strncmp(command,"help",4) && strstr(command,PACKAGE_NAME)){ /*temporary */
    StartMultiFeedback(mask);
    Feedback(mask,"Commands : help set softquit hardquit restart options coptions");
    Feedback(mask,"         : daemonize load");
    Feedback(mask,"Options  : searchdepth easymode secpermove sendgameend limitrematches");
    Feedback(mask,"           issuerematch sendgamestart acceptonly feedback feedbackonlyfinal");
    Feedback(mask,"           feedbackcommand pgnlogging scoreforwhite sendtimeout shortlogfile");
    Feedback(mask,"           shortlogging sendlogin hardlimit noplay autojoin autoreconnect");
    Feedback(mask,"           resign acceptdraw scoreforwhite ownerquiet enginequiet");
    Feedback(mask,"           colortell coloralert colordefault");
    Feedback(mask,"Read the manual page or README file for more information.");
    StopMultiFeedback(mask);
  }else if(!strncmp(command,"help",4)){
    StartForwarding(mask);
    if(mask & CONSOLE){
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

void ExecCommandList(char * command, int mask){
    char *command_c=strdup(command);
    char *command_cc=command_c;
    char *p;
    while(1){
        p=strstr(command_c,"\\n");
        if(p){
            p[0]='\0';
            ExecCommand(command_c, mask);
            command_c=p+2;
        }else{
            ExecCommand(command_c, mask);
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
  if(runData.registered && appData.autoJoin &&
     (!appData.acceptOnly || appData.acceptOnly[0]=='\0')){
    runData.parsingListTourneys=TRUE;
    SendToIcs("td ListTourneys -j\n");
  }
}

Bool EngineToMove(IcsBoard * icsBoard){
  return (icsBoard->status)==1;
}


void HandleBoard(IcsBoard * icsBoard, char *moveList){
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
    if(strcmp(move,"none") && !moveList){
      logme(LOG_INFO, "Move from ICS: %s %s", 
                     icsBoard->lanMove,icsBoard->sanMove);
      SendMoveToComputer(move);
    }
    if(strcmp(bmove.move,"none")){
      logme(LOG_INFO,"Bookmove: %s score=%d\n",bmove.move,bmove.score);
      SendToIcs("%s\n",bmove.move);
      if(appData.feedback && appData.feedbackCommand){
	  SendToIcs("%s Bookmove: %s score=%d\n",appData.feedbackCommand,bmove.move,bmove.score); 
      }
      SendMoveToComputer(bmove.move);
      if(runData.icsBoard.nextMoveNum>1){
	runData.calculatedTime+=100*runData.icsBoard.inctime;
	logme(LOG_DEBUG,"calculatedTime=%d",runData.calculatedTime);
      }
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
  //  if((eol=strchr(line,'\n'))){
  //    *eol='\0';
  //}
  //if((eol=strchr(line,'\r'))){
  //    *eol='\0';
  //}
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

Bool ProcessLogin(char *line){
  /*
   *  Detect our nickname - this is needed when 1) it was given in the 
   *  wrong case and 2) we login as guest where we are assigned a random
   *  guest nickname. 
   */
  char name[30+1];
  char dummy;
  Bool onFICS;
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
  onFICS=FALSE;
  if (!runData.loggedIn &&
      (sscanf(line, "Statistics for %30[^( ]", name) == 1 ||
       (onFICS=   (sscanf(line, "Finger of %30[^:( ]",name)==1)   ))&&
      !strncasecmp(runData.handle, name, strlen(runData.handle))) {
      
      runData.onFICS|=onFICS;
      
      if(runData.onFICS){
	  logme(LOG_INFO,"We seem to be playing on FICS.\n");
      }

      if(!runData.onFICS){ // only FICS has the (undocumented) command "moves l"
	  logme(LOG_INFO,"We seem to be playing on a different server from FICS.\n");
	  if (runData.longAlgMoves) {
	      logme(LOG_WARNING,"Server doesn't support long algebraic move lists. Changing to short mode.\n");
	      runData.longAlgMoves = FALSE;
	  }
      }

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
    StartComputer();
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
        ExecCommandList(appData.sendLogin,0);
    }
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
      //    Feedback(mask,"%s offers a draw.",name);
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
      ExecCommand(tmp,1);
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
      Feedback(CONSOLE|OWNER|SHORTLOG|PROXY,"%s: %s",name,tmp);
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

Bool ProcessIncomingMatches(char *line){
  char name[30+1];
  char name2[30+1];
  char rating[30+1];
  char rating2[30+1];
  char rated[30+1];
  char variant[30+1];
  char color[30+1];
  if(!runData.loggedIn) return FALSE;
  /*
   *  Accept incoming matches
   */
  if ((sscanf(line, "Challenge: %30s (%30[^)])%30s (%30[^)])%30s%30s", 
	      name,rating,name2,rating2,rated,variant) == 6)
      ||
      (sscanf(line,"Challenge: %30s (%30[^)]) [%30[^]]] %30s (%30[^)])%30s%30s",
              name,rating,color,name2,rating2,rated,variant)==7)
  ) {
      // ICC and FICS have different variant names; we need a more generic
      // solution for this...
    if(strcmp(variant,"lightning") && 
       strcmp(variant,"blitz") && 
       strcmp(variant,"standard") &&
       strcmp(variant,"Bullet") && 
       strcmp(variant,"Blitz") && 
       strcmp(variant,"Standard") 
       ){
      logme(LOG_INFO,"Rejected variant %s", variant);
      SendToIcs("tell %s Sorry I only play regular chess.\n",name);
      SendToIcs("decline %s\n", name);    
      return TRUE;
    }
    if(!runData.inTourney && IsNoPlay(name)){
        logme(LOG_DEBUG,"Ignoring %s as he is on our noplay list.",name);
        SendToIcs("decline %s\n",name);
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
      return TRUE;
    }
    if (runData.quitPending) {
      SendToIcs("tell %s Sorry I have to go.\n", name);
      SendToIcs("decline %s\n", name);
      return TRUE;
    }
    if (!runData.inTourney && appData.hardLimit && 
	(runData.numGamesInSeries >= appData.hardLimit) && 
	!strcasecmp(runData.lastPlayer,name)){
      logme(LOG_DEBUG,"Hard limit reached with %s.",name);
      SendToIcs("tell %s You have played me %d times in a row;  I need to take a rest.\n", name, runData.numGamesInSeries);
      SendToIcs("decline %s\n", name);
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
    SendToIcs("accept %s\n", name);
    return TRUE;
  }
  return FALSE;
}

Bool ProcessStandings(char *line){
    int tournamentId;
    static Bool parsingStandings=FALSE;
    static Bool processEndOfTourney=FALSE;
    char c[2];
    if(!runData.loggedIn) return FALSE;
    if(sscanf(line,":mamer TOURNEY #%d UPDATE: The tourney has ended%1[.]",&tournamentId,c)==2){
      processEndOfTourney=TRUE;
      SendToIcs("td standings %d\n",tournamentId);
      return TRUE;
    }
    if(processEndOfTourney &&
       sscanf(line,":Tourney #%d's standings%1[:]",&tournamentId,c)==2){
        parsingStandings=TRUE;
        StartMultiFeedback(CONSOLE|OWNER|SHORTLOG|PROXY);
        logme(LOG_DEBUG,"Detected start of standings of #%d",tournamentId);
        Feedback(CONSOLE|SHORTLOG|PROXY,"Tourney #%d standings:",tournamentId);
        return TRUE;
    }
    if(parsingStandings && (
			    !strncmp(line,":*: Paired.",11) ||
			    !strncmp(line,":Total:",7) 
			    )
       ){
        logme(LOG_DEBUG,"Detected end of standings");
        parsingStandings=FALSE;
        processEndOfTourney=FALSE;
        StopMultiFeedback(CONSOLE|OWNER|SHORTLOG|PROXY);
        return TRUE;
    }
    if(parsingStandings &&
       (
           !strncmp(line,":+-",3) ||
           !strncmp(line,":| ",3) ||
           !strncmp(line,":|-",3)
        )){
        line=strtok(line,"\r\r");
	/* 
	 *  Owner feedback of the standigns looks ugly
	 *  due to the limited possibilities of tells.
	 *  Later we will make the standings more compact.
	 */
        Feedback(CONSOLE|SHORTLOG|PROXY,line);
        return TRUE;
    }
    return FALSE;
 }

Bool ProcessTourneyNotifications(char *line){
  /* 
   * Some of this code will become obsolete once "td GetTourney" works
   * properly.
   */
  int tournamentId;
  Bool endOfTournament=FALSE;
  /* dummies for now */
  char c[2];
  char open[30+1];
  char TM[30+1];
  char type[30+1];
  char date[30+1];
  char color[30+1];
  char name[30+1];


  if(!runData.loggedIn) return FALSE;
  /* join messages */
  if(strstr(line,"You have joined") && 
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
    runData.currentTourney=-1;
    GetTourney();
    /* Should be sendTourneyEnd... */
    if (appData.sendGameEnd){
      ExecCommandList(appData.sendGameEnd,0);
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
  /* Last line of result of "td ListTourneys -j" */
  if(runData.parsingListTourneys && !strncmp(line,":Listed:",8)){
    runData.parsingListTourneys=FALSE;
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
    if(strstr(type,"wild") || strstr(type,"los") || strstr(type,"sui") 
       || strstr(type,"atom") || strstr(type,"zh") || strstr(type,"cra")){
      logme(LOG_DEBUG,"Not joining tourney since it is a nonstandard variant.");
      return TRUE;
    }
    logme(LOG_DEBUG,"Trying to join tourney %d\n",tournamentId);
    SendToIcs("td join %d\n",tournamentId);
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
  char rated[30+1],variant[30+1];
  int time, inc;
  if(!runData.loggedIn) return FALSE;
  /*
   *  Detect start of game, find gametype and rating of opponent
   */
  if (sscanf(line, 
	     "Creating: %30s (%30[^)]) %30s (%30[^)]) %30s %30s %d %d", 
	     name, 
	     rating, 
	     name2, 
	     rating2, 
	     rated, 
	     variant, 
	     &time, 
	     &inc) == 8) {
    logme(LOG_DEBUG, "Detected start of game: %s (%s) vs %s (%s) %s %s %d %d", 
	  name, rating, name2, rating2,rated,variant,time,inc);
    if(appData.sendGameStart){
        ExecCommandList(appData.sendGameStart,0);
    }
    {
        int mask=CONSOLE|SHORTLOG|PROXY;
        if(!appData.ownerQuiet){
            mask|=OWNER;
        }
        if(!runData.inTourney){
            Feedback(mask,"%s %s vs %s %s %s %s %d %d started.", 
                     name, rating, name2, rating2,rated,variant,time,inc);
        }else{
            Feedback(mask,
                     "%s %s vs %s %s %s %s %d %d started (tourney #%d).", 
                     name, rating, name2, rating2,rated,variant,time,inc,
                     runData.currentTourney);
        }
    }
    /* update our state */
    runData.inGame=TRUE;
    /* reset movelist and ask for the moves */
    runData.moveList[0] = '\0';
    SendMarker(ASKSTARTMOVES);
    if (runData.longAlgMoves)
      InternalIcsCommand("moves l\n");
    else
      InternalIcsCommand("moves\n");
    /* send opponent name and ratings to computer */
    if (!strcmp(name, runData.handle)) {
       SendToComputer("name %s\n", name2);
       SendToComputer("rating %d %d\n", atoi(rating), atoi(rating2));
    }
    if (!strcmp(name2, runData.handle)) {
       SendToComputer("name %s\n", name);
       SendToComputer("rating %d %d\n", atoi(rating2), atoi(rating));
    }
    /* update our state */
    runData.waitingForMoveList =TRUE;
    runData.waitingForFirstBoard = TRUE;
    return TRUE;
  }
  /*
   *  Fetch the game ID when a game is started.
   */
  if (!strncmp(line, "{Game ", 6) &&
      strstr(line, runData.handle) &&
      (strstr(line, ") Creating ") || strstr(line, ") Continuing "))) {
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
  p = strtok(line,"\r\n");
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
            HandleBoard(&(runData.icsBoard),runData.moveList);
            runData.parsingMoveList=FALSE;
            runData.waitingForMoveList=FALSE;
            return TRUE;
        }
    }
    return FALSE;
}

Bool ProcessBoard(char *line){
  char *oppname;
  if(!runData.loggedIn) return FALSE;
  if(!ParseBoard(&(runData.icsBoard),line)) return FALSE;
  strncpy(runData.lineBoard, line, sizeof(runData.lineBoard)-1);
  runData.lineBoard[sizeof(runData.lineBoard)-1]='\0';
  if(runData.waitingForFirstBoard){
    runData.waitingForFirstBoard = FALSE;
    if (!strcmp(runData.icsBoard.nameOfWhite, runData.handle)) {
      logme(LOG_INFO, "I'm playing white.");
      runData.computerIsWhite = TRUE;
      runData.calculatedTime=100*runData.icsBoard.whitetime;
      logme(LOG_DEBUG,"calculatedTime=%d",runData.calculatedTime);
      oppname = runData.icsBoard.nameOfBlack;
      
    } else {
      logme(LOG_INFO, "I'm playing black.");
      runData.computerIsWhite = FALSE;
      runData.calculatedTime=100*runData.icsBoard.blacktime;
      logme(LOG_DEBUG,"calculatedTime=%d",runData.calculatedTime);
      oppname = runData.icsBoard.nameOfWhite;
    }
    if (!strcmp(runData.lastPlayer, oppname)) {
      runData.numGamesInSeries++;
    } else {
      runData.numGamesInSeries = 1;
      strcpy(runData.lastPlayer, oppname);
    }
    return TRUE;
  }
  if(runData.waitingForMoveList){
    /* 
     * Avoid prematurely sending moves to the engine. They will be sent as part
     * of the movelist. 
     */
      return TRUE;
  }
  HandleBoard(&(runData.icsBoard),NULL);
  return TRUE;
}

Bool ProcessGameEnd(char *line){
  char resultString[100+1];
  char result[30+1];
  if(!runData.loggedIn) return FALSE;  
  /* 
   *  Check for game-end messages.
   *  We dont care how the game ended, just that it ended.
   */
  memset(resultString,0,sizeof(resultString));
  if(sscanf(line,"{Game %*d %*s vs. %*s%*[ ]%100[^}]} %30s",
	    resultString,
	    result)==2){
    resultString[sizeof(resultString)-1]='\0';
    logme(LOG_DEBUG, "[resultString=%s] [result=%s]",resultString,result);
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
    runData.calculatedTime=0;
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
  /*  static char *lastLine=NULL;*/
  char dummy;
  if(!runData.loggedIn) return FALSE;  
  if(IsMarker(ASKLASTMOVES,line)){
    logme(LOG_DEBUG,"Start of last move list detected.");
    runData.processingLastMoves=TRUE;
    state=0;
    return TRUE;
  }
  if(!runData.processingLastMoves) return FALSE;
  if(sscanf(line,"%*s has no history games%c",&dummy)==1){
    logme(LOG_DEBUG,"No history.");
    runData.processingLastMoves=FALSE;
    return TRUE;
  }
  //  memset(ratingWhite,0,sizeof(ratingWhite));
  // memset(ratingWhite,0,sizeof(ratingBlack));
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
    snprintf(pgnDesc+strlen(pgnDesc), sizeof(pgnDesc)-strlen(pgnDesc), 
	     "%d. %s %s ", moveNumber,move, move2);
    pgnDesc[sizeof(pgnDesc)-1]='\0';  /* paranoia */
    return TRUE;
  }
  if (state==2 && sscanf(line, "%3d. %15s (%*s %15s (%*s", &moveNumber, move, move2) == 2) {
    ConvIcsSanToComp(move);
    snprintf(pgnDesc+strlen(pgnDesc), sizeof(pgnDesc)-strlen(pgnDesc), 
	     "%d. %s ", moveNumber,move);
    pgnDesc[sizeof(pgnDesc)-1]='\0';  /* paranoia */
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
    fprintf(pgnFile,"{%s} %s",resultString,result);
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
        ExecCommandList(appData.sendGameEnd,0);
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



void ProcessIcsLine(char *line){
  char *old_line;
  old_line=strdup(line);
  line=KillPrompts(line);
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
  if(ProcessMoreTime(line))goto finish;
  if(ProcessMoveList(line))goto finish;
  if(ProcessBoard(line))goto finish;
  if(ProcessGameEnd(line))goto finish;
  if(ProcessCreatePGN(line))goto finish;
  if(ProcessCleanUps(line))goto finish;
  if(ProcessMiscFilters(line))goto finish;
  ProcessFeedback(line);

finish:
  
  if(IsMarker(PROXYPROMPT,line)){
      SendToProxy("%s", runData.lastIcsPrompt);
  }else if(IsMarker(STOPFORWARDING,line) && (runData.forwardingMask & PROXY)){
      SendToProxy("%s", runData.lastIcsPrompt);
  }else if(runData.proxyLoginState==PROXY_LOGGED_IN &&
	   !runData.forwarding && 
	    !IsAMarker(line) && 
	    !runData.internalIcsCommand){
      SendToProxy("%s",old_line);
  }

  free(old_line);
}



