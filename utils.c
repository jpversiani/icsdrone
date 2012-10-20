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

const char* logtypestr[] = { "", "ERROR", "WARNING", "INFO", "CHAT", "DEBUG" };


int MonthNumber(char *month){
  int i;
  static char * months[]={"jan",
			  "feb", 
			  "mar", 
			  "apr", 
			  "may", 
			  "jun", 
			  "jul", 
			  "aug", 
			  "sep", 
			  "oct", 
			  "nov",
			  "dec"};
  for(i=0;i<=11;i++){
    if(!strncasecmp(month,months[i],3)){
      return i+1;
    }
  }
  return 0;
}

void SetColor(int color){
#ifdef HAVE_LIBREADLINE
  char *p;
  int real_color=color;
  if(color>7)color-=8;
  if(set_a_foreground){
    p=tparm(set_a_foreground,color);
    putp(p);
  }
  if(real_color>7){
    if(enter_bold_mode){
      putp(enter_bold_mode);
    } 
  }
#endif
}

void ResetColor(){
#ifdef HAVE_LIBREADLINE
  if(set_a_foreground && orig_pair){
    putp(orig_pair);
  }
  if(enter_standout_mode && exit_attribute_mode){
    putp(exit_attribute_mode);
  }
#endif
}

void SetTimeZone(){
  int tz;
  struct tm *r;
  time_t t = time(0);
  int dst;
  r=localtime(&t);
  dst=r->tm_isdst;
  tzset();
#ifndef __FreeBSD__
  if(!timezone){
    return;
  }
#endif
  /* Sorry: no timezones with fractions. They don't seem to work, despite
   * what the helpfile says.
   */
  /*  
      if(dst && tzname[1] && tzname[1][0]){
      SendToIcs("set tzone %s\n",tzname[1]); 
      return;
      }
      if(tzname[0] && tzname[0][0]){
      SendToIcs("set tzone %s\n",tzname[0]); 
      return;
      }
  */
#ifdef __FreeBSD__
  tz=r->tm_gmtoff/3600;
#else
  tz=-(timezone/3600);
#endif
  if(dst>0){
      tz++;
  }
  if(tz<0){
    SendToIcs("set tzone -%d\n",-tz); 
    return;
  }    
  if(tz==0){
    SendToIcs("set tzone %d\n",tz); 
    return;
  }
  if(tz>0){
    SendToIcs("set tzone +%d\n",tz); 
    return;
  }
}

void EchoOff(){
   struct termios t;
   tcgetattr( STDIN_FILENO, &t );
   t.c_lflag &= ~ECHO;
   tcsetattr( STDIN_FILENO, TCSANOW, &t );
}


void EchoOn(){
   struct termios t;
   tcgetattr( STDIN_FILENO, &t );
   t.c_lflag |= ECHO;
   tcsetattr( STDIN_FILENO, TCSANOW, &t );
}


char * PromptInput(char* buf, int bufSize, char *prompt, int echo){
        printf("%s",prompt);
        fflush(stdout);
	if(!echo){
        	EchoOff();
	}	
	if(!myfgets(buf,bufSize,stdin)){
	  ExitOn(EXIT_HARDQUIT,"PromptInput failed\n");
	}
        if(!echo){
        	printf("\n");
        }
        EchoOn();
        buf[strlen(buf)-1]='\0';
        return buf;
}


void ConvCompToIcs(move_t move)
{
    if (strlen(move) == 5 && (move[4] == 'q' || move[4] == 'r' || 
			      move[4] == 'b' || move[4] == 'n' ||
			      move[4] == 'Q' || move[4] == 'R' ||
			      move[4] == 'B' || move[4] == 'N' ||
			      move[4] == 'k' || move[4] == 'K')) {
	move[6] = '\0';
	move[5] = move[4];
	move[4] = '=';
    }
}

/* Turn '\' + 'n' into ' ' + '\n'. 
   Result must be freed. */
char * ConvertNewlines(char *s){
        int k;
 	char *p;
	p=strdup(s);
	for (k = 0; k < strlen(p) - 1; k++) {
		if (p[k] == '\\' && p[k+1] == 'n') {
			p[k] = ' ';
			p[k+1] = '\n';
		}
	}
	return p;
}

void StripCR(char *buf){
    char c;
    /* strip out any '\r' */
    char *source=buf, *dest=buf;
    while ((c=(*dest++=*source++))!='\0'){
	if (c == '\r'){
	    dest--;
	}
    }
}

