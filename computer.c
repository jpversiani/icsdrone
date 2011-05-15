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

// These globals are really ugly.
// Leave for now as things seem to work.
 
static int validKibitzSeen=0;
static int depth=0,eval=0,time_=0,nodes=0;
static int resignScoreSeen=0;
static char pv[256]="";
static Bool forceMode=FALSE;
//static int lastScore=99999;

void Force(){
  SendToComputer("force\n");
  forceMode=TRUE;
}

void Go(){
  /* Avoid sending go several times.
   * This seems to disturb some engines.
   */
  if(forceMode) SendToComputer("go\n");
  forceMode=FALSE;
}

void Depth(int depth){
  if (depth) {
    /* for gnuchess */
    SendToComputer("depth %d\n", depth);
    /* official xboard protocol */
    SendToComputer("sd %d\n", depth);
  }
}

void Easy(Bool easy){
  if(easy){
     SendToComputer("hard\neasy\n");
  }else{
    SendToComputer("hard\n");
  }
}

void Random(){
  SendToComputer("random\n");
}

void Level(int basetime, int inctime){
   SendToComputer("level 0 %d %d\n", basetime, inctime);
}

void SecondsPerMove(int seconds){
  if(seconds){
    SendToComputer("st %d\n",appData.secPerMove);
  }
}

void Result(char * result){
  SendToComputer("result %s\n", result);
}

void EnsureComputerReady(){
  /**********************************************************
   *
   *  There are some exceptional circumstances where the
   *  engine may fail to initialize in time. In that
   *  case we insert a delay and hope for the best.
   *
   *********************************************************/
  if(!runData.computerReady || runData.waitingForPingID){
    logme(LOG_WARNING,"Engine might not be ready. Delaying.");
    logme(LOG_WARNING,"computerReady=%d",
	  runData.computerReady);
    logme(LOG_WARNING,"waitingForPingID=%d",
	  runData.waitingForPingID);
    sleep(ENGINEREADYDELAY);
    runData.computerReady=TRUE;
    runData.waitingForPingID=FALSE;
  }
}

void SendMovesToComputer(char *moveList){
  char *p;
  move_t move;
  char *m;
  m=moveList;
  while((p=strchr(m,'\n'))){
    if(p-m>MS){
      ExitOn(EXIT_HARDQUIT,"move in movelist too long.");
    }
    strncpy(move,m,p-m);
    move[p-m]='\0';
    SendMoveToComputer(move);
    m=p+1;
  }  
}

void SendMoveToComputer(move_t move){
  struct timeval tv;
  gettimeofday(&tv,NULL);
  runData.timeOfLastMove=1000000*((long long)tv.tv_sec)+tv.tv_usec;
  if(!runData.usermove){
    SendToComputer("%s\n",move);
  }else{
    SendToComputer("usermove %s\n", move);
  }
}
				   

void SendTimeToComputer(int whitetime, int blacktime){
  /* time in centiseconds */
  if(runData.computerIsWhite && (runData.calculatedTime<whitetime)){
    logme(LOG_INFO,"Reducing computer time to %d\n",runData.calculatedTime);
    whitetime=runData.calculatedTime;
  }
  if(!runData.computerIsWhite && (runData.calculatedTime<blacktime)){
    logme(LOG_INFO,"Reducing computer time to %d\n",runData.calculatedTime);
    blacktime=runData.calculatedTime;
  }
     SendToComputer("time %d\notim %d\n", 
		    ((runData.computerIsWhite) ? whitetime : blacktime),
		    ((runData.computerIsWhite) ? blacktime : whitetime));
}

void PingComputer()
{
  static int pingID = 1;
  runData.waitingForPingID = pingID++;
  SendToComputer("ping %d\n", runData.waitingForPingID);
}



