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


#include "config.h"

#if defined(__STDC__)
# define P(arg) arg
#else
# define P(arg) ()
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <termios.h>
#include <netdb.h>
#include <arpa/telnet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <setjmp.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <curses.h>
// the following is for cygwin
#ifdef HAVE_TERM_H
#include <term.h>
#else
#ifdef HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#endif
#endif


/*
 * Main data structures
 */

/* For SetOption */

#define CLEAR 1
#define LOGIN 2

/* For setjmp/longjmp */

extern jmp_buf stackPointer;

/* A small string containing various move variants */

#define MS 15
typedef char move_t[MS+1];

/* Mock boolean type */

#undef FALSE
#undef TRUE

typedef enum { FALSE = 0, TRUE = 1 } Bool;

/* Events */

typedef struct event *event_t;

struct event {
  struct timeval time;
  void *data;
  void (*handler)(void *data);
  int has_fired;
};

/* IcsBoard */

typedef struct {
  char board[8][8];       /* [file][rank] */
  char onMove;
  int epFile;
  int whiteCanCastleShort;
  int whiteCanCastleLong;
  int blackCanCastleShort;
  int blackCanCastleLong;
  int movesSinceLastIrreversible;
  int gameNumber;
  char nameOfWhite[18];
  char nameOfBlack[18];
  int status;
  int basetime;
  int inctime;
  int whiteStrength;
  int blackStrength;
  int whitetime;
  int blacktime;
  int nextMoveNum;
  move_t lanMove;
  char lastTime[18];
  move_t sanMove;
  int boardFlipped;  
} IcsBoard;

/* PersistentData */

typedef struct {
  time_t startTime;
  Bool firsttime;
  Bool wasLoggedIn;
  int  games;
} PersistentData;

/* RunData */

typedef struct {
  Bool  computerActive;
  Bool  computerIsWhite;
  int   computerReadFd;
  int   computerWriteFd;
  pid_t computerPid;
  int   icsReadFd;
  int   icsWriteFd;
  int   proxyListenFd;
  int   proxyFd;
  char  handle[30];
  char  passwd[30];
  Bool  quitPending;
  Bool  waitingForFirstBoard;
  int   exitValue;
  int   gameID;
  int   moveNum;
  int   waitingForPingID;
  char* oppname;
  Bool forwarding;
  int forwardingMask;
  Bool loggedIn;
  Bool registered;
  Bool usermove; 
  Bool engineDrawFeature;
  Bool sigint;
  Bool computerReady;
  char lastPlayer[30+1];
  int  numGamesInSeries;
  time_t timeOfLastGame;
  int loginCount;
  char moveList[8192];
  char last_talked_to[30+1];
  IcsBoard icsBoard;
  char lineBoard[512];
  Bool waitingForMoveList;
  Bool promptOnLine;
  int multiFeedbackDepth;
  Bool inExitOn;
  Bool blockConsole;
  Bool inReadline;
  char myname[256];
  char prompt[30+1];
  Bool historyLoaded;
  Bool inhibitPrompt;
  int color;
  Bool longAlgMoves;
  Bool haveCmdResult;
  int calculatedTime;
  long long timeOfLastMove;
  event_t idleTimeoutTimer;
  event_t findChallengeTimer;
  event_t abortTimer;
  event_t flagTimer;
  event_t courtesyAdjournTimer;
  event_t pingTimer;
  char *timestring;
  Bool inGame;
  Bool inTourney;
  int currentTourney;
  int lastGameWasInTourney;
  Bool parsingListTourneys;
  char tournamentOppName[30+1];
  Bool registeredDrawOffer;
  Bool processingTells;
  int  pingCounter;
  pid_t timesealPid;
  time_t lastLoginTime;
#define TELLQUEUESIZE 8192
  char tellQueue[TELLQUEUESIZE];
  event_t tellTimer;
  Bool lastGameWasAbortOrAdjourn;
  Bool onFICS;
  Bool parsingMoveList;
  Bool processingLastMoves;
  int internalIcsCommand;
  char lastIcsPrompt[10];
} RunData;

/* AppData */

