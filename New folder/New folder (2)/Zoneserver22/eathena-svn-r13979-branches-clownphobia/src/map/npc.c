// $Id: npc.c,v 1.5 2004/09/25 05:32:18 MouseJstr Exp $
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "db.h"
#include "timer.h"
#include "nullpo.h"
#include "malloc.h"
#include "map.h"
#include "npc.h"
#include "clif.h"
#include "intif.h"
#include "pc.h"
#include "status.h"
#include "itemdb.h"
#include "script.h"
#include "mob.h"
#include "pet.h"
#include "battle.h"
#include "skill.h"
#include "grfio.h"
#include "showmsg.h"

#ifdef _WIN32
#undef isspace
#define isspace(x)  (x == ' ' || x == '\t')
#endif

struct npc_src_list {
	struct npc_src_list * next;
//	struct npc_src_list * prev; //[Shinomori]
	char name[4];
};

static struct npc_src_list *npc_src_first=NULL;
static struct npc_src_list *npc_src_last=NULL;
static int npc_id=START_NPC_NUM;
static int npc_warp=0;
static int npc_shop=0;
static int npc_script=0;
static int npc_mob=0;
char *current_file = NULL;
int npc_get_new_npc_id(void){ return npc_id++; }

static struct dbt *ev_db;
static struct dbt *npcname_db;

struct event_data {
	struct npc_data *nd;
	int pos;
};
static struct tm ev_tm_b;	// 時計イベント用

static int npc_walktimer(int,unsigned int,int,int); // [Valaris]
static int npc_walktoxy_sub(struct npc_data *nd); // [Valaris]

static void npc_data_final(struct npc_data *nd);

/*==========================================
 * NPCの無効化/有効化
 * npc_enable
 * npc_enable_sub 有効時にOnTouchイベントを実行
 *------------------------------------------
 */
int npc_enable_sub( struct block_list *bl, va_list ap )
{
	struct map_session_data *sd;
	struct npc_data *nd;
	//char *name=(char *)aCallocA(50,sizeof(char)); // fixed [Shinomori]

	nullpo_retr(0, bl);
	nullpo_retr(0, ap);
	nullpo_retr(0, nd=va_arg(ap,struct npc_data *));
	if(bl->type == BL_PC && (sd=(struct map_session_data *)bl)){
		char name[50]; // need 24 + 9 for the "::OnTouch"

		if (nd->flag&1)	// 無効化されている
			return 1;

		if(sd->areanpc_id==nd->bl.id)
			return 1;
		sd->areanpc_id=nd->bl.id;

		sprintf(name,"%s::OnTouch", nd->name);
		npc_event(sd,name,0);
	}
	//aFree(name);
	return 0;
}
int npc_enable(const char *name,int flag)
{
	struct npc_data *nd= (struct npc_data *) strdb_search(npcname_db,name);
	if (nd==NULL)
		return 0;

	if (flag&1) {	// 有効化
		nd->flag&=~1;
		clif_spawnnpc(nd);
	}else if (flag&2){
		nd->flag&=~1;
		nd->option = 0x0000;
		clif_changeoption(&nd->bl);
	}else if (flag&4){
		nd->flag|=1;
		nd->option = 0x0002;
		clif_changeoption(&nd->bl);
	}else{		// 無効化
		nd->flag|=1;
		clif_clearchar(&nd->bl,0);
	}
	if(flag&3 && (nd->u.scr.xs > 0 || nd->u.scr.ys >0))
		map_foreachinarea( npc_enable_sub,nd->bl.m,nd->bl.x-nd->u.scr.xs,nd->bl.y-nd->u.scr.ys,nd->bl.x+nd->u.scr.xs,nd->bl.y+nd->u.scr.ys,BL_PC,nd);

	return 0;
}

/*==========================================
 * NPCを名前で探す
 *------------------------------------------
 */
struct npc_data* npc_name2id(const char *name)
{
	return (struct npc_data *) strdb_search(npcname_db,name);
}

void ev_release(struct dbn *db, int which)
{
    if (which & 0x1)
        aFree(db->key);
    if (which & 0x2)
        aFree(db->data);
}

/*==========================================
 * イベントキューのイベント処理
 *------------------------------------------
 */
int npc_event_dequeue(struct map_session_data *sd)
{
	nullpo_retr(0, sd);

	sd->npc_id=0;
	if (sd->eventqueue[0][0]) {	// キューのイベント処理
		size_t ev;

		// find an empty place in eventtimer list
		for(ev=0;ev<MAX_EVENTTIMER;ev++)
			if( sd->eventtimer[ev]==-1 )
				break;
		if(ev<MAX_EVENTTIMER)
		{	// generate and insert the timer
			int i;
			// copy the first event name
			char *name=(char *)aMalloc(50*sizeof(char));
			memcpy(name,sd->eventqueue[0],50);
			// shift queued events down by one
			for(i=1;i<MAX_EVENTQUEUE;i++)
				memcpy(sd->eventqueue[i-1],sd->eventqueue[i],50);
			// clear the last event
			sd->eventqueue[MAX_EVENTQUEUE-1][0]=0; 
			// add the timer
			sd->eventtimer[ev]=add_timer(gettick()+100,pc_eventtimer,sd->bl.id,(int)name);//!!todo!!

		}else
			printf("npc_event_dequeue: event timer is full !\n");
	}
	return 0;
}

/*==========================================
 * イベントの遅延実行
 *------------------------------------------
 */
int npc_event_timer(int tid,unsigned int tick,int id,int data)
{
	char *eventname = (char *)data;
	struct event_data *ev = (struct event_data *)strdb_search(ev_db,eventname);
	struct npc_data *nd;
	struct map_session_data *sd=map_id2sd(id);
	size_t i;

	if((ev==NULL || (nd=ev->nd)==NULL))
	{
		if(battle_config.error_log)
			printf("npc_event: event not found [%s]\n",eventname);
	}	
	else
	{
		for(i=0;i<MAX_EVENTTIMER;i++) {
			if( nd->eventtimer[i]==tid ) {
				nd->eventtimer[i]=-1;
				npc_event(sd,eventname,0); // sd NULL check is within
				break;
			}
		}
		if(i==MAX_EVENTTIMER && battle_config.error_log)
			printf("npc_event_timer: event timer not found [%s]!\n",eventname);
	}

	aFree(eventname);
	return 0;
}

int npc_timer_event(const char *eventname)	// Added by RoVeRT
{
	struct event_data *ev=(struct event_data *) strdb_search(ev_db,eventname);
	struct npc_data *nd;
//	int xs,ys;

	if((ev==NULL || (nd=ev->nd)==NULL)){
		printf("npc_event: event not found [%s]\n",eventname);
		return 0;
	}

	run_script(nd->u.scr.script,ev->pos,nd->bl.id,nd->bl.id);

	return 0;
}
/*
int npc_timer_sub_sub(void *key,void *data,va_list ap)	// Added by RoVeRT
{
	char *p=(char *)key;
	struct event_data *ev=(struct event_data *)data;
	int *c=va_arg(ap,int *);
	int tick=0,ctick=gettick();
	char temp[10];
	char event[100];

	if(ev->nd->bl.id==(int)*c && (p=strchr(p,':')) && p && strncasecmp("::OnTimer",p,8)==0 ){
		sscanf(&p[9],"%s",temp);
		tick=atoi(temp);

		strcpy( event, ev->nd->name);
		strcat( event, p);

		if (ctick >= ev->nd->lastaction && ctick - ev->nd->timer >= tick) {
			npc_timer_event(event);
			ev->nd->lastaction = ctick;
		}
	}
	return 0;
}

int npc_timer_sub(void *key,void *data,va_list ap)	// Added by RoVeRT
{
	struct npc_data *nd=(struct npc_data*)data;

	if(nd->timer == -1)
		return 0;

	strdb_foreach(ev_db,npc_timer_sub_sub,&nd->bl.id);

	return 0;
}

int npc_timer(int tid,unsigned int tick,int id,int data)	// Added by RoVeRT
{
	strdb_foreach(npcname_db,npc_timer_sub);

	aFree((void*)data);
	return 0;
}*/
/*==========================================
 * イベント用ラベルのエクスポート
 * npc_parse_script->strdb_foreachから呼ばれる
 *------------------------------------------
 */
int npc_event_export(void *key,void *data,va_list ap)
{
	char *lname=(char *)key;
	int pos=(int)data;
	struct npc_data *nd=va_arg(ap,struct npc_data *);

	if ((lname[0]=='O' || lname[0]=='o')&&(lname[1]=='N' || lname[1]=='n')) {
		struct event_data *ev;
		char *buf;
		char *p=strchr(lname,':');
		// エクスポートされる
		ev=(struct event_data *) aCalloc(sizeof(struct event_data), 1);
		buf=(char *) aCallocA(50, 1);
		if (ev==NULL || buf==NULL) {
			printf("npc_event_export: out of memory !\n");
			exit(1);
		}else if (p==NULL || (p-lname)>24) {
			printf("npc_event_export: label name error !\n");
			exit(1);
		}else{
			ev->nd=nd;
			ev->pos=pos;
			*p='\0';
			sprintf(buf,"%s::%s",nd->exname,lname);
			*p=':';
			strdb_insert(ev_db,buf,ev);
//			if (battle_config.etc_log)
//				printf("npc_event_export: export [%s]\n",buf);
		}
	}
	return 0;
}

/*==========================================
 * 全てのNPCのOn*イベント実行
 *------------------------------------------
 */
int npc_event_doall_sub(void *key,void *data,va_list ap)
{
	char *p=(char *)key;
	struct event_data *ev;
	int *c;
	int rid;
	const char *name;

	nullpo_retr(0, ev=(struct event_data *)data);
	nullpo_retr(0, ap);
	nullpo_retr(0, c=va_arg(ap,int *));
	name=va_arg(ap,const char *);
	rid=va_arg(ap, int);

	if( (p=strchr(p,':')) && p && strcmpi(name,p)==0 ){
		run_script(ev->nd->u.scr.script,ev->pos,rid,ev->nd->bl.id);
		(*c)++;
	}

	return 0;
}
int npc_event_doall(const char *name)
{
	int c=0;
	char buf[64]="::";

	strncpy(buf+2,name,62);
	strdb_foreach(ev_db,npc_event_doall_sub,&c,buf,0);
	return c;
}
int npc_event_doall_id(const char *name, int rid)
{
	int c=0;
	char buf[64]="::";

	strncpy(buf+2,name,62);
	strdb_foreach(ev_db,npc_event_doall_sub,&c,buf,rid);
	return c;
}