/*  Converts move from ICS LAN to COMPUTER LAN  */
void ConvIcsLanToComp(char onMove, move_t move)
{
    if(ConvIcsCastlingToLan(onMove,move)){
	return;
    }
    /*  frfr=P --> frfrp  */
    if (move[4] == '='){ 
	move[4] = tolower(move[5]);  
        move[5] = '\0';
    }
   
}

/*  Converts move from ICS SAN to COMPUTER SAN  */
void ConvIcsSanToComp(move_t move)
{
  
}

/* Some ICS's (e.g. ICC) do not include promotion suffix in the LAN move 
 * This is a quick hack to fix that.
 */

void ConvIcsTransferPromotionSuffix(char *sanMove,char *lanMove){
    char *s,*ss;
    if((s=strchr(sanMove,'=')) && !(strchr(lanMove,'='))){
	ss=lanMove+7;
	while(*s!='\0'){
	    *(ss++)=*(s++);
	}
	*ss='\0';
    }
}

Bool ConvIcsCastlingToLan(char onMove, move_t move){
    /* work around  bug in (undocumented) ICS LAN generation */
    Bool frc;
    if(!strcasecmp(move,"oo")){
         strcpy(move,"o-o");
    } else if(!strcasecmp(move,"ooo")) {
        strcpy(move,"o-o-o");
    }
    frc=!strcmp(runData.chessVariant,"fischerandom");
    if(!strcasecmp(move,"o-o") && onMove=='W'){
	if(frc){
	    strcpy(move,"O-O");
	}else{
	    strcpy(move,"e1g1");
	}
      return TRUE;
    }
    if(!strcasecmp(move,"o-o-o") && onMove=='W'){
	if(frc){
	    strcpy(move,"O-O-O");
	}else{
	    strcpy(move,"e1c1");
	}
      return TRUE;
    }
    if(!strcasecmp(move,"o-o") && onMove=='B'){
	if(frc){
	    strcpy(move,"O-O");
	}else{
	    strcpy(move,"e8g8");
	}
      return TRUE;
    }
    if(!strcasecmp(move,"o-o-o") && onMove=='B'){
	if(frc){
	    strcpy(move,"O-O-O");
	}else{
	    strcpy(move,"e8c8");
	}
      return TRUE;
    }
    return FALSE;
}

/*  Converts move from ICS SPECIAL to COMPUTER LAN  */
void ConvIcsSpecialToComp(char onMove, move_t move)
{
    if(ConvIcsCastlingToLan(onMove,move)){
	return;
    }
    /* P/@@-fr --> P@fr */
    if(!strncmp(move+1,"/@@",3)){
	move[1]='@';
	move[2]=move[5];
	move[3]=move[6];
	move[4]='\0';
	return;
    }
    /*  P/fr-fr=P --> frfrp  */
    move[0] = move[2];
    move[1] = move[3];
    move[2] = move[5];
    move[3] = move[6];
    if (move[7] == '=') {
	move[4] = tolower(move[8]);
	move[5] = '\0';
    } else {
	move[4] = '\0';
    } 
}

static FILE* logfile = NULL;
#ifdef HAVE_LIBZ
static gzFile gzlogfile = NULL;
#endif

