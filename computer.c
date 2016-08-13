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
static int depth=0,eval_=0,time_=0,nodes=0;
static int resignScoreSeen=0;
static char pv[256]="";
static Bool forceMode=FALSE;
//static int lastScore=99999;

void Force(){
  SendToComputer("force\n");
  forceMode=TRUE;
  /* What happens if this happens while the computer is thinking? */
}

void Go(){
  /* Avoid sending go several times.
   * This seems to disturb some engines.
   */
    if(forceMode){
	  struct timeval tv;
	  gettimeofday(&tv,NULL);
	  runData.timeOfLastMove=1000000*((long long)tv.tv_sec)+tv.tv_usec;
	  SendToComputer("go\n");
    }
  forceMode=FALSE;
  runData.computerIsThinking=TRUE;
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
  runData.computerIsThinking=FALSE;
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

void SendBoardToComputer(IcsBoard *board, Bool ignoreStartBoard){
      char tmp[256];
	tmp[0]='\0';
	strcat(tmp,"setboard ");
	BoardToFen(tmp+9,&runData.icsBoard);
	if(!strcmp(tmp+9,StartFen) && ignoreStartBoard){
	    logme(LOG_DEBUG,"Do not send the board, since it is the start board for variant normal.");
	    return;
	}
	strcat(tmp,"\n");
	SendToComputer(tmp);
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
  if(!forceMode){
      runData.computerIsThinking=TRUE;
  }
}
				   

void SendTimeToComputer(int whitetime, int blacktime){
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
  runData.computerIsThinking=FALSE;
}

void RawKillComputer(){
    int status;
    int elapsed_time;
    int exited;
    int ret;
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

void KillComputer()
{
    logme(LOG_INFO, "Killing computer");
    if (runData.sigint) InterruptComputer();
    SendToComputer("quit\nexit\n");
    RawKillComputer();
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

static int trackNesting(int c, int nesting){
    static char stack[9];
    if (nesting<9 && c=='(') stack[nesting++]=')';
    if (nesting<9 && c=='[') stack[nesting++]=']';
    if (nesting<9 && c=='{') stack[nesting++]='}';
    if (nesting>0 && c==stack[nesting-1]) nesting--;
    return nesting;
}

#define isMoveChar(c) (isalpha(c) || isdigit(c) || c=='-' || c=='=') // Try to be lax
// Insert move numbers in the PV where they appear missing
char *insertMoveNumbersInPV(char *pv,int plyNr){
  static char buffer[256]="", *s;
  int N=sizeof(buffer), ix=0;
  int lastNr=appData.insertMoveNumbers?-1:99999/*disable*/;
  char space=appData.compactFeedback?"":" ";
  int nesting=0;
  for (s=pv; *s!='\0';s++){
    nesting=trackNesting(*s,nesting);
    if (nesting==0 && isalnum(*s) && (s==pv || !isMoveChar(s[-1]))){ // Word boundary
      if (isalpha(*s)){ // Next word is a move
        if ((lastNr<plyNr) && (lastNr<0 || (plyNr&1)==0)) {
          ix+=snprintf(buffer+ix,N-ix,"%d.%s%s",plyNr>>1,(plyNr&1)?"..":"",space);
          lastNr=plyNr;
        }
        plyNr++;
      } else // Next word is a move number
        lastNr=plyNr;
    }
    if (ix<N-1) buffer[ix++]=*s;
  }
  buffer[ix]='\0';
  return buffer;
}

void PVFeedback(move_t move){
    char buffer[1024];
    int N=sizeof(buffer), ix=0;
    char moveString[15+1], *s;
    int plyNr=runData.nextMoveNum * 2 + !runData.computerIsWhite;

    if(!appData.feedback && !appData.proxyFeedback) return;

    strncpy(moveString, move, sizeof(moveString)-1); // Fallback in case of empty PV

    // Compact feedback: "<score>/<depth> <pv> ... [ {<speed>} ]"
    if (appData.compactFeedback){
      s=insertMoveNumbersInPV(pv, plyNr);
      ix += snprintf(buffer+ix,N-ix,"%+0.2f/%d %s", eval_/100.0,depth,s);
      if (time_ > 0) {
        double Mnps = 100.0*nodes/time_*1e-6;
        ix += snprintf(buffer+ix,N-ix,(Mnps >= 0.995) ? " (%1.1f Mnps)" : " (%1.2f Mnps)", Mnps);
      }
      if(appData.feedback) SendToIcs("%s %s\n",getFeedbackCommand(),buffer);
      if(appData.proxyFeedback) Feedback(PROXY,"\r\n--> icsdrone: %s",buffer);
      return;
    }

    // Prefer to get the first move from pv. But skip anything that doesn't start with a letter
    int nesting=0;
    for (s=pv; *s!='\0'; s++){
      nesting=trackNesting(*s, nesting);
      if (nesting==0 && isalpha(*s) && (s==pv || !isMoveChar(s[-1]))) {
        int n=0;
        sscanf(s, "%15[^ .,]%*[ .,]%n", moveString, &n); // Grab move without punctuation, if any
        s += n; // Advance for remainder of PV (see below)
        break;
      }
    }

    // First line: <moveNumber> <move>, <score>, <depth> [, <speed>, <time> ]
    ix += snprintf(buffer+ix,N-ix,"%d.%s %s", plyNr>>1, (plyNr&1)==0 ? "" : "..", moveString);
    ix += snprintf(buffer+ix,N-ix,", %+0.2f", eval_/100.0);
    ix += snprintf(buffer+ix,N-ix,", %d ply", depth);
    if (time_ > 0) {
      double Mnps = 100.0*nodes/time_*1e-6;
      ix += snprintf(buffer+ix,N-ix,(Mnps >= 0.995) ? ", %1.1f Mnps" : ", %1.2f Mnps", Mnps);
      ix += snprintf(buffer+ix,N-ix,", %0.1f s", time_/100.0);
    }

    if(appData.feedback){
       SendToIcs("%s %s\n",getFeedbackCommand(),buffer);
    }
    if(appData.proxyFeedback){
      Feedback(PROXY,"\r\n--> icsdrone: %s",buffer);
    }

    // Second line: <pv> ...
    if (sscanf(s,"%15s",moveString)==1) { // Is there more after first move?
      s=insertMoveNumbersInPV(s, plyNr+1);
      if(appData.feedback){
	SendToIcs("%s pv %s\n",getFeedbackCommand(),s);
      }
      if(appData.proxyFeedback){
	Feedback(PROXY,"\r\n--> icsdrone: pv %s",s);
      }
    }
}

void ParseEngineVariants(char *line){
    int evc;
    char *line1;
    char *line2;
    char *line_save;
    char *variant;
    char *variant_list;

    evc=0;
    line1=strdup(line);
    line_save=line1;
    line2=strstr(line1,"variants");
    if(line2){
	line1=line2;
    }else{
	return;
    }
    variant_list=strtok(line1," =\"");     /* NO */
    variant_list=strtok(NULL,"\""); /* Again! */
    evc=0;
    variant=strtok(variant_list," ,");
    while(variant){
	if(evc==MAXENGINEVARIANTS){
	    goto finish;
	}
	strncpy(runData.chessVariants[evc++],variant,30);
	logme(LOG_DEBUG,"engine supports variant \"%s\"",variant);
	variant=strtok(NULL," ,");
    }
    runData.chessVariantCount=evc;
	
 finish:
    logme(LOG_DEBUG,"engine supports %d variants",runData.chessVariantCount);
    free(line_save);
}

void ProcessComputerLine(char *line, char *queue) 
{
  move_t move;
  char *tmp;
  /* some dummy variables for sscanf parsing */
  char c;
  int d1,d2;
  char ss[11];

  logcomm("engine","icsdrone", line);
  
  if (sscanf(line, "%*s ... %15s", move) == 1 ||
      sscanf(line, "move %15s", move) == 1) {
    if (runData.gameID != -1) {
      runData.engineMovesPlayed++;
      runData.computerIsThinking=FALSE;
      if((appData.feedback || appData.proxyFeedback) && validKibitzSeen){
	PVFeedback(move);
      }
      if(appData.resign && validKibitzSeen && eval_<RESIGN_SCORE){
	resignScoreSeen++;
        logme(LOG_DEBUG,"Resign score %d < %d seen.",eval_,RESIGN_SCORE);
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
	ConvCompToIcs(move);
	if(appData.acceptDraw && 
	   runData.registeredDrawOffer &&
	   validKibitzSeen &&
	   EvalDraw(eval_)){
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
  }else if(line[0]=='#'){
    /* Comment! Ignore. */
  }else if(!strncasecmp(line,"Illegal move",12) && runData.inGame){
      /* 
       * We have the problem that the illegal move might be from a previous game.
       * It is not completely clear that this is the right test though.
       */
      if(runData.engineMovesPlayed>0){
	BailOut("The engine claims the ICS move is illegal... Bailing out.");
      }else{
	logme(LOG_DEBUG,"Not bailing out since we are at the start of the game.");
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
      logme(LOG_DEBUG,"Selecting short algebraic notation");
      runData.longAlgMoves=FALSE;  /* zippy v2 */
    }
    if (strstr(line, " san=0")) {
      logme(LOG_DEBUG,"Selecting long algebraic notation");
      runData.longAlgMoves=TRUE;  /* zippy v2 */
    }
    if (strstr(line, " usermove=1")) {
      logme(LOG_DEBUG,"Prefixing moves with \"usermove\"");
      runData.usermove=TRUE;  /* zippy v2 */
    }
    if (strstr(line, " draw=0")) {
      logme(LOG_DEBUG,"draw offers will not be passed to engine");
      runData.engineDrawFeature=FALSE;  /* zippy v2 */
    }
    if (strstr(line," variants")){
	ParseEngineVariants(line);
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
      }
    }
  } else if (!strncmp(line,"1/2-1/2",7)) {
          // probably we should send "draw <move>" but this requires cooperation
          // from the engine...
      SendToIcs("draw\n");
      runData.computerIsThinking=FALSE;
  } else if (!strncmp(line,"offer draw",10)) {
      SendToIcs("draw\n");
      runData.computerIsThinking=FALSE;
  } else if (!strncmp(line, "resign",6)) {
      SendToIcs("resign\n");
      runData.computerIsThinking=FALSE;
  } else if (sscanf(line,"%d-%d {%10s resigns%c",&d1,&d2,ss,&c)==4) {
      SendToIcs("resign\n");
      runData.computerIsThinking=FALSE;
  } 
  else if (!appData.engineQuiet && !strncmp(line, "tellics ", 8)) {
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
    int _depth;
    if(sscanf(line,"%d%*[ .+]%d %d %d %254[^\r\n]",
	      &_depth,&eval_,&time_,&nodes,pv)==5){
        depth=_depth; // can be clobbered by false info lines otherwise
        if(appData.scoreForWhite && !runData.computerIsWhite)eval_=-eval_;
      validKibitzSeen=1;
      if(time_>=500 && appData.feedback && !appData.feedbackOnlyFinal){
	PVFeedback(move);
      }
    }
  }
}
