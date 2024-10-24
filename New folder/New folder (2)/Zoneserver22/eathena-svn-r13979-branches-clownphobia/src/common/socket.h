// original : core.h 2003/03/14 11:55:25 Rev 1.4

#ifndef	_SOCKET_H_
#define _SOCKET_H_

#include <stdio.h>

#ifdef __WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <time.h>
#include "malloc.h"

extern time_t tick_;
extern time_t stall_time_;

// define declaration

#define RFIFOSPACE(fd) (session[fd]->max_rdata-session[fd]->rdata_size)
#define RFIFOP(fd,pos) (session[fd]->rdata+session[fd]->rdata_pos+(pos))
// use function instead of macro.
#define RFIFOB(fd,pos) (*(unsigned char*)RFIFOP(fd,pos))
#define RFIFOW(fd,pos) (*(unsigned short*)RFIFOP(fd,pos))
#define RFIFOL(fd,pos) (*(unsigned int*)RFIFOP(fd,pos))
#define RFIFOREST(fd)  (session[fd]->rdata_size-session[fd]->rdata_pos)
#define RFIFOFLUSH(fd) (memmove(session[fd]->rdata,RFIFOP(fd,0),RFIFOREST(fd)),session[fd]->rdata_size=RFIFOREST(fd),session[fd]->rdata_pos=0)
//#define RFIFOSKIP(fd,len) ((session[fd]->rdata_size-session[fd]->rdata_pos-(len)<0) ? (fprintf(stderr,"too many skip\n"),exit(1)) : (session[fd]->rdata_pos+=(len)))

#define RBUFP(p,pos) (((unsigned char*)(p))+(pos))
#define RBUFB(p,pos) (*(unsigned char*)RBUFP((p),(pos)))
#define RBUFW(p,pos) (*(unsigned short*)RBUFP((p),(pos)))
#define RBUFL(p,pos) (*(unsigned int*)RBUFP((p),(pos)))

#define WFIFOSPACE(fd) (session[fd]->max_wdata-session[fd]->wdata_size)
#define WFIFOP(fd,pos) (session[fd]->wdata+session[fd]->wdata_size+(pos))
#define WFIFOB(fd,pos) (*(unsigned char*)WFIFOP(fd,pos))
#define WFIFOW(fd,pos) (*(unsigned short*)WFIFOP(fd,pos))
#define WFIFOL(fd,pos) (*(unsigned int*)WFIFOP(fd,pos))
// use function instead of macro.
//#define WFIFOSET(fd,len) (session[fd]->wdata_size = (session[fd]->wdata_size+(len)+2048 < session[fd]->max_wdata) ? session[fd]->wdata_size+len : session[fd]->wdata_size)
#define WBUFP(p,pos) (((unsigned char*)(p))+(pos))
#define WBUFB(p,pos) (*(unsigned char*)WBUFP((p),(pos)))
#define WBUFW(p,pos) (*(unsigned short*)WBUFP((p),(pos)))
#define WBUFL(p,pos) (*(unsigned int*)WBUFP((p),(pos)))

#ifdef __INTERIX
#define FD_SETSIZE 4096
#endif	// __INTERIX


/* Removed Cygwin FD_SETSIZE declarations, now are directly passed on to the compiler through Makefile [Valaris] */

// Struct declaration

struct socket_data{
	int eof;
	unsigned char *rdata,*wdata;
	int max_rdata,max_wdata;
	int rdata_size,wdata_size;
	time_t rdata_tick;
	int rdata_pos;
	struct sockaddr_in client_addr;
	int (*func_recv)(int);
	int (*func_send)(int);
	int (*func_parse)(int);
	int (*func_console)(char*);
	void* session_data;
};

// Data prototype declaration

#ifdef _WIN32

		#undef FD_SETSIZE
		#define FD_SETSIZE 4096

#endif

extern struct socket_data *session[FD_SETSIZE];

extern int fd_max;





/////////////////////////////
// for those still not building c++
#ifndef __cplusplus
//////////////////////////////

// boolean types for C
typedef int bool;
#define false	(1==0)
#define true	(1==1)

//////////////////////////////
#endif // not cplusplus
//////////////////////////////



//////////////////////////////////
// some checking on sockets
extern inline bool session_isValid(int fd)
{
	return ( (fd>=0) && (fd<FD_SETSIZE) && (NULL!=session[fd]) );
}
extern inline bool session_isActive(int fd)
{
	return ( session_isValid(fd) && !session[fd]->eof );
}	
	
//////////////////////////////////










// Function prototype declaration

int make_listen_port(int);
int make_listen_bind(long,int);
int make_connection(long,int);
int delete_session(int);
int realloc_fifo(int fd,int rfifo_size,int wfifo_size);
int WFIFOSET(int fd,int len);
int RFIFOSKIP(int fd,int len);

int do_sendrecv(int next);
int do_parsepacket(void);
void do_socket(void);
void do_final_socket(void);

extern void flush_fifos();
extern void set_nonblocking(int fd, int yes);

int start_console(void);

void set_defaultparse(int (*defaultparse)(int));
void set_defaultconsoleparse(int (*defaultparse)(char*));

int  Net_Init(void);

extern unsigned int addr_[16];   // ip addresses of local host (host byte order)
extern unsigned int naddr_;   // # of ip addresses


#endif	// _SOCKET_H_