int npc_event_do_sub(void *key,void *data,va_list ap)
{
	char *p=(char *)key;
	struct event_data *ev;
	int *c;
	const char *name;

	nullpo_retr(0, ev=(struct event_data *)data);
	nullpo_retr(0, ap);
	nullpo_retr(0, c=va_arg(ap,int *));

	name=va_arg(ap,const char *);

	if (p && strcmpi(name,p)==0 ) {
		run_script(ev->nd->u.scr.script,ev->pos,0,ev->nd->bl.id);
		(*c)++;
	}

	return 0;
}
int npc_event_do(const char *name)
{
	int c=0;

	if (*name==':' && name[1]==':') {
		return npc_event_doall(name+2);
	}

	strdb_foreach(ev_db,npc_event_do_sub,&c,name);
	return c;
}

/*==========================================
 * 時計イベント実行
 *------------------------------------------
 */
int npc_event_do_clock(int tid,unsigned int tick,int id,int data)
{
	time_t timer;
	struct tm *t;
	char buf[64];
        char *day="";
	int c=0;

	time(&timer);
	t=localtime(&timer);

        switch (t->tm_wday) {
	case 0: day = "Sun"; break;
	case 1: day = "Mon"; break;
	case 2: day = "Tue"; break;
	case 3: day = "Wed"; break;
	case 4: day = "Thu"; break;
	case 5: day = "Fri"; break;
	case 6: day = "Sat"; break;
	}

	if (t->tm_min != ev_tm_b.tm_min ) {
		sprintf(buf,"OnMinute%02d",t->tm_min);
		c+=npc_event_doall(buf);
		sprintf(buf,"OnClock%02d%02d",t->tm_hour,t->tm_min);
		c+=npc_event_doall(buf);
		sprintf(buf,"On%s%02d%02d",day,t->tm_hour,t->tm_min);
		c+=npc_event_doall(buf);
	}
	if (t->tm_hour!= ev_tm_b.tm_hour) {
		sprintf(buf,"OnHour%02d",t->tm_hour);
		c+=npc_event_doall(buf);
	}
	if (t->tm_mday!= ev_tm_b.tm_mday) {
		sprintf(buf,"OnDay%02d%02d",t->tm_mon+1,t->tm_mday);
		c+=npc_event_doall(buf);
	}
	memcpy(&ev_tm_b,t,sizeof(ev_tm_b));
	return c;
}
/*==========================================
 * OnInitイベント実行(&時計イベント開始)
 *------------------------------------------
 */
int npc_event_do_oninit(void)
{
//	int c = npc_event_doall("OnInit");
	sprintf(tmp_output,"Event '"CL_WHITE"OnInit"CL_RESET"' executed with '"
	CL_WHITE"%d"CL_RESET"' NPCs.\n",npc_event_doall("OnInit"));
	ShowStatus(tmp_output);

	add_timer_interval(gettick()+100,
		npc_event_do_clock,0,0,1000);

	return 0;
}
/*==========================================
 * OnTimer NPC event - by RoVeRT
 *------------------------------------------
 */
int npc_addeventtimer(struct npc_data *nd,int tick,const char *name)
{
	int i;
	for(i=0;i<MAX_EVENTTIMER;i++)
		if( nd->eventtimer[i]==-1 )
			break;
	if(i<MAX_EVENTTIMER){
		char *evname=(char *) aMallocA(24);
		if(evname==NULL){
			printf("npc_addeventtimer: out of memory !\n");
			exit(1);
		}
		memcpy(evname,name,24);
		nd->eventtimer[i]=add_timer(gettick()+tick,
			npc_event_timer,nd->bl.id,(int)evname);
	}else
		printf("npc_addtimer: event timer is full !\n");

	return 0;
}

int npc_deleventtimer(struct npc_data *nd,const char *name)
{
	int i;
	for(i=0;i<MAX_EVENTTIMER;i++)
		if( nd->eventtimer[i]!=-1 && strcmp(
			(char *)(get_timer(nd->eventtimer[i])->data), name)==0 ){
				delete_timer(nd->eventtimer[i],npc_event_timer);
				nd->eventtimer[i]=-1;
				break;
		}

	return 0;
}

int npc_cleareventtimer(struct npc_data *nd)
{
	int i;
	for(i=0;i<MAX_EVENTTIMER;i++)
		if( nd->eventtimer[i]!=-1 ){
			delete_timer(nd->eventtimer[i],npc_event_timer);
			nd->eventtimer[i]=-1;
		}

	return 0;
}

int npc_do_ontimer_sub(void *key,void *data,va_list ap)
{
	char *p = (char *)key;
	struct event_data *ev = (struct event_data *)data;
	int *c = va_arg(ap,int *);
//	struct map_session_data *sd=va_arg(ap,struct map_session_data *);
	int option = va_arg(ap,int);
	int tick = 0;
	char temp[10];
	char event[50];

	if(ev->nd->bl.id == (int)*c && (p = strchr(p,':')) && strnicmp("::OnTimer",p,8) == 0){
		sscanf(&p[9], "%s", temp);
		tick = atoi(temp);

		strcpy(event, ev->nd->name);
		strcat(event, p);

		if (option!=0) {
			npc_addeventtimer(ev->nd, tick, event);
		} else {
			npc_deleventtimer(ev->nd, event);
		}
	}
	return 0;
}
int npc_do_ontimer(int npc_id, int option)
{
	strdb_foreach(ev_db, npc_do_ontimer_sub, &npc_id, option);
	return 0;
}
/*==========================================
 * タイマーイベント用ラベルの取り込み
 * npc_parse_script->strdb_foreachから呼ばれる
 *------------------------------------------
 */
int npc_timerevent_import(void *key,void *data,va_list ap)
{
	char *lname=(char *)key;
	int pos=(int)data;
	struct npc_data *nd=va_arg(ap,struct npc_data *);
	int t=0,i=0;

	if(sscanf(lname,"OnTimer%d%n",&t,&i)==1 && lname[i]==':') {
		// タイマーイベント
		struct npc_timerevent_list *te=nd->u.scr.timer_event;
		int j,i=nd->u.scr.timeramount;
		if(te==NULL) te=(struct npc_timerevent_list*)aMallocA(sizeof(struct npc_timerevent_list));
		else te= (struct npc_timerevent_list*)aRealloc( te, sizeof(struct npc_timerevent_list) * (i+1) );
		if(te==NULL){
			printf("npc_timerevent_import: out of memory !\n");
			exit(1);
		}
		for(j=0;j<i;j++){
			if(te[j].timer>t){
				memmove(te+j+1,te+j,sizeof(struct npc_timerevent_list)*(i-j));
				break;
			}
		}
		te[j].timer=t;
		te[j].pos=pos;
		nd->u.scr.timer_event=te;
		nd->u.scr.timeramount=i+1;
	}
	return 0;
}
/*==========================================
 * タイマーイベント実行
 *------------------------------------------
 */
int npc_timerevent(int tid,unsigned int tick,int id,int data)
{
	int next,t;
	struct npc_data* nd=(struct npc_data *)map_id2bl(id);
	struct npc_timerevent_list *te;
	if( nd==NULL || nd->u.scr.nexttimer<0 ){
		printf("npc_timerevent: ??\n");
		return 0;
	}
	nd->u.scr.timertick=tick;
	te=nd->u.scr.timer_event+ nd->u.scr.nexttimer;
	nd->u.scr.timerid = -1;

	t = nd->u.scr.timer+=data;
	nd->u.scr.nexttimer++;
	if( nd->u.scr.timeramount>nd->u.scr.nexttimer ){
		next= nd->u.scr.timer_event[ nd->u.scr.nexttimer ].timer - t;
		nd->u.scr.timerid = add_timer(tick+next,npc_timerevent,id,next);
	}

	run_script(nd->u.scr.script,te->pos,nd->u.scr.rid,nd->bl.id);
	return 0;
}
/*==========================================
 * タイマーイベント開始
 *------------------------------------------
 */
int npc_timerevent_start(struct npc_data *nd, int rid)
{
	int j,n, next;

	nullpo_retr(0, nd);

	n=nd->u.scr.timeramount;
	if( nd->u.scr.nexttimer>=0 || n==0 )
		return 0;

	for(j=0;j<n;j++){
		if( nd->u.scr.timer_event[j].timer > nd->u.scr.timer )
			break;
	}
	if(j>=n) // check if there is a timer to use !!BEFORE!! you write stuff to the structures [Shinomori]
		return 0;

	nd->u.scr.nexttimer=j;
	nd->u.scr.timertick=gettick();
	if (rid >= 0) nd->u.scr.rid=rid;	// changed to: attaching to given rid by default [Shinomori]
	// if rid is less than 0 leave it unchanged [celest]

	next = nd->u.scr.timer_event[j].timer - nd->u.scr.timer;
	nd->u.scr.timerid = add_timer(gettick()+next,npc_timerevent,nd->bl.id,next);
	return 0;
}
/*==========================================
 * タイマーイベント終了
 *------------------------------------------
 */
int npc_timerevent_stop(struct npc_data *nd)
{
	nullpo_retr(0, nd);

	if( nd->u.scr.nexttimer>=0 ){
		nd->u.scr.nexttimer = -1;
		nd->u.scr.timer += (int)(gettick() - nd->u.scr.timertick);
		if(nd->u.scr.timerid!=-1)
			delete_timer(nd->u.scr.timerid,npc_timerevent);
		nd->u.scr.timerid = -1;
		nd->u.scr.rid = 0;
	}
	return 0;
}
/*==========================================
 * タイマー値の所得
 *------------------------------------------
 */
int npc_gettimerevent_tick(struct npc_data *nd)
{
	int tick;

	nullpo_retr(0, nd);

	tick=nd->u.scr.timer;

	if( nd->u.scr.nexttimer>=0 )
		tick += (int)(gettick() - nd->u.scr.timertick);
	return tick;
}
/*==========================================
 * タイマー値の設定
 *------------------------------------------
 */
int npc_settimerevent_tick(struct npc_data *nd,int newtimer)
{
	int flag;

	nullpo_retr(0, nd);

	flag= nd->u.scr.nexttimer;

	npc_timerevent_stop(nd);
	nd->u.scr.timer=newtimer;
	if(flag>=0)
		npc_timerevent_start(nd, -1);
	return 0;
}

/*==========================================
 * イベント型のNPC処理
 *------------------------------------------
 */