void StartComputer()
{
  int fdin[2], fdout[2], argc = 0;
  char buf[1024];
  char *argv[80], *p; /* XXX Buffer overflow alert */
  logme(LOG_DEBUG,"Restarting computer.");
  if(runData.computerActive){
    logme(LOG_DEBUG,"Computer is already active.");
    Force(); /* put engine in a known state */
    if (appData.haveCmdPing){
      PingComputer();
    }
    return;
  }
  runData.computerReady=FALSE;
  if(pipe(fdin) || pipe(fdout)){
    ExitOn(EXIT_HARDQUIT,"Unable to create pipe!\n");
  }
  switch(runData.computerPid = fork()) {
  case -1:
    ExitOn(EXIT_HARDQUIT,"Unable to create new process!\n");
  case 0:
    logme(LOG_DEBUG,"Setting priority of Engine to %d.",appData.nice);
    if(nice(appData.nice)==-1){
      logme(LOG_ERROR,"Unable to set priority of engine");
    }
    close(fileno(stdin));
    if(dup(fdin[0])==-1){
      ExitOn(EXIT_HARDQUIT,"Unable to duplicate fdin!\n");
    }
    close(fileno(stdout));
    if(dup(fdout[1])==-1){
      ExitOn(EXIT_HARDQUIT,"Unable to duplicate fdout!\n");
    }
    close(fdin[0]);
    close(fdin[1]);
    close(fdout[0]);
    close(fdout[1]);
    strcpy(buf, appData.program);
    p = strtok(buf, " ");
    do {
      argv[argc++] = strdup(p);
    } while ((p = strtok(NULL, " ")));
    argv[argc] = NULL;
    if(setsid()<0){
      ExitOn(EXIT_HARDQUIT,"Could not put engine in its own session.");
    }
    dup2(fileno(stdout),STDERR_FILENO); /* Redirect stderr to stdout, like '2>&1' in sh */
    UnblockSignals();
    if(execvp(argv[0], argv)==-1){
      ExitOn(EXIT_HARDQUIT,"Unable to execute chess program!  (have you supplied the correct path/filename?)\n");
    }
    break;
  default:
    close(fdin[0]);
    close(fdout[1]);
    //    printf("computer: closed %d %d\n",fdin[0],fdout[1]);
    //    printf("computer: inuse %d %d\n",fdin[1],fdout[0]);
    runData.computerReadFd = fdout[0];
    runData.computerWriteFd = fdin[1];
    break;
  } 
  logme(LOG_DEBUG,"Declaring computer active");
  runData.computerActive=TRUE;
  SendToComputer("xboard\n");   /* zippy v1 */
  SendToComputer("protover 2\n");  /* zippy v2 */
  SendToComputer("post\n"); /* for feedback */
  SendToComputer("easy\n"); /* to avoid gnuchess using 100% CPU time */
  Force(); /* put engine in a known state */
  if (appData.haveCmdPing)
    PingComputer();
}


void ResetComputer(){
  logme(LOG_DEBUG,"Resetting computer.");
  if (appData.dontReuseEngine) {
    KillComputer();
  } else {
    SendToComputer("\nforce\nnew\neasy\n");
  }
  validKibitzSeen=0;
}

void KillComputer()
{
    int status;
    int elapsed_time;
    int exited;
    int ret;
    logme(LOG_INFO, "Killing computer");
    if (runData.sigint) InterruptComputer();
    SendToComputer("quit\nexit\n");
    elapsed_time=0;
    exited=FALSE;
    ret=0;
    
    while(elapsed_time<KILLTIMEOUT){
	ret=waitpid(runData.computerPid,&status,WNOHANG);
	if(ret==0){
	    logme(LOG_DEBUG,"Computer has not exited yet. Sleeping %dms.\n", KILLGRANULARITY);
	    my_sleep(KILLGRANULARITY);
	    elapsed_time+=KILLGRANULARITY;
	}else{
	    exited=TRUE;
	    break;
	}
    }
    if(!exited){
	logme(LOG_DEBUG,"Computer wouldn't exit by itself. Terminating it.\n");
	kill(runData.computerPid,SIGKILL);
	waitpid(runData.computerPid,&status,0);    
    }

    if(WIFEXITED(status)){
	logme(LOG_DEBUG, "Computer exited with status %d.\n",WEXITSTATUS(status));
    }else{
	logme(LOG_DEBUG, "Computer terminated with signal %d.\n",WTERMSIG(status));
    }
    runData.waitingForPingID = 0;
    runData.computerActive = FALSE;
    close(runData.computerReadFd);
    close(runData.computerWriteFd);
}


void InterruptComputer()
{
  logme(LOG_DEBUG, "Interrupting computer");
  EnsureComputerReady();
  kill(runData.computerPid, SIGINT);
}

void NewGame(){
  ResetComputer();
  StartComputer();
}

