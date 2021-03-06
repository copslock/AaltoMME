/* AaltoMME - Mobility Management Entity for LTE networks
 * Copyright (C) 2013 Vicent Ferrer Guash & Jesus Llorente Santos
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   MME.h
 * @Author Vicent Ferrer
 * @date   March, 2013
 * @brief  MME type definition and functions.
 * @modifiedby Jesus Llorente, Todor Ginchev
 * @lastmodified 9 October 2017
 *
 * The goal of this file is to define the generic functions of the MME and its interfaces.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netinet/sctp.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <glib.h>

#include "logmgr.h"
#include "gtp.h"
#include "commands.h"
#include "MME.h"
#include "MME_S1.h"
#include "MME_S11.h"
#include "MME_S6a.h"
#include "MME_Controller.h"
#include "HSS.h"
#include "MMEutils.h"
#include "S1Assoc.h"
#include "ECMSession.h"
#include "NAS_EMM.h"
#include "nodemgr.h"


/**@brief Simple UDP creation
 * @param [in] port server UDP port
 * @returns file descriptor*/
int init_udp_srv(const char *src, int port){
    int fd;
    struct sockaddr_in addr;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        log_msg(LOG_ERR, errno, "socket(domain=%d, type=%d, protocol=%d) failed", AF_INET, SOCK_DGRAM, 0);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, src, &(addr.sin_addr));

    addr.sin_port = htons(port);
    #if defined(__FreeBSD__) || defined(__APPLE__)
    addr.sin_len = sizeof(addr);
    #endif

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        log_msg(LOG_ERR, errno, "bind(fd=%d, addr=%lx, len=%d) failed", fd, (unsigned long) &addr, sizeof(addr));
        return -1;
    }
    return fd;
}

/**@brief Simple SCTP creation
 * @param [in] port server SCTP port
 * @returns file descriptor*/
int init_sctp_srv(const char *src, int port){
    int listenSock, on=0, status, optval;
    struct sockaddr_in servaddr;

    struct sctp_initmsg initmsg;


    if ((listenSock =  socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP ) )< 0 ) {
        log_msg(LOG_ERR, errno, "socket(domain=%d, type=%d, protocol=%d) failed", AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        return -1;
    }

    /* Accept connections from any interface */
    bzero( (void *)&servaddr, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, src, &(servaddr.sin_addr));
    /* servaddr.sin_addr.s_addr = addr; */
    servaddr.sin_port = htons(port);

    /* Enable socket address reuse to avoid binding issues */
    int enable = 1;
    if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        log_msg(LOG_ERR, errno, "setsockopt(fd=%d) to turn off bind address checking failed. ", listenSock);
        return -1;
    }


    if (bind( listenSock, (struct sockaddr *)&servaddr, sizeof(servaddr) ) < 0) {
        log_msg(LOG_ERR, errno, "bind(fd=%d, addr=%lx, len=%d) failed", listenSock, (unsigned long) &servaddr, sizeof(servaddr));
        return -1;
    }

    /* Specify that a maximum of 5 streams will be available per socket */
    memset( &initmsg, 0, sizeof(initmsg) );
    initmsg.sinit_num_ostreams = 2;
    initmsg.sinit_max_instreams = 2;
    initmsg.sinit_max_attempts = 4;
    if ( setsockopt( listenSock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg) )< 0) {
        log_msg(LOG_ERR, errno, "setsockopt error");
        return -1;
    }

    /* Disable Nagle algorithm */
    optval=1;
    if (setsockopt(listenSock, IPPROTO_SCTP, SCTP_NODELAY, (void*)&optval, sizeof(optval)) ==-1){
        log_msg(LOG_WARNING, errno, "Couldn't set SCTP_NODELAY socket option.");
        /* No critical*/
    }

    /* Place the server socket into the listening state */
    if (listen( listenSock, 5 )< 0) {
        log_msg(LOG_ERR, errno, "listen(fd=%d, 5) failed", listenSock);
        return -1;
    }

    return listenSock;
}

bool mme_init_ifaces(struct mme_t *self){

    /*LibEvent structures*/
    struct event_base *base;
    uint32_t addr = 0;

    self->s6a = s6a_init((gpointer)self);
    if(!(self->s6a)){
        return false;
    }

    /*Init command server, returns server file descriptor*/
    self->cmd = servcommand_init(self, COMMAND_PORT);
    if(!(self->cmd)){
        goto err_cmd;
    }

    /*Init S11 server*/
    self->s11 = s11_init((gpointer)self);
    if(!(self->s11)){
        goto err_s11;
    }

    /*Init S1 server*/
    self->s1 = s1_init((gpointer)self);
    if(!(self->s1)){
        goto err_s1;
    }

    /*Init Controller server*/
    self->sdnCtrl = sdnCtrl_init((gpointer)self);
    if(!(self->sdnCtrl)){
        goto err_sdnCtrl;
    }
    return true;

 err_sdnCtrl:
    sdnCtrl_free(self->sdnCtrl);
 err_s1:
    s1_free(self->s1);
 err_s11:
    s11_free(self->s11);
 err_cmd:
    s6a_free(self->s6a);
    return false;
}