int npc_event(struct map_session_data *sd,const char *eventname,int mob_kill)
{
	struct event_data *ev=(struct event_data *) strdb_search(ev_db,eventname);
	struct npc_data *nd;
	int xs,ys;
	char mobevent[100];

	if( sd == NULL ){
		printf("npc_event nullpo?\n");
	}

	if(ev==NULL && eventname && strcmp(((eventname)+strlen(eventname)-9),"::OnTouch") == 0)
	return 1;

	if(ev==NULL || (nd=ev->nd)==NULL){
		if(mob_kill && (ev==NULL || (nd=ev->nd)==NULL)){
			strcpy( mobevent, eventname);
			strcat( mobevent, "::OnMyMobDead");
			ev= (struct event_data *) strdb_search(ev_db,mobevent);
	if (ev==NULL || (nd=ev->nd)==NULL) {
				if (strnicmp(eventname,"GM_MONSTER",10)!=0)
					printf("npc_event: event not found [%s]\n",mobevent);
				return 0;
			}
		}
		else {
			if(battle_config.error_log)
				printf("npc_event: event not found [%s]\n",eventname);
			return 0;
		}
	}

	xs=nd->u.scr.xs;
	ys=nd->u.scr.ys;
	if (xs>=0 && ys>=0 ) {
		if (nd->bl.m != sd->bl.m )
			return 1;
		if ( xs>0 && (sd->bl.x<nd->bl.x-xs/2 || nd->bl.x+xs/2<sd->bl.x) )
			return 1;
		if ( ys>0 && (sd->bl.y<nd->bl.y-ys/2 || nd->bl.y+ys/2<sd->bl.y) )
			return 1;
	}

	if ( sd->npc_id!=0) {
//		if (battle_config.error_log)
//			printf("npc_event: npc_id != 0\n");
		int i;
		for(i=0;i<MAX_EVENTQUEUE;i++)
			if (!sd->eventqueue[i][0])
				break;
		if (i==MAX_EVENTQUEUE) {
			if (battle_config.error_log)
				printf("npc_event: event queue is full !\n");
		}else{
//			if (battle_config.etc_log)
//				printf("npc_event: enqueue\n");
			memcpy(sd->eventqueue[i],eventname,50);
		}
		return 1;
	}
	if (nd->flag&1) {	// 無効化されている
		npc_event_dequeue(sd);
		return 0;
	}

	sd->npc_id=nd->bl.id;
	sd->npc_pos=run_script(nd->u.scr.script,ev->pos,sd->bl.id,nd->bl.id);
	return 0;
}


int npc_command_sub(void *key,void *data,va_list ap)
{
	char *p=(char *)key;
	struct event_data *ev=(struct event_data *)data;
	char *npcname=va_arg(ap,char *);
	char *command=va_arg(ap,char *);
	char temp[100];

	if(strcmp(ev->nd->name,npcname)==0 && (p=strchr(p,':')) && p && strnicmp("::OnCommand",p,10)==0 ){
		sscanf(&p[11],"%s",temp);

		if (strcmp(command,temp)==0)
			run_script(ev->nd->u.scr.script,ev->pos,0,ev->nd->bl.id);
	}

	return 0;
}

int npc_command(struct map_session_data *sd,char *npcname,char *command)
{
	strdb_foreach(ev_db,npc_command_sub,npcname,command);

	return 0;
}
/*==========================================
 * 接触型のNPC処理
 *------------------------------------------
 */
int npc_touch_areanpc(struct map_session_data *sd,int m,int x,int y)
{
	int i,f=1;
	int xs,ys;

	nullpo_retr(1, sd);

	if(sd->npc_id)
		return 1;

	for(i=0;i<map[m].npc_num;i++) {
		if (map[m].npc[i]->flag&1) {	// 無効化されている
			f=0;
			continue;
		}

		switch(map[m].npc[i]->bl.subtype) {
		case WARP:
			xs=map[m].npc[i]->u.warp.xs;
			ys=map[m].npc[i]->u.warp.ys;
			break;
		case SCRIPT:
			xs=map[m].npc[i]->u.scr.xs;
			ys=map[m].npc[i]->u.scr.ys;
			break;
		default:
			continue;
		}
		if (x >= map[m].npc[i]->bl.x-xs/2 && x < map[m].npc[i]->bl.x-xs/2+xs &&
		   y >= map[m].npc[i]->bl.y-ys/2 && y < map[m].npc[i]->bl.y-ys/2+ys)
			break;
	}
	if (i==map[m].npc_num) {
		if (f) {
			if (battle_config.error_log)
				printf("npc_touch_areanpc : some bug \n");
		}
		return 1;
	}
	switch(map[m].npc[i]->bl.subtype) {
		case WARP:
			if (sd->status.option&6)	// hidden chars cannot use warps -- is it the same for scripts too?
				break;
			skill_stop_dancing(&sd->bl,0);
			pc_setpos(sd,map[m].npc[i]->u.warp.name,map[m].npc[i]->u.warp.x,map[m].npc[i]->u.warp.y,0);
			break;
		case SCRIPT:
		{
			//char *name=(char *)aCallocA(50,sizeof(char));  // fixed [Shinomori]
			char name[50]; // need 24 max + 9 for "::OnTouch"

			if(sd->areanpc_id == map[m].npc[i]->bl.id)
				return 1;
			sd->areanpc_id = map[m].npc[i]->bl.id;

			sprintf(name,"%s::OnTouch", map[m].npc[i]->name);

			if( npc_event(sd,name,0)>0 )
				npc_click(sd,map[m].npc[i]->bl.id);
			//aFree(name);
			break;
		}
	}
	return 0;
}

/*==========================================
 * 近くかどうかの判定
 *------------------------------------------
 */
int npc_checknear(struct map_session_data *sd,int id)
{
	struct npc_data *nd;

	nullpo_retr(0, sd);

	nd=(struct npc_data *)map_id2bl(id);
	if (nd==NULL || nd->bl.type!=BL_NPC) {
		if (battle_config.error_log)
			printf("no such npc : %d\n",id);
		return 1;
	}

	if (nd->class_<0)	// イベント系は常にOK
		return 0;

	// エリア判定
	if (nd->bl.m!=sd->bl.m ||
	   nd->bl.x<sd->bl.x-AREA_SIZE-1 || nd->bl.x>sd->bl.x+AREA_SIZE+1 ||
	   nd->bl.y<sd->bl.y-AREA_SIZE-1 || nd->bl.y>sd->bl.y+AREA_SIZE+1)
		return 1;

	return 0;
}

/*==========================================
 * NPCのオープンチャット発言
 *------------------------------------------
 */
int npc_globalmessage(const char *name,char *mes)
{
	struct npc_data *nd=(struct npc_data *) strdb_search(npcname_db,name);
	char temp[100];
	char ntemp[50];
	char *ltemp;

	if(nd==NULL) return 0;
	if(name==NULL) return 0;

	ltemp=strchr(name,'#');
	if(ltemp!=NULL) {
		strncpy(ntemp,name,ltemp - name);	// 123#456 の # から後ろを削除する
		ntemp[ltemp - name]=0x00;	// strncpy のバグ？使い方間違ってる？
	}

	snprintf(temp, sizeof temp ,"%s : %s",ntemp,mes);
	clif_GlobalMessage(&nd->bl,temp);

	return 0;
}

/*==========================================
 * クリック時のNPC処理
 *------------------------------------------
 */
