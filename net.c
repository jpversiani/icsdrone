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

int OpenTCP(const char *host, int port)
{
    int fd, b0, b1, b2, b3;
    struct sockaddr_in sa;
    struct hostent *hp;
  
    if (!(hp = gethostbyname(host))) {
	if (sscanf(host, "%d.%d.%d.%d", &b0, &b1, &b2, &b3) == 4) {
            /*
             * This leaks a bit of memory on reconnect.
             * Easy to fix but no time to test.
             * Note that we will come here only in very exceptional circumstances.
             * I.e. when gethostbyname fails and the address was specified in
             * a numeric form. 
             */
	    hp = (struct hostent *) calloc(1, sizeof(struct hostent));
	    hp->h_addrtype = AF_INET;
	    hp->h_length = 4;
	    hp->h_addr_list = (char **) calloc(2, sizeof(char *));
	    hp->h_addr_list[0] = (char *) malloc(4);
	    hp->h_addr_list[0][0] = b0;
	    hp->h_addr_list[0][1] = b1;
	    hp->h_addr_list[0][2] = b2;
	    hp->h_addr_list[0][3] = b3;
	} else {
	    ExitOn(EXIT_QUITRESTART,"Failed to resolve address.");
	}
    }

    /*bytezero(&sa, sizeof(struct sockaddr_in));*/
    memset(&sa,0,sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons((unsigned short) port);
    /*bytecopy(&sa.sin_addr, hp->h_addr, hp->h_length);*/
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) 
	ExitOn(EXIT_HARDQUIT,"Unable to create socket.");
  
    if (connect(fd, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0){ 
	ExitOn(EXIT_QUITRESTART,"Unable to connect to socket.");
    }
    return fd;
}

/* I really should use buffers for writing but what the heck */
#define WRITE(a,b) { if (write(a,b,strlen(b)) == -1) ExitOn(EXIT_QUITRESTART,"Write failed."); }

void SendToIcs(char *format, ... )
{
  char buf[16384] = "";
  va_list ap;
  
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  buf[sizeof(buf)-1]='\0';
  // make sure we end with a newline
  if(strlen(buf)>0 && buf[strlen(buf)-1]!='\n'){
      strcat(buf,"\n");
  }
  logcomm("icsdrone","ics", buf);
  WRITE(runData.icsWriteFd, buf); 
  va_end(ap);
}

void SendToComputer(char *format, ... )
{
    char buf[16384] = "";
    va_list ap;
    
    if (runData.computerActive) {
	va_start(ap, format);
	vsprintf(buf, format, ap);
	logcomm("icsdrone","engine", buf);
	WRITE(runData.computerWriteFd, buf);
	va_end(ap);
    }
}

void SendToConsole(char *format, ... )
{
  char buf[16384] = "";
  va_list ap;
  
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  buf[sizeof(buf)-1]='\0';
  logcomm("icsdrone","console", buf);
  printf("%s",buf);
  va_end(ap);
}


#define TS_NONE 0
#define TS_IAC 1
#define TS_CMD 2
int ProcessRawInput(int fd, char *buf, int sizeofbuf, void (*LineFunc)(char *line, char *queue))
{
    int i, state, num, pos = strlen(buf);
    u_char c;
    char tmp[1024];
    num = read(fd, buf + pos, sizeofbuf - pos - 1);
    if ((num == -1) && ((errno == EWOULDBLOCK) || (errno == EAGAIN))) {
	return NETOK;
    } else if (num == -1) { /* some other error */
       logme(LOG_ERROR,"Error in ProcessRawInput fd=%d: %s",fd,strerror(errno));
	//	ExitOn(EXIT_HARDQUIT,"Unknown error in ProcessRawInput.");
	return ERROR;
    } else if (num == 0) {  /* disconnection */
        logme(LOG_ERROR,"End of file in ProcessRawInput");
	return ERROR;          
    } else if (num > 0) {
	buf[(pos += num)] = '\0';
	state = TS_NONE; i = 0;
	while ((c = buf[i++])) {
	    switch(state) {
	    case TS_NONE:
		if (c == IAC) {
		    state = TS_IAC;
		} else if ((c == '\n') || (c == '\r')) {
		    /* scan the input */
		    /*stringcopy(tmp, buf, i-1);*/
		    int u, nl_seen;
		    strncpy(tmp,buf,i);
		    tmp[i]='\0';
		    u=i; 
		    /* timeseal on Windows generates \r\n\r as eol sequence!
                       We consider any sequence of consecutive \n and \r's containing
                       at most one \n to be an eol sequence. */
		    nl_seen=(c=='\n');
		    while ((!nl_seen && ( buf[i]=='\r' || buf[i]=='\n')) || (nl_seen && buf[i]=='\r')) {
		        if(buf[i]=='\n'){
			  nl_seen=TRUE;
		        }
			if(u<sizeof(tmp)){
			    tmp[u]=buf[i];
			    tmp[u+1]='\0';
			    i++;
			    u++;
			}
		    }
		    LineFunc(tmp,buf+i);
		    /*bytecopy(buf, buf + i, pos+1 - i);*/
		    memmove(buf, buf + i, pos+1 - i);
		    i = 0;
		}
		break;
	    case TS_IAC:
		if (c == WILL || c == WONT || c == DO || c == DONT) {
		    state = TS_CMD;
		} else if (c == IP) {  /* disconnection */
		    return ERROR;   
		} else {               
		    state = TS_NONE;
		    /*bytecopy(buf, buf + i, pos+1 - i);*/
		    memmove(buf, buf + i, pos+1 - i);
		    i = 0;
		}
		break;
	    case TS_CMD:
		/*bytecopy(buf, buf + i, pos+1 - i);*/
		memmove(buf, buf + i, pos+1 - i);
		state = TS_NONE;
		i = 0;
		break;
	    }
	}          
    }
    return NETOK;
}