void mme_close_ifaces(struct mme_t *self){
    sdnCtrl_free(self->sdnCtrl);
    s6a_free(self->s6a);
    servcommand_stop(self->cmd);
    s11_free(self->s11);
    s1_free(self->s1);
}


void mme_registerRead(struct mme_t *self, int fd, event_callback_fn cb, void * args){
    struct event *ev;
    int *_fd = g_new(gint, 1);
    *_fd = fd;
    log_msg(LOG_DEBUG, 0, "ENTER, fd %u", fd);
    ev = event_new(self->evbase, fd, EV_READ|EV_PERSIST, (event_callback_fn)cb, args);
    evutil_make_socket_nonblocking(fd);
    event_add(ev, NULL);
    g_hash_table_insert(self->ev_readers, _fd, ev);
}

void test_lprint(gpointer data, gpointer user){
    log_msg(LOG_DEBUG, 0, "Key: fd %u", *(int*)data);
}

void mme_deregisterRead(struct mme_t *self, int fd){
    if(g_hash_table_remove(self->ev_readers, &fd) != TRUE){
        log_msg(LOG_ERR, 0, "Unable to find read event, fd %u", fd);
        GList * l = g_hash_table_get_keys(self->ev_readers);
        g_list_foreach(l, test_lprint, NULL);
        g_list_free(l);
    }
}

struct event_base *mme_getEventBase(struct mme_t *self){
    return self->evbase;
}

struct t_message *newMsg(){
    struct t_message *msg;
    msg = malloc(sizeof(struct t_message));
    memset(msg, 0, sizeof(struct t_message));
    return msg;
}

void freeMsg(void *msg){
    free((struct t_message *)msg);
}

unsigned int newTeid(){
    static uint32_t i = 1;
    return i++;
}

uint32_t mme_newLocalUEid(struct mme_t *self){
    guint32 *i = g_new0(guint32, 1);
    for (*i=1; *i<=MAX_UE ;(*i)++){
        if(!g_hash_table_contains(self->s1_localIDs, i)){
            g_hash_table_add(self->s1_localIDs, i);
            log_msg(LOG_DEBUG, 0, "MME S1AP UE ID %u Chosen", *i);
            return *i;
        }
        /* log_msg(LOG_DEBUG, 0, "MME S1AP UE ID %u exists already", *i); */
    }
    g_free(i);
    log_msg(LOG_ERR, 0, "Maximum number of UE (%u) reached", *i);
    return 0;
}

void mme_freeLocalUEid(struct mme_t *self, uint32_t id){
    if(!g_hash_table_remove(self->s1_localIDs, &id)){
        log_msg(LOG_ERR, 0, "MME UE S1AP ID (%u) to be free not found", id);
    }
}

const ServedGUMMEIs_t *mme_getServedGUMMEIs(const struct mme_t *mme){
     return mme->servedGUMMEIs;
 }

const char *mme_getLocalAddress(const struct mme_t *mme){
    return mme->ipv4;
}

const char *mme_getStateDir(const struct mme_t *mme){
    return mme->stateDir;
}

TimerMgr mme_getTimerMgr(struct mme_t *self){
    return self->tm;
}

void mme_registerS1Assoc(struct mme_t *self, gpointer assoc){
    g_hash_table_insert(self->s1_by_GeNBid, s1Assoc_getID_p(assoc), assoc);
}

void mme_deregisterS1Assoc(struct mme_t *self, gpointer assoc){
    if(g_hash_table_remove(self->s1_by_GeNBid, s1Assoc_getID_p(assoc)) != TRUE){
        log_msg(LOG_ERR, 0, "Unable to find S1 Assoction");
    }
}


void mme_lookupS1Assoc(struct mme_t *self, gconstpointer geNBid, gpointer *assoc){
    *assoc = g_hash_table_lookup(self->s1_by_GeNBid, geNBid);
}


void mme_registerEMMCtxt(struct mme_t *self, gpointer emm){
    g_hash_table_insert(self->emm_sessions,
                        emm_getM_TMSI_p(emm),
                        emm);
}

void mme_deregisterEMMCtxt(struct mme_t *self, gpointer emm){
    if(g_hash_table_remove(self->emm_sessions, emm_getM_TMSI_p(emm)) != TRUE){
        log_msg(LOG_ERR, 0, "Unable to find EMM session");
    }
}