int npc_click(struct map_session_data *sd,int id)
{
	struct npc_data *nd;

	nullpo_retr(1, sd);

	if (sd->npc_id != 0) {
		if (battle_config.error_log)
			printf("npc_click: npc_id != 0\n");
		return 1;
	}

	if (npc_checknear(sd,id))
		return 1;

	nd=(struct npc_data *)map_id2bl(id);

	if (nd->flag&1)	// 無効化されている
		return 1;

	sd->npc_id=id;
	switch(nd->bl.subtype) {
	case SHOP:
		clif_npcbuysell(sd,id);
		npc_event_dequeue(sd);
		break;
	case SCRIPT:
		sd->npc_pos=run_script(nd->u.scr.script,0,sd->bl.id,id);
		break;
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_scriptcont(struct map_session_data *sd,int id)
{
	struct npc_data *nd;

	nullpo_retr(1, sd);

	if (id!=sd->npc_id)
		return 1;
	if (npc_checknear(sd,id))
		return 1;

	nd=(struct npc_data *)map_id2bl(id);

	sd->npc_pos=run_script(nd->u.scr.script,sd->npc_pos,sd->bl.id,id);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_buysellsel(struct map_session_data *sd,int id,int type)
{
	struct npc_data *nd;

	nullpo_retr(1, sd);

	if (npc_checknear(sd,id))
		return 1;

	nd=(struct npc_data *)map_id2bl(id);
	if (nd->bl.subtype!=SHOP) {
		if (battle_config.error_log)
			printf("no such shop npc : %d\n",id);
		sd->npc_id=0;
		return 1;
	}
	if (nd->flag&1)	// 無効化されている
		return 1;

	sd->npc_shopid=id;
	if (type==0) {
		clif_buylist(sd,nd);
	} else {
		clif_selllist(sd);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_buylist(struct map_session_data *sd,int n,unsigned short *item_list)
{
	struct npc_data *nd;
	double z;
	int i,j,w,skill,itemamount=0,new_=0;

	nullpo_retr(3, sd);
	nullpo_retr(3, item_list);

	if (npc_checknear(sd,sd->npc_shopid))
		return 3;

	nd=(struct npc_data*)map_id2bl(sd->npc_shopid);
	if (nd->bl.subtype!=SHOP)
		return 3;

	for(i=0,w=0,z=0;i<n;i++) {
		for(j=0;nd->u.shop_item[j].nameid;j++) {
			if (nd->u.shop_item[j].nameid==item_list[i*2+1])
				break;
		}
		if (nd->u.shop_item[j].nameid==0)
			return 3;

		if (itemdb_value_notdc(nd->u.shop_item[j].nameid))
			z+=(double)nd->u.shop_item[j].value * item_list[i*2];
		else
			z+=(double)pc_modifybuyvalue(sd,nd->u.shop_item[j].value) * item_list[i*2];
		itemamount+=item_list[i*2];

		switch(pc_checkadditem(sd,item_list[i*2+1],item_list[i*2])) {
		case ADDITEM_EXIST:
			break;
		case ADDITEM_NEW:
			new_++;
			break;
		case ADDITEM_OVERAMOUNT:
			return 2;
		}

		w+=itemdb_weight(item_list[i*2+1]) * item_list[i*2];
	}
	if (z > (double)sd->status.zeny)
		return 1;	// zeny不足
	if (w+sd->weight > sd->max_weight)
		return 2;	// 重量超過
	if (pc_inventoryblank(sd)<new_)
		return 3;	// 種類数超過

	pc_payzeny(sd,(int)z);
	for(i=0;i<n;i++) {
		struct item item_tmp;

		memset(&item_tmp,0,sizeof(item_tmp));
		item_tmp.nameid = item_list[i*2+1];
		item_tmp.identify = 1;	// npc販売アイテムは鑑定済み

		pc_additem(sd,&item_tmp,item_list[i*2]);
	}

	//商人経験値
/*	if ((sd->status.class_ == 5) || (sd->status.class_ == 10) || (sd->status.class_ == 18)) {
		z = z * pc_checkskill(sd,MC_DISCOUNT) / ((1 + 300 / itemamount) * 4000) * battle_config.shop_exp;
		pc_gainexp(sd,0,z);
	}*/
	if (battle_config.shop_exp > 0 && z > 0 && (skill = pc_checkskill(sd,MC_DISCOUNT)) > 0) {
		if (sd->status.skill[MC_DISCOUNT].flag != 0)
			skill = sd->status.skill[MC_DISCOUNT].flag - 2;
		if (skill > 0) {
			z = z * (double)skill * (double)battle_config.shop_exp/10000.;
			if (z < 1)
				z = 1;
			pc_gainexp(sd,0,(int)z);
		}
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int npc_selllist(struct map_session_data *sd,int n,unsigned short *item_list)
{
	double z;
	int i,skill,itemamount=0;

	nullpo_retr(1, sd);
	nullpo_retr(1, item_list);

	if (npc_checknear(sd,sd->npc_shopid))
		return 1;
	for(i=0,z=0;i<n;i++) {
		int nameid;
		if (item_list[i*2]-2 <0 || item_list[i*2]-2 >=MAX_INVENTORY)
			return 1;
		nameid=sd->status.inventory[item_list[i*2]-2].nameid;
		if (nameid == 0 ||
		   sd->status.inventory[item_list[i*2]-2].amount < item_list[i*2+1])
			return 1;
		if (itemdb_value_notoc(nameid))
			z+=(double)itemdb_value_sell(nameid) * item_list[i*2+1];
		else
			z+=(double)pc_modifysellvalue(sd,itemdb_value_sell(nameid)) * item_list[i*2+1];
		itemamount+=item_list[i*2+1];
	}

	if (z > MAX_ZENY) z = MAX_ZENY;
	pc_getzeny(sd,(int)z);
	for(i=0;i<n;i++) {
		int item_id=item_list[i*2]-2;
		if(	sd->status.inventory[item_id].nameid>0 && sd->inventory_data[item_id] != NULL &&
			sd->inventory_data[item_id]->type==7 && sd->status.inventory[item_id].amount>0 &&
			sd->status.inventory[item_id].card[0] == (short)0xff00)
				if(search_petDB_index(sd->status.inventory[item_id].nameid, PET_EGG) >= 0)
					intif_delete_petdata(MakeDWord(sd->status.inventory[item_id].card[1],sd->status.inventory[item_id].card[2]));
		pc_delitem(sd,item_id,item_list[i*2+1],0);
	}

	//商人経験値
	if (battle_config.shop_exp > 0 && z > 0 && (skill = pc_checkskill(sd,MC_OVERCHARGE)) > 0) {
		if (sd->status.skill[MC_OVERCHARGE].flag != 0)
			skill = sd->status.skill[MC_OVERCHARGE].flag - 2;
		if (skill > 0) {
			z = z * (double)skill * (double)battle_config.shop_exp/10000.;
			if (z < 1)
				z = 1;
			pc_gainexp(sd,0,(int)z);
		}
	}

	return 0;

}

// [Valaris] NPC Walking

/*==========================================
 * Time calculation concerning one step next to npc
 *------------------------------------------
 */
static int calc_next_walk_step(struct npc_data *nd)
{
	nullpo_retr(0, nd);

	if(nd->walkpath.path_pos>=nd->walkpath.path_len)
		return -1;
	if(nd->walkpath.path[nd->walkpath.path_pos]&1)
		return status_get_speed(&nd->bl)*14/10;
	return status_get_speed(&nd->bl);
}


/*==========================================
 * npc Walk processing
 *------------------------------------------
 */
static int npc_walk(struct npc_data *nd,unsigned int tick,int data)
{
	int moveblock;
	int i;
	static int dirx[8]={0,-1,-1,-1,0,1,1,1};
	static int diry[8]={1,1,0,-1,-1,-1,0,1};
	int x,y,dx,dy;

	nullpo_retr(0, nd);

	nd->state.state=MS_IDLE;
	if(nd->walkpath.path_pos>=nd->walkpath.path_len || nd->walkpath.path_pos!=data)
		return 0;

	nd->walkpath.path_half ^= 1;
	if(nd->walkpath.path_half==0){
		nd->walkpath.path_pos++;
		if(nd->state.change_walk_target){
			npc_walktoxy_sub(nd);
			return 0;
		}
	}
	else {
		if(nd->walkpath.path[nd->walkpath.path_pos]>=8)
			return 1;

		x = nd->bl.x;
		y = nd->bl.y;
		if(map_getcell(nd->bl.m,x,y,CELL_CHKNOPASS)) {
			npc_stop_walking(nd,1);
			return 0;
		}
		nd->dir=nd->walkpath.path[nd->walkpath.path_pos];
		dx = dirx[nd->dir];
		dy = diry[nd->dir];

		if(map_getcell(nd->bl.m,x+dx,y+dy,CELL_CHKNOPASS)) {
			npc_walktoxy_sub(nd);
			return 0;
		}

		moveblock = ( x/BLOCK_SIZE != (x+dx)/BLOCK_SIZE || y/BLOCK_SIZE != (y+dy)/BLOCK_SIZE);

		nd->state.state=MS_WALK;
		map_foreachinmovearea(clif_npcoutsight,nd->bl.m,x-AREA_SIZE,y-AREA_SIZE,x+AREA_SIZE,y+AREA_SIZE,dx,dy,BL_PC,nd);

		x += dx;
		y += dy;

		if(moveblock) map_delblock(&nd->bl);
		nd->bl.x = x;
		nd->bl.y = y;
		if(moveblock) map_addblock(&nd->bl);

		map_foreachinmovearea(clif_npcinsight,nd->bl.m,x-AREA_SIZE,y-AREA_SIZE,x+AREA_SIZE,y+AREA_SIZE,-dx,-dy,BL_PC,nd);
		nd->state.state=MS_IDLE;
	}
	if((i=calc_next_walk_step(nd))>0){
		i = i>>1;
		if(i < 1 && nd->walkpath.path_half == 0)
			i = 1;
		nd->walktimer=add_timer(tick+i,npc_walktimer,nd->bl.id,nd->walkpath.path_pos);
		nd->state.state=MS_WALK;

		if(nd->walkpath.path_pos>=nd->walkpath.path_len)
			clif_fixnpcpos(nd);	// When npc stops, retransmission current of a position.

	}
	return 0;
}

int npc_changestate(struct npc_data *nd,int state,int type)
{
	int i;

	nullpo_retr(0, nd);

	if(nd->walktimer != -1)
		delete_timer(nd->walktimer,npc_walktimer);
	nd->walktimer=-1;
	nd->state.state=state;

	switch(state){
	case MS_WALK:
		if((i=calc_next_walk_step(nd))>0){
			i = i>>2;
			nd->walktimer=add_timer(gettick()+i,npc_walktimer,nd->bl.id,0);
		}
		else
			nd->state.state=MS_IDLE;
		break;
	case MS_DELAY:
		nd->walktimer=add_timer(gettick()+type,npc_walktimer,nd->bl.id,0);
		break;

	}

	return 0;
}

static int npc_walktimer(int tid,unsigned int tick,int id,int data)
{
	struct npc_data *nd;

	nd=(struct npc_data*)map_id2bl(id);
	if(nd == NULL || nd->bl.type != BL_NPC)
		return 1;

	if(nd->walktimer != tid){
		return 0;
	}

	nd->walktimer=-1;

	if(nd->bl.prev == NULL)
		return 1;

	switch(nd->state.state){
		case MS_WALK:
			npc_walk(nd,tick,data);
			break;
		case MS_DELAY:
			npc_changestate(nd,MS_IDLE,0);
			break;
		default:
			break;
	}
	return 0;
}


static int npc_walktoxy_sub(struct npc_data *nd)
{
	struct walkpath_data wpd;

	nullpo_retr(0, nd);

	if(path_search(&wpd,nd->bl.m,nd->bl.x,nd->bl.y,nd->to_x,nd->to_y,nd->state.walk_easy))
		return 1;
	memcpy(&nd->walkpath,&wpd,sizeof(wpd));

	nd->state.change_walk_target=0;
	npc_changestate(nd,MS_WALK,0);

	clif_movenpc(nd);

	return 0;
}

int npc_walktoxy(struct npc_data *nd,int x,int y,int easy)
{
	struct walkpath_data wpd;

	nullpo_retr(0, nd);

	if(nd->state.state == MS_WALK && path_search(&wpd,nd->bl.m,nd->bl.x,nd->bl.y,x,y,0) )
		return 1;

	nd->state.walk_easy = easy;
	nd->to_x=x;
	nd->to_y=y;
	if(nd->state.state == MS_WALK) {
		nd->state.change_walk_target=1;
	} else {
		return npc_walktoxy_sub(nd);
	}

	return 0;
}

int npc_stop_walking(struct npc_data *nd,int type)
{
	nullpo_retr(0, nd);

	if(nd->state.state == MS_WALK || nd->state.state == MS_IDLE) {
		int dx=0,dy=0;

		nd->walkpath.path_len=0;
		if(type&4){
			dx=nd->to_x-nd->bl.x;
			if(dx<0)
				dx=-1;
			else if(dx>0)
				dx=1;
			dy=nd->to_y-nd->bl.y;
			if(dy<0)
				dy=-1;
			else if(dy>0)
				dy=1;
		}
		nd->to_x=nd->bl.x+dx;
		nd->to_y=nd->bl.y+dy;
		if(dx!=0 || dy!=0){
			npc_walktoxy_sub(nd);
			return 0;
		}
		npc_changestate(nd,MS_IDLE,0);
	}
	if(type&0x01)
		clif_fixnpcpos(nd);
	if(type&0x02) {
		int delay=status_get_dmotion(&nd->bl);
		unsigned int tick = gettick();
		if(nd->canmove_tick < tick)
			nd->canmove_tick = tick + delay;
	}

	return 0;
}

int npc_remove_map (struct npc_data *nd)
{
    nullpo_retr(1, nd);

    if(nd->bl.prev == NULL)
        return 1;

#ifdef PCRE_SUPPORT
    npc_chat_finalize(nd);
#endif
    clif_clearchar_area(&nd->bl,2);
	strdb_erase(npcname_db, (nd->bl.subtype < SCRIPT) ? nd->name : nd->exname);
    map_delblock(&nd->bl);
	map_deliddb(&nd->bl);

    return 0;
}

int npc_unload(struct npc_data *nd)
{
	npc_remove_map (nd);

	npc_data_final(nd);

	return 0;
}

//
// 初期化関係
//

/*==========================================
 * 読み込むnpcファイルのクリア
 *------------------------------------------
 */
void npc_clearsrcfile()
{
	struct npc_src_list *p=npc_src_first;

	while( p ) {
		struct npc_src_list *p2=p;
		p=p->next;
		aFree(p2);
	}
	npc_src_first=NULL;
	npc_src_last=NULL;
}
/*==========================================
 * 読み込むnpcファイルの追加
 *------------------------------------------
 */
void npc_addsrcfile(char *name)
{
	struct npc_src_list *new_;
	size_t len;

	if ( strcmpi(name,"clear")==0 ) {
		npc_clearsrcfile();
		return;
	}

	{
		// prevent multiple insert of source files
		struct npc_src_list *p=npc_src_first;
		while( p )
		{   // found the file, no need to insert it again
			if( 0==strcmp(name,p->name) )
				return;
			p=p->next;
		}
	}

	len = sizeof(*new_) + strlen(name);
	new_=(struct npc_src_list *)aCalloc(1,len);
	new_->next = NULL;
	strncpy(new_->name,name,strlen(name)+1);
	if (npc_src_first==NULL)
		npc_src_first = new_;
	if (npc_src_last)
		npc_src_last->next = new_;

	npc_src_last=new_;
}
/*==========================================
 * 読み込むnpcファイルの削除
 *------------------------------------------
 */
void npc_delsrcfile(char *name)
{
	struct npc_src_list *p=npc_src_first,*pp=NULL,**lp=&npc_src_first;

	if ( strcmpi(name,"all")==0 ) {
		npc_clearsrcfile();
		return;
	}

	for( ; p; lp=&p->next,pp=p,p=p->next ) {
		if ( strcmp(p->name,name)==0 ) {
			*lp=p->next;
			if ( npc_src_last==p )
				npc_src_last=pp;
			aFree(p);
			break;
		}
	}
}

/*==========================================
 * warp行解析
 *------------------------------------------
 */
int npc_parse_warp(char *w1,char *w2,char *w3,char *w4)
{
	int x,y,xs,ys,to_x,to_y,m;
	int i,j;
	char mapname[24],to_mapname[24];
	struct npc_data *nd;

	// 引数の個数チェック
	if (sscanf(w1,"%[^,],%d,%d",mapname,&x,&y) != 3 ||
	   sscanf(w4,"%d,%d,%[^,],%d,%d",&xs,&ys,to_mapname,&to_x,&to_y) != 5) {
		printf("bad warp line : %s\n",w3);
		return 1;
	}

	m=map_mapname2mapid(mapname);

	nd=(struct npc_data *)aCalloc(1,sizeof(struct npc_data));
	nd->bl.id=npc_get_new_npc_id();
	nd->n=map_addnpc(m,nd);

	nd->bl.prev = nd->bl.next = NULL;
	nd->bl.m=m;
	nd->bl.x=x;
	nd->bl.y=y;
	nd->dir=0;
	nd->flag=0;
	memcpy(nd->name,w3,24);
	memcpy(nd->exname,w3,24);

	nd->chat_id=0;
	if (!battle_config.warp_point_debug)
		nd->class_=WARP_CLASS;
	else
		nd->class_=WARP_DEBUG_CLASS;
	nd->speed=200;
	nd->option = 0;
	nd->opt1 = 0;
	nd->opt2 = 0;
	nd->opt3 = 0;
	memcpy(nd->u.warp.name,to_mapname,16);
	xs+=2; ys+=2;
	nd->u.warp.x=to_x;
	nd->u.warp.y=to_y;
	nd->u.warp.xs=xs;
	nd->u.warp.ys=ys;

	for(i=0;i<ys;i++) {
		for(j=0;j<xs;j++) {
			if(map_getcell(m,x-xs/2+j,y-ys/2+i,CELL_CHKNOPASS))
				continue;
			map_setcell(m,x-xs/2+j,y-ys/2+i,CELL_SETNPC);
		}
	}

//	printf("warp npc %s %d read done\n",mapname,nd->bl.id);
	npc_warp++;
	nd->bl.type=BL_NPC;
	nd->bl.subtype=WARP;
	map_addblock(&nd->bl);
	clif_spawnnpc(nd);
	strdb_insert(npcname_db,nd->name,nd);

	return 0;
}

/*==========================================
 * shop行解析
 *------------------------------------------
 */
static int npc_parse_shop(char *w1,char *w2,char *w3,char *w4)
{
	char *p;
	int x, y, dir, m;
	int max = 100, pos = 0;
	char mapname[24];
	struct npc_data *nd;

	// 引数の個数チェック
	if (sscanf(w1, "%[^,],%d,%d,%d", mapname, &x, &y, &dir) != 4 ||
	    strchr(w4, ',') == NULL) {
		printf("bad shop line : %s\n", w3);
		return 1;
	}
	m = map_mapname2mapid(mapname);

	nd = (struct npc_data *)aCalloc(1,sizeof(struct npc_data) +
		sizeof(nd->u.shop_item[0]) * (max + 1));
	p = strchr(w4, ',');

	while (p && pos < max) {
		int nameid,value;
		struct item_data *id;
		p++;
		if (sscanf(p, "%d:%d", &nameid, &value) != 2)
			break;
		nd->u.shop_item[pos].nameid = nameid;
		id = itemdb_search(nameid);
		if (value < 0)			
			value = id->value_buy;
		nd->u.shop_item[pos].value = value;
		// check for bad prices that can possibly cause exploits
		if (value*75/100 < id->value_sell*124/100) {
			sprintf (tmp_output, "Item %s [%d] buying:%d < selling:%d\n",
				id->name, id->nameid, value*75/100, id->value_sell*124/100);
			ShowWarning (tmp_output);
		}
		pos++;
		p=strchr(p,',');
	}
	if (pos == 0) {
		aFree(nd);
		return 1;
	}
	nd->u.shop_item[pos++].nameid = 0;

	nd->bl.prev = nd->bl.next = NULL;
	nd->bl.m = m;
	nd->bl.x = x;
	nd->bl.y = y;
	nd->bl.id = npc_get_new_npc_id();
	nd->dir = dir;
	nd->flag = 0;
	memcpy(nd->name, w3, 24);
	nd->class_ = atoi(w4);
	nd->speed = 200;
	nd->chat_id = 0;
	nd->option = 0;
	nd->opt1 = 0;
	nd->opt2 = 0;
	nd->opt3 = 0;

	nd = (struct npc_data *)aRealloc(nd,
		sizeof(struct npc_data) + sizeof(nd->u.shop_item[0]) * pos);

	//printf("shop npc %s %d read done\n",mapname,nd->bl.id);
	npc_shop++;
	nd->bl.type=BL_NPC;
	nd->bl.subtype=SHOP;
	nd->n=map_addnpc(m,nd);
	map_addblock(&nd->bl);
	clif_spawnnpc(nd);
	strdb_insert(npcname_db,nd->name,nd);

	return 0;
}
/*==========================================
 * NPCのラベルデータコンバート
 *------------------------------------------
 */
int npc_convertlabel_db(void *key,void *data,va_list ap)
{
	char *lname=(char *)key;
	int pos=(int)data;
	struct npc_data *nd;
	struct npc_label_list *lst;
	int num;
	char *p=strchr(lname,':');

	nullpo_retr(0, ap);
	nullpo_retr(0, nd=va_arg(ap,struct npc_data *));

	lst=nd->u.scr.label_list;
	num=nd->u.scr.label_list_num;
	if(!lst){
		lst=(struct npc_label_list *)aCallocA(1,sizeof(struct npc_label_list));
		num=0;
	}else
		lst=(struct npc_label_list *)aRealloc(lst,sizeof(struct npc_label_list)*(num+1));

	*p='\0';
	
	// here we check if the label fit into the buffer
	if (strlen(lname)>23) { 
		printf("npc_parse_script: label name longer than 23 chars! '%s'\n (%s)", lname, current_file);
		exit(1);
	}
	memcpy(lst[num].name,lname,strlen(lname)+1); //including EOS
	
	
	*p=':';
	lst[num].pos=pos;
	nd->u.scr.label_list=lst;
	nd->u.scr.label_list_num=num+1;
	return 0;
}
/*==========================================
 * script行解析
 *------------------------------------------
 */
static int npc_parse_script(char *w1,char *w2,char *w3,char *w4,char *first_line,FILE *fp,int *lines)
{
	int x, y, dir = 0, m, xs = 0, ys = 0, class_ = 0;	// [Valaris] thanks to fov
	char mapname[24];
	unsigned char *srcbuf = NULL, *script;
	int srcsize = 65536;
	int startline = 0;
	unsigned char line[1024];
	int i;
	struct npc_data *nd;
	int evflag = 0;
	struct dbt *label_db;
	char *p;
	struct npc_label_list *label_dup = NULL;
	int label_dupnum = 0;
	int src_id = 0;

	if (strcmp(w1, "-") == 0) {
		x = 0; y = 0; m = -1;
	} else {
		// 引数の個数チェック
		if (sscanf(w1, "%[^,],%d,%d,%d", mapname, &x, &y, &dir) != 4 ||
			(strcmp(w2, "script") == 0 && strchr(w4,',') == NULL)) {
			printf("bad script line : %s\n", w3);
			return 1;
		}
		m = map_mapname2mapid(mapname);
	}

	if (strcmp(w2, "script") == 0){
		// スクリプトの解析
		srcbuf = (unsigned char *)aCallocA(srcsize, sizeof(char));
		if (strchr(first_line, '{')) {
			strcpy((char *)srcbuf, strchr(first_line, '{'));
			startline = *lines;
		} else
			srcbuf[0] = 0;
		while (1) {
			for (i = strlen((const char *)srcbuf) - 1; i >= 0 && isspace(srcbuf[i]); i--)
				;
			if (i >= 0 && srcbuf[i] == '}')
				break;
			fgets ((char *)line, 1020, fp);
			(*lines)++;
			if (feof(fp))
				break;
			if (strlen((char *)srcbuf) + strlen((char *)line) + 1 >= srcsize) {
				srcsize += 65536;
				srcbuf = (unsigned char *)aRealloc(srcbuf, srcsize);
				memset(srcbuf + srcsize - 65536, '\0', 65536);
			}
			if (srcbuf[0] != '{') {
				if (strchr((char *) line,'{')) {
					strcpy((char *) srcbuf, strchr((const char *) line, '{'));
					startline = *lines;
				}
			} else
				strcat((char *) srcbuf, (const char *) line);
		}
		script = (unsigned char *) parse_script((unsigned char *) srcbuf, startline);
		if (script == NULL) {
			// script parse error?
			aFree(srcbuf);
			return 1;
		}
	} else {
		// duplicateする
		char srcname[128];
		struct npc_data *nd2;
		if (sscanf(w2, "duplicate(%[^)])", srcname) != 1) {
			printf("bad duplicate name (in %s)! : %s", current_file, w2);
			return 0;
		}
		if ((nd2 = npc_name2id(srcname)) == NULL) {
			printf("bad duplicate name (in %s)! (not exist) : %s\n", current_file, srcname);
			return 0;
		}
		script = (unsigned char *)nd2->u.scr.script;
		label_dup = nd2->u.scr.label_list;
		label_dupnum = nd2->u.scr.label_list_num;
		src_id = nd2->bl.id;

	}// end of スクリプト解析

	nd = (struct npc_data *)aCalloc(1, sizeof(struct npc_data));

	if (m == -1){
		// スクリプトコピー用のダミーNPC
	} else if (sscanf(w4, "%d,%d,%d", &class_, &xs, &ys) == 3) {
		// 接触型NPC
		int i, j;

		if (xs >= 0) xs = xs * 2 + 1;
		if (ys >= 0) ys = ys * 2 + 1;

		if (class_ >= 0) {
			for (i = 0; i < ys; i++) {
				for (j = 0; j < xs; j++) {
					if (map_getcell(m, x - xs/2 + j, y - ys/2 + i, CELL_CHKNOPASS))
						continue;
					map_setcell(m, x - xs/2 + j, y - ys/2 + i, CELL_SETNPC);
				}
			}
		}
		nd->u.scr.xs = xs;
		nd->u.scr.ys = ys;
	} else {
		// クリック型NPC
		class_ = atoi(w4);
		nd->u.scr.xs = 0;
		nd->u.scr.ys = 0;
	}

	if (class_ < 0 && m >= 0) {	// イベント型NPC
		evflag = 1;
	}

	while ((p = strchr(w3,':'))) {
		if (p[1] == ':') break;
	}
	if (p) {
		*p = 0;
		memcpy(nd->name, w3, 24);
		memcpy(nd->exname, p+2, 24);
	} else {
		memcpy(nd->name, w3, 24);
		memcpy(nd->exname, w3, 24);
	}

	nd->bl.prev = nd->bl.next = NULL;
	nd->bl.m = m;
	nd->bl.x = x;
	nd->bl.y = y;
	nd->bl.id = npc_get_new_npc_id();
	nd->dir = dir;
	nd->flag = 0;
	nd->class_ = class_;
	nd->speed = 200;
	nd->u.scr.script = (char *) script;
	nd->u.scr.src_id = src_id;
	nd->chat_id = 0;
	nd->option = 0;
	nd->opt1 = 0;
	nd->opt2 = 0;
	nd->opt3 = 0;
	nd->walktimer = -1;

	//printf("script npc %s %d %d read done\n",mapname,nd->bl.id,nd->class_);
	npc_script++;
	nd->bl.type = BL_NPC;
	nd->bl.subtype = SCRIPT;

	// clear event timers upon initialise
	memset (nd->eventqueue, 0, sizeof(nd->eventqueue));
	for (i = 0; i < MAX_EVENTTIMER; i++)
		nd->eventtimer[i] = -1;

	if (m >= 0) {
		nd->n = map_addnpc(m, nd);
		map_addblock(&nd->bl);

		if (evflag) {	// イベント型
			struct event_data *ev = (struct event_data *)aCalloc(1, sizeof(struct event_data));
			ev->nd = nd;
			ev->pos = 0;
			strdb_insert(ev_db, nd->exname, ev);
		} else
			clif_spawnnpc(nd);
	} else {
		// we skip map_addnpc, but still add it to the list of ID's
		map_addiddb(&nd->bl);
	}
	strdb_insert(npcname_db, nd->exname, nd);

	//-----------------------------------------
	// ラベルデータの準備
	if (srcbuf){
		// script本体がある場合の処理
		// ラベルデータのコンバート
		label_db = script_get_label_db();
		strdb_foreach(label_db, npc_convertlabel_db, nd);

		// もう使わないのでバッファ解放
		aFree(srcbuf);
	} else {
		// duplicate
//		nd->u.scr.label_list = aMallocA(sizeof(struct npc_label_list)*label_dupnum);
//		memcpy(nd->u.scr.label_list,label_dup,sizeof(struct npc_label_list)*label_dupnum);
		nd->u.scr.label_list = label_dup;	// ラベルデータ共有
		nd->u.scr.label_list_num = label_dupnum;
	}

	//-----------------------------------------
	// イベント用ラベルデータのエクスポート
	for (i = 0; i < nd->u.scr.label_list_num; i++){
		char *lname = nd->u.scr.label_list[i].name;
		int pos = nd->u.scr.label_list[i].pos;

		if ((lname[0] == 'O' || lname[0] == 'o') && (lname[1] == 'N' || lname[1] == 'n')) {
/*
I rearrange the code so this is just for commenting; remove it if you have enough if it [Shinomori]
			struct event_data *ev;
			char *buf;
			// エクスポートされる
			ev=(struct event_data *)aCalloc(1,sizeof(struct event_data));
why allocing 50 chars ?
			buf=(char *)aCallocA(50,sizeof(char));
why checking here? 
lname is identical to nd->u.scr.label_list[i].name which is only 24 chars so check for strlen should be 23
			if (strlen(lname)>24) {
				printf("npc_parse_script: label name error (%s) !\n", current_file);
				exit(1);
			}else{
				//struct event_data *ev2;
				ev->nd=nd;
				ev->pos=pos;
				sprintf(buf,"%s::%s",nd->exname,lname);
				//ev2 = strdb_search(ev_db,buf);
				//if(ev2 != NULL) {
				//	printf("npc_parse_script : duplicate event %s\n",buf);
				//	aFree(ev2);
				//}
you are sure reentering the same database key will overwrite the existing entry?
				strdb_insert(ev_db,buf,ev);
anyway instead of removing data from the db and inserting a new one
wouldn't it be easier just not to insert the new duplicate event, it is a duplicate anyway?
			}	
*/
			// this check is useless here because the buffer is only 24 chars 
			// and already overwritten if this is here is reached
			// I leave the check anyway but place it correctly to npc_convertlabel_db
			if (strlen(lname)>23) { 
				printf("npc_parse_script: label name longer than 23 chars! '%s' (%s)\n", lname, current_file);
				exit(1);
			}else{
				struct event_data *ev;
				struct event_data *ev2;
				char *buf;
				// エクスポートされる
				
				// 51 comes from: 24 for npc name + 24 for label + 2 for a "::" and 1 for EOS
				//buf=(char *)aMalloc(51,sizeof(char)); 
				// but to save some memory we alloc only the really necessary space
				buf=(char *)aMalloc( (3+strlen(nd->exname)+strlen(lname))*sizeof(char));
				sprintf(buf,"%s::%s",nd->exname,lname);
				
				// search the label in ev_db; 
				// remember the label is max 50 chars + eos; see the strdb_init below
				ev2 = (struct event_data *)strdb_search(ev_db,buf);
				if(ev2 != NULL) {
					printf("npc_parse_script : duplicate event %s (%s)\n",buf, current_file);
					
					// just skip the label insertion and free the alloced buffer
					aFree(buf);
				}
				else
				{	// generate the data and insert it
					ev=(struct event_data *)aCalloc(1,sizeof(struct event_data));
					ev->nd=nd;
					ev->pos=pos;
					strdb_insert(ev_db,buf,ev);
				}

			}
		}
	}

	//-----------------------------------------
	// ラベルデータからタイマーイベント取り込み
	for (i = 0; i < nd->u.scr.label_list_num; i++){
		int t = 0, k = 0;
		char *lname = nd->u.scr.label_list[i].name;
		int pos = nd->u.scr.label_list[i].pos;
		if (sscanf(lname, "OnTimer%d%n", &t, &k) == 1 && lname[k] == '\0') {
			// タイマーイベント
			struct npc_timerevent_list *te = nd->u.scr.timer_event;
			int j, k = nd->u.scr.timeramount;
			if (te == NULL)
				te = (struct npc_timerevent_list *)aCallocA(1,sizeof(struct npc_timerevent_list));
			else
				te = (struct npc_timerevent_list *)aRealloc( te, sizeof(struct npc_timerevent_list) * (k+1) );
			for (j = 0; j < k; j++){
				if (te[j].timer > t){
					memmove(te+j+1, te+j, sizeof(struct npc_timerevent_list)*(k-j));
					break;
				}
			}
			te[j].timer = t;
			te[j].pos = pos;
			nd->u.scr.timer_event = te;
			nd->u.scr.timeramount = k+1;
		}
	}
	nd->u.scr.nexttimer = -1;
	nd->u.scr.timerid = -1;

	return 0;
}

/*==========================================
 * function行解析
 *------------------------------------------
 */
static int npc_parse_function(char *w1,char *w2,char *w3,char *w4,char *first_line,FILE *fp,int *lines)
{
	char *srcbuf=NULL,*script;
	int srcsize=65536;
	int startline=0;
	char line[1024];
	int i;
//	struct dbt *label_db;
	char *p;

	// スクリプトの解析
	srcbuf=(char *)aCallocA(srcsize,sizeof(char));
	if (strchr(first_line,'{')) {
		strcpy(srcbuf,strchr(first_line,'{'));
		startline=*lines;
	} else
		srcbuf[0]=0;
	while(1) {
		for(i=strlen(srcbuf)-1;i>=0 && isspace(srcbuf[i]);i--);
		if (i>=0 && srcbuf[i]=='}')
			break;
		fgets(line,1020,fp);
		(*lines)++;
		if (feof(fp))
			break;
		if (strlen(srcbuf)+strlen(line)+1>=srcsize) {
			srcsize += 65536;
			srcbuf = (char *)aRealloc(srcbuf, srcsize);
			memset(srcbuf + srcsize - 65536, '\0', 65536);
		}
		if (srcbuf[0]!='{') {
			if (strchr(line,'{')) {
				strcpy(srcbuf,strchr(line,'{'));
				startline=*lines;
			}
		} else
			strcat(srcbuf,line);
	}
	script= parse_script((unsigned char *) srcbuf,startline);
	if (script==NULL) {
		// script parse error?
		aFree(srcbuf);
		return 1;
	}

	p=(char *)aCallocA(50,sizeof(char));

	strncpy(p,w3,50);
	strdb_insert(script_get_userfunc_db(),p,script);

//	label_db=script_get_label_db();

	// もう使わないのでバッファ解放
	aFree(srcbuf);

//	printf("function %s => %p\n",p,script);

	return 0;
}


/*==========================================
 * mob行解析
 *------------------------------------------
 */
int npc_parse_mob(char *w1,char *w2,char *w3,char *w4)
{
	int m,x,y,xs,ys,class_,num,delay1,delay2,level;
	int i;
	char mapname[24];
	char mobname[24];
	char eventname[24]="";
	struct mob_data *md;

	xs=ys=0;
	delay1=delay2=0;
	// 引数の個数チェック
	if (sscanf(w1,"%[^,],%d,%d,%d,%d",mapname,&x,&y,&xs,&ys) < 3 ||
	   sscanf(w4,"%d,%d,%d,%d,%s",&class_,&num,&delay1,&delay2,eventname) < 2 ) {
		printf("bad monster line : %s\n",w3);
		return 1;
	}

	m=map_mapname2mapid(mapname);

	if ( num>1 && battle_config.mob_count_rate!=100) {
		if ( (num=num*battle_config.mob_count_rate/100)<1 )
			num=1;
	}

	for(i=0;i<num;i++) {
		md=(struct mob_data *)aCalloc(1,sizeof(struct mob_data));
		memset(md,0,sizeof(struct mob_data));	//Why not 0 up the structure?	[Skotlex]

		if(class_>4000) { // large/tiny mobs [Valaris]
			md->size=2;
			class_-=4000;
		}
		else if(class_>2000) {
			md->size=1;
			class_-=2000;
		}

		md->bl.prev=NULL;
		md->bl.next=NULL;
		md->bl.m=m;
		md->bl.x=x;
		md->bl.y=y;

		if(sscanf(w3,"%[^,],%d",mobname,&level) > 1)
			md->level=level;
		if(strcmp(mobname,"--en--")==0)
			memcpy(md->name,mob_db[class_].name,24);
		else if(strcmp(mobname,"--ja--")==0)
			memcpy(md->name,mob_db[class_].jname,24);
		else memcpy(md->name,mobname,24);		

		md->n = i;
		md->base_class = md->class_ = class_;
		md->bl.id=npc_get_new_npc_id();
		md->m =m;
		md->x0=x;
		md->y0=y;
		md->xs=xs;
		md->ys=ys;
		md->spawndelay1=delay1;
		md->spawndelay2=delay2;

//		memset(&md->state,0,sizeof(md->state));
		md->timer = -1;
//		md->target_id=0;
//		md->attacked_id=0;
		md->speed=mob_db[class_].speed;

		if (mob_db[class_].mode&0x02)
			md->lootitem=(struct item *)aCalloc(LOOTITEM_SIZE,sizeof(struct item));
		else
			md->lootitem=NULL;

		if (strlen(eventname)>=4) {
			memcpy(md->npc_event,eventname,24);
		}else
			memset(md->npc_event,0,24);

		md->bl.type=BL_MOB;
		map_addiddb(&md->bl);
		mob_spawn(md->bl.id);

		npc_mob++;
	}
	//printf("warp npc %s %d read done\n",mapname,nd->bl.id);

	return 0;
}

/*==========================================
 * マップフラグ行の解析
 *------------------------------------------
 */
static int npc_parse_mapflag(char *w1,char *w2,char *w3,char *w4)
{
	int m;
	char mapname[24],savemap[16];
	int savex,savey;
	char drop_arg1[16],drop_arg2[16];
	int drop_id=0,drop_type=0,drop_per=0;

	// 引数の個数チェック
//	if (	sscanf(w1,"%[^,],%d,%d,%d",mapname,&x,&y,&dir) != 4 )
	if (	sscanf(w1,"%[^,]",mapname) != 1 )
		return 1;

	m=map_mapname2mapid(mapname);
	if (m<0)
		return 1;

//マップフラグ
	if ( strcmpi(w3,"nosave")==0) {
		if (strcmp(w4,"SavePoint")==0) {
			memcpy(map[m].save.map,"SavePoint",10);
			map[m].save.x=-1;
			map[m].save.y=-1;
		}else if (sscanf(w4,"%[^,],%d,%d",savemap,&savex,&savey)==3) {
			memcpy(map[m].save.map,savemap,16);
			map[m].save.x=savex;
			map[m].save.y=savey;
		}
		map[m].flag.nosave=1;
	}
	else if (strcmpi(w3,"nomemo")==0) {
		map[m].flag.nomemo=1;
	}
	else if (strcmpi(w3,"noteleport")==0) {
		map[m].flag.noteleport=1;
	}
	else if (strcmpi(w3,"nowarp")==0) {
		map[m].flag.nowarp=1;
	}
	else if (strcmpi(w3,"nowarpto")==0) {
		map[m].flag.nowarpto=1;
	}
	else if (strcmpi(w3,"noreturn")==0) {
		map[m].flag.noreturn=1;
	}
	else if (strcmpi(w3,"monster_noteleport")==0) {
		map[m].flag.monster_noteleport=1;
	}
	else if (strcmpi(w3,"nobranch")==0) {
		map[m].flag.nobranch=1;
	}
	else if (strcmpi(w3,"nopenalty")==0) {
		map[m].flag.nopenalty=1;
	}
	else if (strcmpi(w3,"pvp")==0) {
		map[m].flag.pvp=1;
	}
	else if (strcmpi(w3,"pvp_noparty")==0) {
		map[m].flag.pvp_noparty=1;
	}
	else if (strcmpi(w3,"pvp_noguild")==0) {
		map[m].flag.pvp_noguild=1;
	}
	else if (strcmpi(w3,"pvp_nightmaredrop")==0) {
		if (sscanf(w4,"%[^,],%[^,],%d",drop_arg1,drop_arg2,&drop_per)==3) {		int i;
			if(strcmp(drop_arg1,"random")==0)
				drop_id = -1;
			else if(itemdb_exists( (drop_id=atoi(drop_arg1)) )==NULL)
				drop_id = 0;
			if(strcmp(drop_arg2,"inventory")==0)
				drop_type = 1;
			else if(strcmp(drop_arg2,"equip")==0)
				drop_type = 2;
			else if(strcmp(drop_arg2,"all")==0)
				drop_type = 3;

			if(drop_id != 0){
				for (i=0;i<MAX_DROP_PER_MAP;i++){
					if(map[m].drop_list[i].drop_id==0){
						map[m].drop_list[i].drop_id = drop_id;
						map[m].drop_list[i].drop_type = drop_type;
						map[m].drop_list[i].drop_per = drop_per;
						break;
					}
				}
				map[m].flag.pvp_nightmaredrop=1;
			}
		}
	}
	else if (strcmpi(w3,"pvp_nocalcrank")==0) {
		map[m].flag.pvp_nocalcrank=1;
	}
	else if (strcmpi(w3,"gvg")==0) {
		map[m].flag.gvg=1;
	}
	else if (strcmpi(w3,"gvg_noparty")==0) {
		map[m].flag.gvg_noparty=1;
	}
	else if (strcmpi(w3,"gvg_dungeon")==0) {
		map[m].flag.gvg_dungeon=1;
	}
	else if (strcmpi(w3,"nozenypenalty")==0) {
		map[m].flag.nozenypenalty=1;
	}
	else if (strcmpi(w3,"notrade")==0) {
		map[m].flag.notrade=1;
	}
	else if (strcmpi(w3,"noskill")==0) {
		map[m].flag.noskill=1;
	}
	else if (battle_config.pk_mode && strcmpi(w3,"nopvp")==0) { // nopvp for pk mode [Valaris]
		map[m].flag.nopvp=1;
		map[m].flag.pvp=0;
		map[m].flag.pkmode=0;
	}
	else if (strcmpi(w3,"pkmode")==0) {
		map[m].flag.pkmode=1;
	}
	else if (strcmpi(w3,"noicewall")==0) { // noicewall [Valaris]
		map[m].flag.noicewall=1;
	}
	else if (strcmpi(w3,"snow")==0) { // snow [Valaris]
		map[m].flag.snow=1;
	}
	else if (strcmpi(w3,"clouds")==0) {
		map[m].flag.clouds=1;
	}
	else if (strcmpi(w3,"fog")==0) { // fog [Valaris]
		map[m].flag.fog=1;
	}
	else if (strcmpi(w3,"fireworks")==0) {
		map[m].flag.fireworks=1;
	}
	else if (strcmpi(w3,"sakura")==0) { // sakura [Valaris]
		map[m].flag.sakura=1;
	}
	else if (strcmpi(w3,"leaves")==0) { // leaves [Valaris]
		map[m].flag.leaves=1;
	}
	else if (strcmpi(w3,"rain")==0) { // rain [Valaris]
		map[m].flag.rain=1;
	}
	else if (strcmpi(w3,"indoors")==0) { // celest
		map[m].flag.indoors=1;
	}
	else if (strcmpi(w3,"nogo")==0) { // celest
		map[m].flag.nogo=1;
	}
	else if (strcmpi(w3,"noexp")==0) { // Lorky
		map[m].flag.nobaseexp=1;
		map[m].flag.nojobexp=1;
	}
	else if (strcmpi(w3,"nobaseexp")==0) { // Lorky
		map[m].flag.nobaseexp=1;
	}
	else if (strcmpi(w3,"nojobexp")==0) { // Lorky
		map[m].flag.nojobexp=1;
	}
	else if (strcmpi(w3,"noloot")==0) { // Lorky
		map[m].flag.nomobloot=1;
		map[m].flag.nomvploot=1;
	}
	else if (strcmpi(w3,"nomobloot")==0) { // Lorky
		map[m].flag.nomobloot=1;
	}
	else if (strcmpi(w3,"nomvploot")==0) { // Lorky
		map[m].flag.nomvploot=1;
	}

	return 0;
}

/*==========================================
 * Setting up map cells
 *------------------------------------------
 */
static int npc_parse_mapcell(char *w1,char *w2,char *w3,char *w4)
{
	int m, cell, x, y, x0, y0, x1, y1;
	char type[24], mapname[24];

	if (sscanf(w1, "%[^,]", mapname) != 1)
		return 1;

	m = map_mapname2mapid(mapname);
	if (m < 0)
		return 1;

	if (sscanf(w3, "%[^,],%d,%d,%d,%d", type, &x0, &y0, &x1, &y1) < 4) {
		ShowError("Bad setcell line : %s\n",w3);
		return 1;
	}
	cell = strtol(type,(char **)NULL,0);
	//printf ("0x%x %d %d %d %d\n", cell, x0, y0, x1, y1);

	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }

	for (x = x0; x <= x1; x++) {
		for (y = y0; y <= y1; y++) {
			map_setcell(m,x,y,cell);
			//printf ("setcell 0x%x %d %d %d\n", cell, m, x, y);
		}
	}

	return 0;
}

void npc_parsesrcfile(char *name)
{
	int m, lines = 0;
	char line[1024];

	FILE *fp = fopen (name,"r");
	if (fp == NULL) {
		ShowError ("File not found : %s\n", name);
		exit(1);
	}
	current_file = name;

	while (fgets(line, 1020, fp)) {
		char w1[1024], w2[1024], w3[1024], w4[1024], mapname[1024];
		int i, j, w4pos, count;
		lines++;

		if (line[0] == '/' && line[1] == '/')
			continue;
		// 不要なスペースやタブの連続は詰める
		for (i = j = 0; line[i]; i++) {
			if (line[i]==' ') {
				if (!((line[i+1] && (isspace(line[i+1]) || line[i+1]==',')) ||
					 (j && line[j-1]==',')))
					line[j++]=' ';
			} else if (line[i]=='\t') {
				if (!(j && line[j-1]=='\t'))
					line[j++]='\t';
			} else
 				line[j++]=line[i];
		}
		// 最初はタブ区切りでチェックしてみて、ダメならスペース区切りで確認
		if ((count = sscanf(line,"%[^\t]\t%[^\t]\t%[^\t\r\n]\t%n%[^\t\r\n]", w1, w2, w3, &w4pos, w4)) < 3 &&
		   (count = sscanf(line,"%s%s%s%n%s", w1, w2, w3, &w4pos, w4)) < 3) {
			continue;
		}
		// マップの存在確認
		if (strcmp(w1,"-") !=0 && strcmpi(w1,"function") != 0 ){
			sscanf(w1,"%[^,]",mapname);
			m = map_mapname2mapid(mapname);
			if (strlen(mapname)>16 || m<0) {
			// "mapname" is not assigned to this server
				continue;
			}
		}
		if (strcmpi(w2,"warp") == 0 && count > 3) {
			npc_parse_warp(w1,w2,w3,w4);
		} else if (strcmpi(w2,"shop") == 0 && count > 3) {
			npc_parse_shop(w1,w2,w3,w4);
		} else if (strcmpi(w2,"script") == 0 && count > 3) {
			if (strcmpi(w1,"function") == 0) {
				npc_parse_function(w1,w2,w3,w4,line+w4pos,fp,&lines);
			} else {
				npc_parse_script(w1,w2,w3,w4,line+w4pos,fp,&lines);
			}
		} else if ((i = 0, sscanf(w2,"duplicate%n",&i), (i > 0 && w2[i] == '(')) && count > 3) {
			npc_parse_script(w1,w2,w3,w4,line+w4pos,fp,&lines);
		} else if (strcmpi(w2,"monster") == 0 && count > 3) {
			npc_parse_mob(w1,w2,w3,w4);
		} else if (strcmpi(w2,"mapflag") == 0 && count >= 3) {
			npc_parse_mapflag(w1,w2,w3,w4);
		} else if (strcmpi(w2,"setcell") == 0 && count >= 3) {
			npc_parse_mapcell(w1,w2,w3,w4);
		}
	}
	fclose(fp);

	return;
}

static int npc_read_indoors(void)
{
	char *buf,*p;
	int s, m;

	buf=(char *) grfio_reads("data\\indoorrswtable.txt",&s);

	if(buf==NULL)
		return -1;

	buf[s]=0;
	for(p=buf;p-buf<s;){
		char buf2[64];

		if(sscanf(p,"%[^#]#",buf2) == 1){
			char map_name[64] = "";
			strncpy(map_name, buf2, strlen(buf2) - 4);
			strcat(map_name, ".gat");
			if ((m = map_mapname2mapid(map_name)) >= 0)
				map[m].flag.indoors=1;
		}

		p=strchr(p,10);
		if(!p) break;
		p++;
	}
	aFree(buf);
	sprintf(tmp_output,"Done reading '"CL_WHITE"%s"CL_RESET"'.\n","data\\indoorrswtable.txt");
	ShowStatus(tmp_output);

	return 0;
}

static int ev_db_final(void *key,void *data,va_list ap)
{
	aFree(data);
	if(strstr((const char *) key,"::")!=NULL)
		aFree(key);
	return 0;
}

static void npc_data_final(struct npc_data *nd) {
	if (nd->chat_id) {
		struct chat_data *cd = (struct chat_data*)map_id2bl(nd->chat_id);
		if (cd) aFree (cd);
		cd = NULL;
	}	
	if (nd->bl.subtype == SCRIPT) {
		if (nd->u.scr.timerid != -1)
			delete_timer(nd->u.scr.timerid, npc_timerevent);
		npc_cleareventtimer (nd);
		if (nd->u.scr.timer_event)
			aFree(nd->u.scr.timer_event);
		if (nd->u.scr.src_id == 0) {
			if(nd->u.scr.script) {
				aFree(nd->u.scr.script);
				nd->u.scr.script = NULL;
			}
			if (nd->u.scr.label_list) {
				aFree(nd->u.scr.label_list);
				nd->u.scr.label_list = NULL;
			}
		}
	}
	aFree(nd);
}

static int npcname_db_final(void *key,void *data,va_list ap)
{
// At this point there shouldn't be any npc's left! If there are leave them to
// the memory allocators to report as memory leaks so it can be fixed
	/*struct npc_data *nd = (struct npc_data *) data;
	if (nd && nd->bl.prev != NULL)
		npc_data_final(nd);*/

	return 0;
}
/*==========================================
 * 
 *------------------------------------------
 */
int npc_cleanup_sub (struct block_list *bl, va_list ap) {
	nullpo_retr(0, bl);

	switch(bl->type) {
	case BL_NPC:
		npc_unload((struct npc_data *)bl);
		break;
	case BL_MOB:
		mob_unload((struct mob_data *)bl);
		break;
	}

	return 0;
}
int npc_reload(void)
{
	struct npc_src_list *nsl;
	int m, last_npc_id;
	time_t last_time = time(0);
	int busy = 0;
	char c = '-';

	for (m = 0; m < map_num; m++) {
		map_foreachinarea(npc_cleanup_sub, m, 0, 0, map[m].xs, map[m].ys, 0);
		map[m].npc_num = 0;
	}
	if(ev_db)
		strdb_final(ev_db,ev_db_final);
	if(npcname_db)
		strdb_final(npcname_db,npcname_db_final);

	// anything else we should cleanup?
	// Reloading npc's now
	ev_db = strdb_init(51);
	npcname_db = strdb_init(24);
	ev_db->release = ev_release;
	npc_warp = npc_shop = npc_script = npc_mob = 0;
	last_npc_id = npc_id;
	
	for (nsl = npc_src_first; nsl; nsl = nsl->next) {
		npc_parsesrcfile(nsl->name);
		printf("\r");
		ShowStatus("Loading NPCs... Working: ");
		if (last_time != time(0)) {
			last_time = time(0);
			switch(busy) {
				case 0: c='\\'; busy++; break;
				case 1: c='|'; busy++; break;
				case 2: c='/'; busy++; break;
				case 3: c='-'; busy=0;
			}
		}
		printf("[%c]",c);
		fflush(stdout);
	}
	printf("\r");
	ShowInfo ("Done loading '"CL_WHITE"%d"CL_RESET"' NPCs:%30s\n\t-'"
		CL_WHITE"%d"CL_RESET"' Warps\n\t-'"
		CL_WHITE"%d"CL_RESET"' Shops\n\t-'"
		CL_WHITE"%d"CL_RESET"' Scripts\n\t-'"
		CL_WHITE"%d"CL_RESET"' Mobs\n",
		npc_id - last_npc_id, "", npc_warp, npc_shop, npc_script, npc_mob);

	return 0;
}
/*==========================================
 * 終了
 *------------------------------------------
 */
int do_final_npc(void)
{
	int i;
	struct block_list *bl;
	struct npc_data *nd;
	struct mob_data *md;
	struct pet_data *pd;

	for (i = START_NPC_NUM; i < npc_id; i++){
		if((bl = map_id2bl(i))){
			if(bl->type == BL_NPC && (nd = (struct npc_data *)bl)){
				npc_unload(nd);
			}else if (bl->type == BL_MOB && (md = (struct mob_data *)bl)){
				if (md->lootitem){
					aFree(md->lootitem);
					md->lootitem = NULL;
				}
				aFree(md);
				md = NULL;
			}else if(bl->type == BL_PET && (pd = (struct pet_data *)bl)){
				aFree(pd);
				pd = NULL;
			}
		}
	}

	if(ev_db)
		strdb_final(ev_db,ev_db_final);
 	if(npcname_db)
		strdb_final(npcname_db,npcname_db_final);

	npc_clearsrcfile();

	return 0;
}

/*==========================================
 * npc初期化
 *------------------------------------------
 */
int do_init_npc(void)
{
	struct npc_src_list *nsl;
	time_t last_time = time(0);
	int busy = 0;
	char c = '-';

	// indoorrswtable.txt and etcinfo.txt [Celest]
	if (battle_config.indoors_override_grffile)
		npc_read_indoors();
	//npc_read_weather();

	// comparing only the first 24 chars of labels that are 50 chars long isn't that nice
	// will cause "duplicated" labels where actually no dup is...
	//ev_db=strdb_init(24); 
	ev_db = strdb_init(51);
	npcname_db = strdb_init(24);
	ev_db->release = ev_release;

	memset(&ev_tm_b, -1, sizeof(ev_tm_b));

	for (nsl = npc_src_first; nsl; nsl = nsl->next) {
		/*if(nsl->prev){ // [Shinomori]
			aFree(nsl->prev);
			nsl->prev = NULL;
		}*/
		//
		npc_parsesrcfile(nsl->name);
		current_file = NULL;
		printf("\r");
		ShowStatus("Loading NPCs... Working: ");
		if (last_time != time(0)) {
			last_time = time(0);
			switch(busy) {
				case 0: c='\\'; busy++; break;
				case 1: c='|'; busy++; break;
				case 2: c='/'; busy++; break;
				case 3: c='-'; busy=0;
			}
		}
		printf("[%c]",c);
		fflush(stdout);
//		printf("\rLoading NPCs [%d]: %-54s",npc_id-START_NPC_NUM,nsl->name);
//		fflush(stdout);
	}
	printf("\r");
	ShowInfo ("Done loading '"CL_WHITE"%d"CL_RESET"' NPCs:%30s\n\t-'"
		CL_WHITE"%d"CL_RESET"' Warps\n\t-'"
		CL_WHITE"%d"CL_RESET"' Shops\n\t-'"
		CL_WHITE"%d"CL_RESET"' Scripts\n\t-'"
		CL_WHITE"%d"CL_RESET"' Mobs\n",
		npc_id - START_NPC_NUM, "", npc_warp, npc_shop, npc_script, npc_mob);
	
	add_timer_func_list(npc_walktimer,"npc_walktimer"); // [Valaris]
	add_timer_func_list(npc_event_timer,"npc_event_timer");
	add_timer_func_list(npc_event_do_clock,"npc_event_do_clock");
	add_timer_func_list(npc_timerevent,"npc_timerevent");

	//exit(1);

	return 0;
}