typedef struct {
    char* icsHost;
    int   icsPort;
    int   proxyPort;
    char* proxyHost;
    Bool  proxy;
    int   searchDepth;
    int   secPerMove;
    int   logLevel;
    char* logFile;
    char* owner;
    Bool   console;
    Bool   easyMode;
    char*   book;
    char* program;
    char* sendGameEnd;
    char*   sendTimeout;
    int   limitRematches;
    Bool  haveCmdPing;
    char* loginScript;
    Bool  issueRematch;
    char* sendGameStart;
    char* acceptOnly;
    char* timeseal;
    int daemonize;
    char* handle;
    char* passwd;
    Bool logging;
    Bool logAppend;
    Bool feedback;
    Bool feedbackOnlyFinal;
    Bool dontReuseEngine;
    int nice;
    char* pgnFile;
    Bool pgnLogging;
    Bool resign;
    Bool scoreForWhite;
    char* shortLogFile;
    Bool shortLogging;
    Bool autoJoin;
    char* sendLogin;
    char* noplay;
    int colorAlert;
    int colorTell;
    int colorDefault;
    int hardLimit;
    Bool acceptDraw;
    Bool autoReconnect;
    Bool ownerQuiet;
    char * feedbackCommand;
    Bool engineQuiet;
} AppData;

extern PersistentData  persistentData;
extern RunData         runData;
extern AppData         appData;



/* 
 *  EXIT_RESTART and EXIT_HARDQUIT are returned as exit variables.
 *  The others are for internal use only.
 */

#define EXIT_RESTART      1
#define EXIT_SOFTQUIT     2
#define EXIT_HARDQUIT     3
#define EXIT_QUITRESTART  4

/*
 * Scheduling
 */

void create_timer P((event_t *timer,
                     int time, 
                     void (*handler)(void *), 
                     void *data));
void delete_timer P((event_t *event));
struct timeval sched_idle_time P(());
int process_timer_list P(());

/*
 * Argument parsing
 */

typedef enum { ArgNull = 0, ArgString, ArgInt, ArgBool } ArgType;

typedef struct {
    char    *argName;
    ArgType  argType;
    void    *argValue;
    int     runTime;
} ArgTuple;

typedef ArgTuple ArgList[];
extern ArgList argList;


extern int ParseArgs(int argc, char* argv[]);

/*
 * Computer stuff
 */

extern void NewGame P(());
extern void StartComputer P(());
extern void KillComputer P(());
extern void InterruptComputer P(());
extern void EnsureComputerReady P(());
extern void ProcessComputerLine P((char *line)); 
extern void Force P(());
extern void Go P(());
extern void Level P((int,int));
extern void SecondsPerMove P((int));
extern void Result P((char *));
extern void Depth P((int));
extern void Easy P((Bool));
extern void Random P(());
extern void SetInterface P(());
extern void SetScoreForWhite P(());
extern void SendMovesToComputer P((char *));
extern void SendMoveToComputer P((char *));
extern void SendTimeToComputer P((int,int));
extern Bool EvalDraw P((int));

#define RESIGN_SCORE -600
#define RESIGN_SCORE_TIMES 3
#define DRAW_SCORE1 0
#define DRAW_DEPTH1 35
#define DRAW_SCORE2 40
#define DRAW_DEPTH2 60

#define KILLTIMEOUT 1000
#define KILLGRANULARITY 200
#define ENGINEREADYDELAY 3

/*
 * Interaction with FICS
 */

extern void ProcessIcsLine P((char *));
extern void ExecCommand P((char *, int interactive));
extern void ExecFile P((char *, int interactive));
extern void Feedback P((int mask, char *format, ... ));
extern char *myfgets P((char *s, int size, FILE *stream));
extern void strip_nts P((char *s, char *strip));
void CancelTimers P(());
#define PINGINTERVAL 60*1000
#define PINGWINDOW 2
#define RECONNECTINTERVAL 30
#define COURTESYADJOURNINTERVAL 45*1000
#define IDLETIMEOUT 600*1000
#define ABORTTIMEOUT 60*1000
#define FINDCHALLENGETIMEOUT 60*1000

/*
 * Generated by http://www.network-science.de/ascii/
 * using the "big" font.
 *
 */

/*
 * Proxy
 */