void mme_lookupEMMCtxt(struct mme_t *self, const guint32 m_tmsi, gpointer *emm){
    *emm = g_hash_table_lookup(self->emm_sessions, &m_tmsi);
}

void mme_lookupEMMCtxt_byIMSI(struct mme_t *self, const guint64 imsi, gpointer *emm){
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, self->emm_sessions);
    while(g_hash_table_iter_next(&iter, &key, &value)){
        if(emm_getIMSI(value)==imsi){
            *emm = value;
            return;
        }
    }
}

void mme_registerECM(struct mme_t *self, gpointer ecm){
    g_hash_table_insert(self->ecm_sessions_by_localID,
                        ecmSession_getMMEUEID_p(ecm),
                        ecm);
}

void mme_deregisterECM(struct mme_t *self, gpointer ecm){
    if(g_hash_table_remove(self->ecm_sessions_by_localID,
                           ecmSession_getMMEUEID_p(ecm)) != TRUE){
        log_msg(LOG_ERR, 0, "Unable to find ECM session");
    }
}

void mme_lookupECM(struct mme_t *self, const guint32 id, gpointer *ecm){
    *ecm = g_hash_table_lookup(self->ecm_sessions_by_localID, &id);
}

void mme_paging(struct mme_t *self, gpointer emm){
    /* TODO paging*/
    GHashTableIter iter;
    gpointer assoc;

    g_hash_table_iter_init (&iter, self->s1_by_GeNBid);
    while (g_hash_table_iter_next (&iter, NULL, &assoc)){
        s1Assoc_paging(assoc, emm);
    }
}

GList *mme_getS1Assocs(struct mme_t *self){
    return g_hash_table_get_values(self->s1_by_GeNBid);
}

gpointer mme_getS6a(struct mme_t *self){
    return self->s6a;
}

gpointer mme_getS11(struct mme_t *self){
    return self->s11;
}

gboolean mme_GUMMEI_IsLocal(const struct mme_t *self,
                            const guint32 plmn,
                            const guint16 mmegi,
                            const guint8 mmec){
    return TRUE;
}

gboolean mme_containsSupportedTAs(const struct mme_t *self, SupportedTAs_t *tas){
    int i, j, k, l;
    BPLMNs_t *bc_l;
    PLMNidentity_t *plmn_eNB, *plmn_MME;
    ServedPLMNs_t *served;
    gboolean flag = false;


    for(i=0; i<tas->size; i++){
        bc_l = tas->item[i]->broadcastPLMNs;
        for(j=0; j<bc_l->n ; j++){
            plmn_eNB = bc_l->pLMNidentity[j];
            for(k=0; k<self->servedGUMMEIs->size; k++){
                served = self->servedGUMMEIs->item[k]->servedPLMNs;
                for(l=0; l<served->size ; l++){
		    guint8 plmn_mme_printable [7] = {0};
		    guint8 plmn_eNB_printable [7] = {0};
		    plmn_MME = served->item[l];
		    plmn_FillPLMNFromTBCD (plmn_mme_printable, plmn_MME->tbc.s);
		    plmn_FillPLMNFromTBCD (plmn_eNB_printable, plmn_eNB->tbc.s);
                    log_msg(LOG_DEBUG, 0, "Comparing SupportedTA with ServedGUMMEIs"
                            " PLMN %s <=> %s",
                            plmn_mme_printable, plmn_eNB_printable );
		    if(memcmp(plmn_MME->tbc.s, plmn_eNB->tbc.s, 3)==0){
                        log_msg(LOG_INFO, 0, "Found compatible PLMN %s",
                            plmn_mme_printable);
		        flag = TRUE;
                    }
                }
            }
        }
    }
    return flag;
}


static void mme_stopEMM(gpointer mtmsi,
                        gpointer emm,
                        gpointer mme){
    struct mme_t *self = (struct mme_t *)mme;
    log_msg(LOG_DEBUG, 0, "Stoping EMM");
    emm_stop(emm);
}

static gboolean mme_disconnectAssoc(gpointer geNBid,
                                    gpointer assoc,
                                    gpointer mme){
    struct mme_t *self = (struct mme_t *)mme;
    log_msg(LOG_INFO, 0, "Removing S1 Association with eNB \"%s\"",
            s1Assoc_getName(assoc));
    mme_deregisterRead(mme, s1Assoc_getfd(assoc));
    s1Assoc_disconnect(assoc);
    return TRUE;
}

void mme_stop(MME mme){
    struct mme_t *self = (struct mme_t *)mme;
    struct timeval exit_time;

    g_hash_table_foreach(self->emm_sessions, mme_stopEMM, self);
    g_hash_table_foreach_remove(self->s1_by_GeNBid, mme_disconnectAssoc, self);
    /* event_base_loopbreak(self->evbase); */
    exit_time.tv_sec = 5;
    exit_time.tv_usec = 0;
    event_base_loopexit(self->evbase, &exit_time);
}