void StartLogging()
{
    if(!appData.logging) return;
    if(appData.logFile && !appData.logAppend){	
   	 printf("Logging to \"%s\".\n",appData.logFile);
    }else if(appData.logFile && appData.logAppend){
   	 printf("Appending to \"%s\".\n",appData.logFile);
    }
    if (appData.logFile) {
	if (!strcmp(&appData.logFile[strlen(appData.logFile) - 3], ".gz")) {
#ifdef HAVE_LIBZ
          if(appData.logAppend){
	    gzlogfile = gzopen(appData.logFile, "ab9");
          }else{
            gzlogfile = gzopen(appData.logFile, "wb9");
	}
#else
	    ExitOn(EXIT_HARDQUIT,"Can't log to .gz as icsDrone wasn't linked with zlib.");
#endif
	} else {
           if(appData.logAppend){
	    logfile = fopen(appData.logFile, "a");
           }else{
	    logfile = fopen(appData.logFile, "w");
           }
	}
    }
#ifdef HAVE_LIBZ
    if(!logfile && !gzlogfile){
#else
    if(!logfile){
#endif
       ExitOn(EXIT_HARDQUIT,"Unable to open logfile.");
    }
}

void StopLogging()
{
    if(!appData.logging) return;
    if (logfile) {
	fclose(logfile);
	logfile=NULL;
#ifdef HAVE_LIBZ
    } else if (gzlogfile) {
	gzclose(gzlogfile);
	gzlogfile=NULL;
#endif
    }
}

 void logcomm(char *source, char *dest, char *line){
    char out[2048] = "";
    char convert[2048]="";
    time_t t = time(0);
    char *timestring = ctime(&t);
    char c;
    char *p, *q, *last_q;
    if(!appData.logging) return;
    if(!line) return;
    p=line;
    q=convert;
    last_q=convert+sizeof(convert)-5 ;
                           /* 5=backslash, 3 octal digits and null */
    while((c=*(p++)) && q<=last_q){
	if(isprint(c)){
	    *(q++)=c;
	}else{
	    *(q++)='\\';
	    snprintf(q,4,"%03o",c);
	    q+=3;
	}
    }
    *q='\0';
    convert[sizeof(convert)-1]='\0';
    timestring[strlen(timestring)-1] = '\0';
    snprintf(out, sizeof(out),"%s:%s:%s->%s: %s\n", 
	     timestring, 
	     logtypestr[LOG_DEBUG],
	     source,
	     dest,
	     convert
	     );
    out[sizeof(out)-1]='\0';
    if (logfile){
	fprintf(logfile, "%s", out);
	fflush(logfile);
    }
#ifdef HAVE_LIBZ
    else if (gzlogfile)
	gzputs(gzlogfile, out);
#endif
 }

void logme(LogType type, const char *format, ... )
{
    char buf[8196] = "";
    char out[2048] = "";
    char *p;
    time_t t = time(0);
    va_list ap;
    char *timestring = ctime(&t);
    int i;
    if(!appData.logging) return;
    /* get rid of the '\n' - ctime() must be on the top ten of worst 
     * standard functions */
    timestring[strlen(timestring)-1] = '\0';
    va_start(ap, format);
    vsnprintf(buf,8196, format, ap);
    va_end(ap);


    /* make sure te buf ends with a \n */
    if (buf[strlen(buf) - 1] != '\n')
	strcat(buf, "\n");


    /* write them out line by line */
    for (p = buf, i = 0; i < strlen(p); ) {


	if (p[i] == '\n') {
	    p[i] = '\0';
	    snprintf(out, 2048,"%s:%s:%s\n", timestring, logtypestr[type], p);
            /* this is ugly but seems to be correct... */
	    p = p+i+1;
	    i = 0;

	    if (logfile){
	      fprintf(logfile, "%s", out);
                fflush(logfile);
            }
#ifdef HAVE_LIBZ
	    else if (gzlogfile)
		gzputs(gzlogfile, out);
#endif

	} else {
	    i++;
	}
    }
}

#define ONEMILLION 1000000
#define ONETHOUSAND 1000

struct timeval time_diff(struct timeval tv1,struct timeval tv2){
    struct timeval ret;
    ret.tv_sec=tv1.tv_sec-tv2.tv_sec;
    ret.tv_usec=tv1.tv_usec-tv2.tv_usec;
    if(ret.tv_usec<0){
        ret.tv_usec+=ONEMILLION;
        ret.tv_sec--;
    }
    return ret;
}

Bool time_ge(struct timeval tv1,struct timeval tv2){
    struct timeval diff=time_diff(tv1,tv2);
    return diff.tv_sec>=0;
}

Bool time_gt(struct timeval tv1,struct timeval tv2){
    struct timeval diff=time_diff(tv2,tv1);
    return diff.tv_sec<0;
}

/*
 * t is in msec
 */

struct timeval time_add(struct timeval tv, int t){
    tv.tv_sec+=t/ONETHOUSAND;
    tv.tv_usec+=(t%ONETHOUSAND)*ONETHOUSAND;
    if(tv.tv_usec>ONEMILLION){
        tv.tv_usec-=ONEMILLION;
        tv.tv_sec++;
    }else if(tv.tv_usec<0){
        tv.tv_usec+=ONEMILLION;
        tv.tv_sec--;
    }
    return tv;
}

/* fgets(), but strip '\r' chars out afterword */
char *myfgets (char *s, int size, FILE *stream)
{
    char *result = fgets(s, size, stream);
    if (result)
        strip_nts(s, "\r");
    return result;
}

/* strip characters out of a null-terminated string */
void strip_nts(char *s, char *strip)
{
    int i, j;

    for (i=j=0; (s[j] = s[i]) != '\0'; i++,j++)
        if (strchr(strip, s[i]))
            j--;
}

void my_sleep(int msec){
    struct timespec tm;
    tm.tv_sec=msec/1000;
    tm.tv_nsec=1000000*(msec%1000);
    nanosleep(&tm,NULL);

}

Bool IsWhiteSpace(char *s){
     int i;
     for(i=0;i<strlen(s);i++){
	 if(!strchr(" \r\n",s[i])){
	     return FALSE;
	 }
     }
     return TRUE;
 }