extern void ProcessProxyLine P((char *));
extern int StartProxy P((void));
extern void CloseProxy P((void));
extern void SendToProxy P((char *format, ... ));

/*
 * Error handling
 */
 
extern void ExitOn(int exitValue, char *errmsg);

/*
 * Interaction with network
 */

extern int  OpenTCP P((const char *host, int port));
extern void SendToIcs P((char *, ... ));
extern void SendToComputer P((char *, ... ));

/*
 * Useful functions
 */

extern void ConvIcsLanToComp P((char, char *));
extern void ConvIcsSanToComp P((char *));
extern void ConvIcsSpecialToComp P((char, char *));
extern void ConvCompToIcs P((char *));
extern Bool ConvIcsCastlingToLan P((char ,char *));
extern char* ConvertNewlines P((char *));
extern char* PromptInput P((char * buf, int bufsize, char * prompt, int echo));
extern void EchoOn P(());
extern void EchoOff P(());
extern void Daemonize P(());
extern void SetTimeZone P(());
extern void SetColor P(());
extern void ResetColor P(());
extern int  MonthNumber P((char *));
extern void BlockSignals P(());
extern void UnblockSignals P(());
extern int  SetOption P((char *,int,int, char *, ...));
extern struct timeval time_diff P((struct timeval,struct timeval));
extern Bool time_ge P((struct timeval,struct timeval));
extern Bool time_gt P((struct timeval,struct timeval));
extern struct timeval time_add P((struct timeval tv, int t));
extern void my_sleep P((int msec));
extern Bool IsWhiteSpace P((char *s));
/*
 * Logging
 */

typedef enum { LOG_ERROR = 1, LOG_WARNING, LOG_INFO, LOG_CHAT, LOG_DEBUG } LogType;
extern void logme   P((LogType type, const char* format, ...));
extern void logcomm P((char * source, char * dest, char * line));

extern void StartLogging P(());
extern void StopLogging P(());

#define OWNER 0x1
#define CONSOLE 0x2
#define PROXY 0x4
/* 
 *  The shortlog is a compact log of things you might be interested in later.
 *  It is not meant as a debug log. Use the standard logfile for that. 
 */
#define SHORTLOG 0x8

/*
 * Board
 */

Bool ParseBoard P((IcsBoard *icsBoard, char * line));

/*
 * Book
 */

typedef unsigned long long int uint64;
typedef unsigned short uint16;
typedef struct{
  move_t move;
  int score;
} book_move_t;
void random_init P(());
void hash_init P(());
uint64 hash_key P((IcsBoard *icsBoard)); 
void book_close P(());
void book_open P((const char file_name[]));
void book_move P((IcsBoard *icsBoard, book_move_t * bmove, Bool random));

/*
 * Console
 */

void ProcessConsoleLine P((char *));
void Prompt P(());

#ifndef HAVE_LIBREADLINE
#define COLOR_DEFAULT 0
#define COLOR_ALERT 0
#define COLOR_TELL 0
#else
#ifndef __CYGWIN__
#define COLOR_DEFAULT COLOR_MAGENTA
#define COLOR_ALERT COLOR_RED
#define COLOR_TELL COLOR_RED
#else
/*
 * The standard colors are not very bright on the cygwin console */
#define COLOR_DEFAULT COLOR_MAGENTA+8
#define COLOR_ALERT COLOR_RED+8
#define COLOR_TELL COLOR_RED+8
#endif
#endif

void SetFeedbackColor P((int));

/*
 * Markers
 */

extern void SendMarker P((char *));
extern Bool IsMarker P((char *, char*));
extern Bool IsAMarker P((char *));
#define STARTFORWARDING "start"
#define STOPFORWARDING "stop"
#define ENDTELL   "endt"
#define ASKLASTMOVES "askm"
#define DOCLEANUPS "clean"
#define ASKSTARTMOVES "asks"
#define PING "ping"
#define ENDLOGIN "endlogin"
#define PROXYPROMPT "prompt"
#define STARTINTERNAL "startinternal"
#define STOPINTERNAL "stopinternal"

/*
 * Miscellaneous definitions
 */


#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define NETOK 0
#define ERROR -1
#define NOERROR 1

#define BUF_SIZE 8192
#define NETNEWLINE "\r\n"
#define PID_T pid_t