Bool EvalDraw(int eval){
  logme(LOG_DEBUG,"Checking if eval=%d amounts to a draw score.",eval);
  if(eval<=DRAW_SCORE1 && runData.icsBoard.nextMoveNum>= DRAW_DEPTH1){
    logme(LOG_DEBUG,
	  "Draw score because eval(%d)<=%d and move(%d)>=%d",
	  eval,
	  DRAW_SCORE1,
	  runData.icsBoard.nextMoveNum,
	  DRAW_DEPTH1);
    return TRUE;
  }
  if(eval<=DRAW_SCORE2 && runData.icsBoard.nextMoveNum>= DRAW_DEPTH2){
    logme(LOG_DEBUG,
	  "Draw score because eval(%d)<=%d and move(%d)>=%d",
	  eval,
	  DRAW_SCORE2,
	  runData.icsBoard.nextMoveNum,
	  DRAW_DEPTH2);
    return TRUE;
  }
  logme(LOG_DEBUG,"No eval=%d is not a draw score.",eval);
  return FALSE;
}

void PVFeedback(){
    if(!appData.feedbackCommand) return;
    SendToIcs("%s depth=%d score=%0.2f time=%0.2f node=%d speed=%d pv=%s\n",
              appData.feedbackCommand,
              depth,
              (eval+0.0)/100,
              (time_+0.0)/100,
              nodes,
              time_?(int)(100*(nodes+0.0)/time_):0,
              pv);
  /* Feedback(CONSOLE,"Analysis: depth=%d score=%0.2f time=%0.2f node=%d speed=%d pv=%s",depth,
     (eval+0.0)/100,
     (time_+0.0)/100,
     nodes,
     time_?(int)(100*(nodes+0.0)/time_):0,
     pv); */
}