void kill_handler(evutil_socket_t listener, short event, void *arg){
    struct mme_t *mme = (struct mme_t *)arg;
    log_msg(LOG_INFO, 0, "SIGINT detected. Closing MME");
    mme_stop(mme);
}

MME mme_init(struct event_base *evbase){
    struct mme_t *self;
    GError *err = NULL;


    if (!evbase){
        g_error("The libevent event-base parameter is NULL");
        return NULL;
    }

    self = malloc(sizeof(struct mme_t));
    if(self==NULL){
        g_error("Unable to allocate MME memory");
        return NULL;
    }
    memset(self, 0, sizeof(struct mme_t));

    /*Assign event base structure*/
    self->evbase = evbase;

    /* New timer manager */
    self->tm = init_timerMgr(self->evbase);

    /*Create event for processing SIGINT*/
    self->kill_event = evsignal_new(self->evbase, SIGINT, kill_handler, self);
    event_add(self->kill_event, NULL);

    /*Init node manager*/
    if(!init_nodemgr()){
        goto err1;
    }

    /*Load MME information from config file*/
    loadMMEinfo(self, &err);
    if(err){
        log_msg(LOG_ERR, 0, err->message);
        g_error_free(err);
        goto err1;
    }

    self->ev_readers =
        g_hash_table_new_full(g_int_hash,
                              g_int_equal,
                              g_free,
                              (GDestroyNotify) event_free);
    self->s1_localIDs =
        g_hash_table_new_full(g_int_hash,
                              g_int_equal,
                              g_free,
                              NULL);
    self->ecm_sessions_by_localID =
        g_hash_table_new_full(g_int_hash,
                              g_int_equal,
                              NULL,
                              (GDestroyNotify)ecmSession_free);
    self->emm_sessions =
        g_hash_table_new_full(g_int_hash,
                              g_int_equal,
                              NULL,
                              (GDestroyNotify) emm_free);
    self->s1_by_GeNBid =
        g_hash_table_new_full((GHashFunc)  globaleNBID_Hash,
                              (GEqualFunc) globaleNBID_Equal,
                              NULL,
                              NULL);
    if(!mme_init_ifaces(self)){
        goto err_ifaces;
    };
    return self;

 err_ifaces:
    g_hash_table_destroy(self->s1_by_GeNBid);
    g_hash_table_destroy(self->emm_sessions);
    g_hash_table_destroy(self->ecm_sessions_by_localID);
    g_hash_table_destroy(self->s1_localIDs);
    g_hash_table_destroy(self->ev_readers);
    freeMMEinfo(self);

 err1:
    event_free(self->kill_event);
    free_timerMgr(self->tm);
    free(self);

    return NULL;
}

void mme_free(MME mme){
    struct mme_t *self = (struct mme_t *)mme;
    mme_close_ifaces(self);

    event_free(self->kill_event);

    g_hash_table_destroy(self->s1_by_GeNBid);
    g_hash_table_destroy(self->emm_sessions);
    g_hash_table_destroy(self->ecm_sessions_by_localID);
    g_hash_table_destroy(self->s1_localIDs);
    g_hash_table_destroy(self->ev_readers);

    freeMMEinfo(self);
    free_nodemgr();
    free_timerMgr(self->tm);

    free(self);
}

void mme_main(){
    char *logLvl = NULL;
    int lvl=0;
    /*Create event base structure*/
    struct event_base *evbase = event_base_new();

    /*Init syslog entity*/
    logLvl = getenv("MME_LOGLEVEL");
    if(!logLvl){
        init_logger("MME", LOG_INFO);
        log_msg(LOG_INFO, 0, "MME_LOGLEVEL not set. "
                "Using default log level INFO");
    }else {
        lvl = atoi(logLvl);
        if(lvl<1 || lvl>7){
            init_logger("MME", LOG_INFO);
            log_msg(LOG_INFO, 0, "MME_LOGLEVEL with invalid value %d. "
                    "Default log level INFO set", lvl);
        }else{
            init_logger("MME", lvl);
        }
    }

    log_msg(LOG_INFO, 0, "running MME v%s", MME_VERSION);

    MME mme = mme_init(evbase);
    if(!mme){
        log_msg(LOG_ERR, 0, "MME initialization error");
        goto free_event_base;
    }
    /* Blocking loop*/
    event_base_dispatch(evbase);

    /*Free structures*/
    mme_free(mme);

 free_event_base:
    event_base_free(evbase);

    /*Close syslog entity*/
    close_logger();
}