void ProcessComputerLine(char *line) 
{
  move_t move;
  char *tmp;
  logme(LOG_DEBUG, "engine->icdrone: %s", line);
  
  if (sscanf(line, "%*s ... %15s", move) == 1 ||
      sscanf(line, "move %15s", move) == 1) {
    if (runData.gameID != -1) {
      if(appData.feedback && validKibitzSeen){
	PVFeedback();
      }
      if(appData.resign && validKibitzSeen && eval<RESIGN_SCORE){
	resignScoreSeen++;
        logme(LOG_DEBUG,"Resign score %d < %d seen.",eval,RESIGN_SCORE);
	logme(LOG_DEBUG,"Resign score seen %d consecutive times.",
	      resignScoreSeen);
      }else{
	resignScoreSeen=0;
      }
      if(appData.resign && resignScoreSeen>=RESIGN_SCORE_TIMES){
	logme(LOG_INFO,
	      "Resigning since we saw the resign score (%d) %d times...",
	      RESIGN_SCORE,
	      RESIGN_SCORE_TIMES);
	resignScoreSeen=0;
	SendToIcs("resign\n");
      }else {
	struct timeval tv;
	gettimeofday(&tv,NULL);
	if(runData.icsBoard.nextMoveNum>1){
	  runData.calculatedTime=runData.calculatedTime+
	                    100*runData.icsBoard.inctime-
      (1000000*((long long)tv.tv_sec)+tv.tv_usec-runData.timeOfLastMove)/10000;
	              
	}
	logme(LOG_DEBUG,"calculatedTime=%d",runData.calculatedTime);
	ConvCompToIcs(move);
	if(appData.acceptDraw && 
	   runData.registeredDrawOffer &&
	   validKibitzSeen &&
	   EvalDraw(eval)){
	   SendToIcs("draw\n");
	}else if(runData.registeredDrawOffer){
	  SendToComputer("draw\n");
	}
	SendToIcs("%s\n", move);
	runData.registeredDrawOffer=FALSE;
	validKibitzSeen=0;
      }
    } else {
      logme(LOG_WARNING, 
	    "Move received from engine but we're not playing anymore!"); 
    }
  } else if (!strncmp(line, "pong ", 5)) {
    if (runData.waitingForPingID == atoi(line + 5)) {
      runData.waitingForPingID = 0;
      logme(LOG_DEBUG, 
	    "Got ping reply from computer matching our ping request.");
    } else {
      logme(LOG_WARNING, "Got unexpected ping reply from computer.");
    }
  } else if (strstr(line,"llegal") && strstr(line,"protover 2")){
    /* xboard 2 is not supported. Let's hope for the best. */
    logme(LOG_WARNING,"xboard 2 protocol is not supported by engine.");
    /* Override what user told us. */
    appData.dontReuseEngine=TRUE; 
    /*runData.computerReady=TRUE; */
  } else if (strstr(line, "feature ")) {
    /* If reuse=1, we acknowledge this and override 
     * whatever the user told us 
     */
    logme(LOG_DEBUG,"Processing feature list");
    if (strstr(line, " reuse=1")) {
      logme(LOG_DEBUG,"Reusing engine for now.");
      appData.dontReuseEngine=FALSE;
    }
    
    if (strstr(line, " sigint=0")) {
      /* This does nothing as it is the default now. */
      logme(LOG_DEBUG,"Setting runData.sigint = FALSE");
      runData.sigint = FALSE; 
    }
    if (strstr(line, " sigint=1")) {
      logme(LOG_DEBUG,"Setting runData.sigint = TRUE");
      runData.sigint = TRUE; 
    }
    /* ping command */
    if (strstr(line, " ping=1")) {
      logme(LOG_DEBUG,"Setting appData.haveCmdPing = TRUE");
      appData.haveCmdPing = TRUE;
    }
    if (strstr(line, " ics=1")) {
      logme(LOG_DEBUG,"Setting icshostname");
      SendToComputer("ics %s\n", appData.icsHost);  /* zippy v2 */
    }
    if (strstr(line, " san=1")) {
      logme(LOG_DEBUG,"Selecting standard notation");
      runData.longAlgMoves=FALSE;  /* zippy v2 */
    }
    if (strstr(line, " usermove=1")) {
      logme(LOG_DEBUG,"Prefixing moves with \"usermove\"");
      runData.usermove=TRUE;  /* zippy v2 */
    }
    if (strstr(line, " draw=0")) {
      logme(LOG_DEBUG,"draw offers will not be passed to engine");
      runData.engineDrawFeature=FALSE;  /* zippy v2 */
    }
    if (strstr(line, " done=1")) {
      logme(LOG_DEBUG,"Computer is ready");
      runData.computerReady=TRUE;  /* zippy v2 */
    }
    if ((tmp=strstr(line,"myname="))){
      if(sscanf(tmp,"myname=\"%254[^\"]",runData.myname)==1){
	logme(LOG_DEBUG,"Engine: %s.",runData.myname);
	SetInterface();
	SetScoreForWhite();
	if(persistentData.firsttime){
	  if(!appData.sendGameStart){
        SetOption("sendGameStart",LOGIN,0,"%s This is %s", appData.feedbackCommand, runData.myname);
	  }
	}
      }
    }
  } else if (strstr(line,"1/2-1/2")) {
          // probably we should send "draw <move>" but this requires cooperation
          // from the engine...
      SendToIcs("draw\n");
  } else if (!strncmp(line,"offer draw",10)) {
      SendToIcs("draw\n");
  } else if (!strncmp(line, "resign",6)) {
    SendToIcs("resign\n");
  } else if (!appData.engineQuiet && !strncmp(line, "tellics ", 8)) {
      SendToIcs("%s\n", line + 8);
  } else if(!appData.engineQuiet && !strncmp(line,"tellothers ",11)) {
    SendToIcs("whisper %s\n",line+11);
  } else if(!appData.engineQuiet && !strncmp(line,"tellall ",8)) {
    SendToIcs("kibitz %s\n",line+8);
  } else if (!appData.engineQuiet && !strncmp(line, "tellicsnoalias ", 15)) {
    SendToIcs("$%s\n", line + 15);
  } else if (!appData.engineQuiet && (!strncmp(line, "whisp", 5) ||
	     !strncmp(line, "kibit", 5) ||
	     !strncmp(line, "say", 3) ||
	     !strncmp(line, "tell", 4))) {
    SendToIcs("%s\n", line);
  } else {
    if(sscanf(line,"%d%*[ .+]%d %d %d %254[^\r\n]",
	      &depth,&eval,&time_,&nodes,pv)==5){
        if(appData.scoreForWhite && !runData.computerIsWhite)eval=-eval;
      validKibitzSeen=1;
      if(time_>=500 && appData.feedback && !appData.feedbackOnlyFinal){
	PVFeedback();
      }
    }
  }
}
