/* This application was initially developed as a Final Project by
 *     Vicent Ferrer Guasch (vicent.ferrerguasch@aalto.fi)
 * under the supervision of,
 *     Jukka Manner (jukka.manner@aalto.fi)
 *     Jose Costa-Requena (jose.costa@aalto.fi)
 * in AALTO University and partially funded by EIT ICT labs.
 */

/**
 * @file   S11_User.c
 * @Author Vicent Ferrer
 * @date   June, 2015
 * @brief  S11 User
 *
 */

#include "S11_User.h"
#include "S11_FSMConfig.h"
#include "MME_S11.h"
#include "nodemgr.h"
#include "logmgr.h"

#include <string.h>
#include <netinet/in.h>

#include "gtp.h"

typedef struct{
    gpointer    s11;    /**< s11 stack handler*/
    S11_State *state;  /**< s11 state for this user*/
    uint32_t  lTEID;  /**< local control TEID*/
    uint32_t  rTEID;  /**< remote control TEID*/
    struct sockaddr     rAddr;       /**<Peer IP address, IPv4 or IPv6*/
    socklen_t           rAddrLen;    /**<Peer Socket length returned by recvfrom*/
    struct user_ctx_t *user; /**< User information*/
    union gtp_packet oMsg;
    uint32_t oMsglen;
    union gtp_packet iMsg;
    uint32_t iMsglen;
    union gtpie_member *ie[GTPIE_SIZE];
    uint8_t cause;
    void(*cb)(gpointer);
    gpointer args;
}S11_user_t;

#define PARSE_ERROR parse_error()

GQuark
parse_error (void)
{
  return g_quark_from_static_string ("gtpv2-parse-error");
}

gpointer s11u_newUser(gpointer s11, struct user_ctx_t *user){
    struct nodeinfo_t sgw;
    struct sockaddr_in *peer;

    S11_user_t *self = g_new0(S11_user_t, 1);
    self->lTEID = newTeid();
    self->rTEID = 0;
    self->user  = user;
    self->s11   = s11;

    /*Get SGW addr*/
    getNode(&sgw, SGW, user);
    peer = (struct sockaddr_in *)&(self->rAddr);
    peer->sin_family = AF_INET;
    peer->sin_port = htons(GTP2C_PORT);
    peer->sin_addr.s_addr = sgw.addrv4.s_addr;
    self->rAddrLen = sizeof(struct sockaddr_in);

    /*Initial state noCtx*/
    changeState(self, noCtx);
    return self;
}

void s11u_freeUser(gpointer self){
    g_free(self);
}

static validateSourceAddr(S11_user_t* self, const char * src){
    char r[INET6_ADDRSTRLEN];

    switch(self->rAddr.sa_family){
    case AF_INET:
	    inet_ntop(AF_INET, &(((struct sockaddr_in*)&(self->rAddr))->sin_addr),
	              r, INET_ADDRSTRLEN);
        break;
    case AF_INET6:
	    inet_ntop(AF_INET6, &(((struct sockaddr_in6*)&(self->rAddr))->sin6_addr),
	              r, INET6_ADDRSTRLEN);
        break;
    }
    return strcmp(r, src) == 0;
}

void processMsg(gpointer u, const struct t_message *msg){
    S11_user_t *self = (S11_user_t*)u;

    if(!validateSourceAddr(self, msg->srcAddr)){
        log_msg(LOG_WARNING, 0, "S11 - Wrong S-GW source (%s)."
                "Ignoring packet", msg->srcAddr);
        return;
    }

    self->iMsglen = msg->length;
    memcpy(&(self->iMsg), &(msg->packet.gtp), msg->length);

    self->state->processMsg(self);
}

void attach(gpointer session, void(*cb)(gpointer), gpointer args){
    S11_user_t *self = (S11_user_t*)session;
    self->cb = cb;
    self->args = args;
    self->state->attach(self);
}

void detach(gpointer session, void(*cb)(gpointer), gpointer args){
    S11_user_t *self = (S11_user_t*)session;
    self->cb = cb;
    self->args = args;
    self->state->detach(self);
}

void modBearer(gpointer session, void(*cb)(gpointer), gpointer args){
    S11_user_t *self = (S11_user_t*)session;
    self->cb = cb;
    self->args = args;
    self->state->modBearer(self);
}


gboolean s11u_hasPendingResp(gpointer self){
    return TRUE;
}


int *s11u_getTEIDp(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    return &(self->lTEID);
}

void s11u_setState(gpointer u, S11_State *s){
    S11_user_t *self = (S11_user_t*)u;
    self->state = s;
}

static void s11_send(S11_user_t* self){
	const int sock = s11_fg(self->s11);
	/*Packet header modifications*/
    self->oMsg.gtp2l.h.seq = hton24(getNextSeq(self->s11));
    self->oMsg.gtp2l.h.tei = self->rTEID;


    if (sendto(sock, &(self->oMsg), self->oMsglen, 0,
                   &(self->rAddr), self->rAddrLen) < 0) {
	    log_errpack(LOG_ERR, errno, (struct sockaddr_in *)&(self->rAddr),
	                &(self->oMsg), self->oMsglen,
	                "Sendto(fd=%d, msg=%lx, len=%d) failed",
	                sock, (unsigned long) &(self->oMsg), self->oMsglen);
    }
}


void returnControl(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    self->cb(self->args);
}

void returnControlAndRemoveSession(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    void(*cb)(gpointer);
    gpointer args;
    cb = self->cb;
    args = self->args;
    s11_deleteSession(self->s11, self);
    cb(args);
}

void parseIEs(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    gtp2ie_decap(self->ie, &(self->iMsg), self->iMsglen);
}

const int getMsgType(const gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    return self->iMsg.gtp2l.h.type;
}

void sendCreateSessionReq(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    struct fteid_t  fteid;
    struct qos_t    *qos;
    union gtpie_member ie[14], ie_bearer_ctx[3];
    int hlen, a;
    uint32_t ielen, ienum=0;
    uint8_t bytefield[30], *tmp;
    struct nodeinfo_t pgwInfo;

    log_msg(LOG_DEBUG, 0, "Enter");
    /*  Send Create Context Request to SGW*/

    qos = &(self->user->ebearer->qos);

    self->oMsglen = get_default_gtp(2, GTP2_CREATE_SESSION_REQ, &(self->oMsg));

    /*IMSI*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.t=GTPV2C_IE_IMSI;
    dec2tbcd(ie[ienum].tliv.v, &ielen, self->user->imsi);
    ie[ienum].tliv.l=hton16(ielen);
    ienum++;
    /*MSISDN*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.t=GTPV2C_IE_MSISDN;
    dec2tbcd(ie[ienum].tliv.v, &ielen, self->user->msisdn);
    ie[ienum].tliv.l=hton16(ielen);
    ienum++;
    /*MEI*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.t=GTPV2C_IE_MEI;
    dec2tbcd(ie[ienum].tliv.v, &ielen, self->user->imsi);
    ie[ienum].tliv.l=hton16(ielen);
    ienum++;
    /*RAT type*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(1);
    ie[ienum].tliv.t=GTPV2C_IE_RAT_TYPE;
    ie[ienum].tliv.v[0]=6;                  /*Type 6= EUTRAN*/
    ienum++;
    /*F-TEID*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(9);
    ie[ienum].tliv.t=GTPV2C_IE_FTEID;
    fteid.ipv4=1;
    fteid.ipv6=0;
    fteid.iface= hton8(S11_MME);
    fteid.teid = hton32(self->lTEID);
    inet_pton(AF_INET,
              s11_getLocalAddress(self->s11),
              &(fteid.addr.addrv4));
    ie[ienum].tliv.l=hton16(FTEID_IP4_SIZE);
    memcpy(ie[ienum].tliv.v, &fteid, FTEID_IP4_SIZE);
    ienum++;
    /*F-TEID PGW S5/S8 Address for Control Plane or PMIP */
    ie[ienum].tliv.i=1;
    ie[ienum].tliv.l=hton16(FTEID_IP4_SIZE);
    ie[ienum].tliv.t=GTPV2C_IE_FTEID;
    fteid.ipv4=1;
    fteid.ipv6=0;
    fteid.iface= hton8(S5S8C_PGW);
    fteid.teid = hton32(0);
    getNode(&pgwInfo, PGW, self->user);
    fteid.addr.addrv4 = pgwInfo.addrv4.s_addr;
    memcpy(ie[ienum].tliv.v, &fteid, FTEID_IP4_SIZE);
    ienum++;
    /*APN*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(strlen(self->user->aPname));
    ie[ienum].tliv.t=GTPV2C_IE_APN;
    sprintf(ie[ienum].tliv.v, self->user->aPname, strlen(self->user->aPname));
    ienum++;

    /*PAA*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(5);
    ie[ienum].tliv.t=GTPV2C_IE_PAA;
    bytefield[0]=0x01;  /*PDN Type  IPv4 */
    memset(bytefield+1, 0, 4);   /*IP = 0.0.0.0*/
    memcpy(ie[ienum].tliv.v, bytefield, 5);
    ienum++;
    /*Serving Network*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(3);
    ie[ienum].tliv.t=GTPV2C_IE_SERVING_NETWORK;
    memcpy(ie[ienum].tliv.v, self->user->tAI.sn, 3);
    ienum++;
    /*PDN type*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(1);
    ie[ienum].tliv.t=GTPV2C_IE_PDN_TYPE;
    bytefield[0]=self->user->pdn_type; /* PDN type IPv4*/
    memcpy(ie[ienum].tliv.v, bytefield, 1);
    ienum++;
    /*APN restriction*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(1);
    ie[ienum].tliv.t=GTPV2C_IE_APN_RESTRICTION;
    bytefield[0]=0x00; /* APN restriction*/
    memcpy(ie[ienum].tliv.v, bytefield, 1);
    ienum++;
    /*Selection Mode*/
    ie[ienum].tliv.i=0;
    ie[ienum].tliv.l=hton16(1);
    ie[ienum].tliv.t=GTPV2C_IE_SELECTION_MODE;
    bytefield[0]=0x01; /* Selection Mode*/
    memcpy(ie[ienum].tliv.v, bytefield, 1);
    ienum++;

    /*Protocol Configuration Options*/
    if(self->user->pco[0]==0x27){
        ie[ienum].tliv.i=0;
        ie[ienum].tliv.l=hton16(self->user->pco[1]);
        ie[ienum].tliv.t=GTPV2C_IE_PCO;
        tmp = self->user->pco+2;
        memcpy(ie[ienum].tliv.v, tmp, self->user->pco[1]);
        ienum++;
    }
    /*Bearer contex*/
        /*EPS Bearer ID */
        ie_bearer_ctx[0].tliv.i=0;
        ie_bearer_ctx[0].tliv.l=hton16(1);
        ie_bearer_ctx[0].tliv.t=GTPV2C_IE_EBI;
        /*EBI = 5,  EBI > 4, see 3GPP TS 24.007 11.2.3.1.5  EPS bearer identity */
        ie_bearer_ctx[0].tliv.v[0]=0x05;
        /* Bearer QoS */
        ie_bearer_ctx[1].tliv.i=0;
        ie_bearer_ctx[1].tliv.l=hton16(sizeof(struct qos_t));
        ie_bearer_ctx[1].tliv.t=GTPV2C_IE_BEARER_LEVEL_QOS;
        memcpy(ie_bearer_ctx[1].tliv.v, qos, sizeof(struct qos_t));
        /*EPS Bearer TFT */
        /*ie_bearer_ctx[2].tliv.i=0;
        ie_bearer_ctx[2].tliv.l=hton16(3);
        ie_bearer_ctx[2].tliv.t=GTPV2C_IE_BEARER_TFT;
        bytefield[0]=0x01;
        bytefield[1]=0x01;
        bytefield[2]=0x01;
        memcpy(ie_bearer_ctx[2].tliv.v, bytefield, 3);
    gtp2ie_encaps_group(GTPV2C_IE_BEARER_CONTEXT, 0, &ie[12], ie_bearer_ctx, 3);*/
    gtp2ie_encaps_group(GTPV2C_IE_BEARER_CONTEXT, 0, &ie[ienum],
                        ie_bearer_ctx, 2);
    ienum++;
    gtp2ie_encaps(ie, ienum, &(self->oMsg), &(self->oMsglen));
    s11_send(self);
}

void sendModifyBearerReq(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    struct fteid_t  fteid;
    union gtpie_member ie[13], ie_bearer_ctx[3];
    int hlen, a;
    uint32_t fteid_size;

    log_msg(LOG_DEBUG, 0, "Enter");
    /*  Send Create Context Request to SGW*/
    /******************************************************************************/

    self->oMsglen = get_default_gtp(2, GTP2_MODIFY_BEARER_REQ, &(self->oMsg));

    /*F-TEID*/
    ie[0].tliv.i=0;
    ie[0].tliv.t=GTPV2C_IE_FTEID;
    ie[0].tliv.l=hton16(FTEID_IP4_SIZE);
    fteid.ipv4=1;
    fteid.ipv6=0;
    fteid.iface= hton8(S11_MME);
    fteid.teid = hton32(self->user->S11MMETeid);
    inet_pton(AF_INET,
              s11_getLocalAddress(self->s11),
              &(fteid.addr.addrv4));
    memcpy(ie[0].tliv.v, &fteid, FTEID_IP4_SIZE);

    /*Bearer contex*/
        /*EPS Bearer ID */
        ie_bearer_ctx[0].tliv.i=0;
        ie_bearer_ctx[0].tliv.l=hton16(1);
        ie_bearer_ctx[0].tliv.t=GTPV2C_IE_EBI;
        ie_bearer_ctx[0].tliv.v[0]=self->user->ebearer[0].id;
        /* fteid S1-U eNB*/
        memcpy(&fteid, &(self->user->ebearer[0].s1u_eNB), sizeof(struct fteid_t));
        ie_bearer_ctx[1].tliv.i=0;
        ie_bearer_ctx[1].tliv.t=GTPV2C_IE_FTEID;
        if(fteid.ipv4 == 1 && fteid.ipv6 == 0){
            fteid_size = FTEID_IP4_SIZE;
        }else if (fteid.ipv4 == 0 && fteid.ipv6 == 1){
            fteid_size = FTEID_IP6_SIZE;
        }else{
            fteid_size = FTEID_IP46_SIZE;
        }
        ie_bearer_ctx[1].tliv.l=hton16(fteid_size);
        memcpy(ie_bearer_ctx[1].tliv.v, &fteid, fteid_size);
    gtp2ie_encaps_group(GTPV2C_IE_BEARER_CONTEXT, 0, &ie[1], ie_bearer_ctx, 2);
    gtp2ie_encaps(ie, 2, &(self->oMsg), &(self->oMsglen));

    s11_send(self);
}

void sendDeleteSessionReq(gpointer u){
    S11_user_t *self = (S11_user_t*)u;

    union gtpie_member ie[13];

    /*  Send Delete Session Request to SGW*/
    /******************************************************************************/

    self->oMsglen = get_default_gtp(2, GTP2_DELETE_SESSION_REQ, &(self->oMsg));

    /*  EPS Bearer ID (EBI) to be removed*/
    ie[0].tliv.i=0;
    ie[0].tliv.l=hton16(1);
    ie[0].tliv.t=GTPV2C_IE_EBI;
    /*ie[0].tliv.v[0]=user.ebi; *//*Future*/
    ie[0].tliv.v[0]=0x05; /*EBI = 5,  EBI > 4, see 3GPP TS 24.007 11.2.3.1.5  EPS bearer identity */

    gtp2ie_encaps(ie, 1, &(self->oMsg), &(self->oMsglen));

    s11_send(self);
}

const gboolean accepted(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    uint16_t vsize;
    uint8_t value[2];

    /* Cause*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_CAUSE, 0, value, &vsize);
    if(vsize=2){
        self->cause = value[0];
        return value[0]==GTPV2C_CAUSE_REQUEST_ACCEPTED;
    }else{
        log_msg(LOG_ERR, 0, "GTPv2 Cause IE Parse Error");
        return;
    }
}

const int cause(gpointer u){
    S11_user_t *self = (S11_user_t*)u;
    uint16_t vsize;
    uint8_t value[2];

    gtp2ie_gettliv(self->ie, GTPV2C_IE_CAUSE, 0, value, &vsize);
    if(vsize=2){
        return value[0];
    }else{
        log_msg(LOG_ERR, 0, "GTPv2 Cause IE Parse Error");
        return;
    }
}

void parseCtxRsp(gpointer u, GError **err){
    S11_user_t *self = (S11_user_t*)u;
    union gtpie_member *bearerCtxGroupIE[GTPIE_SIZE];
    uint8_t value[40];
    uint16_t vsize;
    uint32_t numIE;
    struct fteid_t fteid;
    struct in_addr s1uaddr;

    /* F-TEID S11 (SGW)*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_FTEID, 0, value, &vsize);
    if(value!= NULL && vsize>0){
	    /* memcpy(&(self->user->s11), value, vsize); */
	    s11u_setS11fteid(self, value);
        log_msg(LOG_DEBUG, 0, "S11 Sgw teid = %x into", hton32(self->rTEID));
    }


    /* F-TEID S5 /S8 (PGW)*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_FTEID, 1, value, &vsize);
    if(value!= NULL && vsize>0){
        memcpy(&(self->user->s5s8), value, vsize);
        log_msg(LOG_DEBUG, 0, "S5/S8 Pgw teid = %x into", hton32(self->user->s5s8.teid));
    }

    /* PDN Address Allocation - PAA*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_PAA, 0, value, &vsize);
    if(value!= NULL && vsize>0){
        memcpy(&(self->user->pAA), value, vsize);
        log_msg(LOG_DEBUG, 0, "PDN Allocated Addr type %u", self->user->pAA.type);
    }
    /* APN Restriction*/

    vsize=0;
    /* Protocol Configuration Options PCO*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_PCO, 0, value, &vsize);
    if(value!= NULL && vsize>0){
        memcpy(&(self->user->pco[2]), value, vsize);
        self->user->pco[1]= (uint8_t) ntoh16(vsize);
        log_msg(LOG_DEBUG, 0, "PDN Allocated Addr type %u", self->user->pAA.type);
    }

    /* Bearer Context*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_BEARER_CONTEXT, 0, value, &vsize);
    if(value!= NULL && vsize>0){
        gtp2ie_decaps_group(bearerCtxGroupIE, &numIE, value, vsize);

        /* EPS Bearer ID*/
        gtp2ie_gettliv(bearerCtxGroupIE, GTPV2C_IE_EBI, 0, value, &vsize);
        memcpy(&(self->user->ebearer[0].id), value, vsize);
        log_msg(LOG_DEBUG, 0, "EPC Bearer ID = %u", self->user->ebearer[0].id);

        /* F-TEID S1-U (SGW)*/
        gtp2ie_gettliv(bearerCtxGroupIE, GTPV2C_IE_FTEID, 0, value, &vsize);
        if(value!= NULL && vsize>0){
            memcpy(&(self->user->ebearer[0].s1u_sgw), value, vsize);
            s1uaddr.s_addr = self->user->ebearer[0].s1u_sgw.addr.addrv4;
            log_msg(LOG_DEBUG, 0, "S1-u Sgw teid = %x, ip = %s", hton32(self->user->ebearer[0].s1u_sgw.teid), inet_ntoa(s1uaddr));
        }

        /* F-TEID S5/S8-U(PGW)*/
        gtp2ie_gettliv(bearerCtxGroupIE, GTPV2C_IE_FTEID, 1, value, &vsize);
        if(value!= NULL && vsize>0){
            memcpy(&(self->user->ebearer[0].s5s8u), value, vsize);
            log_msg(LOG_DEBUG, 0, "S5/S8 Pgw teid = %x into", hton32(self->user->ebearer[0].s5s8u.teid));
        }else{

        }
    }
}

void parseModBearerRsp(gpointer u, GError **err){
    S11_user_t *self = (S11_user_t*)u;
    union gtpie_member *bearerCtxGroupIE[GTPIE_SIZE];
    uint8_t value[40];
    uint16_t vsize;
    uint32_t numIE;
    struct fteid_t fteid;

    /*  TODO @Vicent Check message mandatory IE*/
    log_msg(LOG_DEBUG, 0, "Parsing Modify Bearer Req");

    /* Bearer Context*/
    gtp2ie_gettliv(self->ie, GTPV2C_IE_BEARER_CONTEXT, 0, value, &vsize);
    if(value!= NULL && vsize>0){
        gtp2ie_decaps_group(bearerCtxGroupIE, &numIE, value, vsize);

        /* EPS Bearer ID*/
        gtp2ie_gettliv(bearerCtxGroupIE, GTPV2C_IE_EBI, 0, value, &vsize);
        if(vsize != 1 && self->user->ebearer[0].id != *value){
            g_set_error (err,
                         PARSE_ERROR,                 // error domain
                         1,            // error code
                         "EPC Bearer ID %u != %u received",
                         self->user->ebearer[0].id, *value);
            return;
        }

        /* F-TEID S1-U (SGW)*/
        gtp2ie_gettliv(bearerCtxGroupIE, GTPV2C_IE_FTEID, 0, value, &vsize);
        if(value!= NULL && vsize>0){
            memcpy(&fteid, value, vsize);
            if(memcmp(&(self->user->ebearer[0].s1u_sgw), value, vsize) != 0){
                g_set_error (err,
                             PARSE_ERROR,                 // error domain
                             1,            // error code
                             "S1-U SGW FEID not corresponds to the stored one");
                return;
            }
        }
    }
}

void parseDelCtxRsp(gpointer u, GError **err){
    S11_user_t *self = (S11_user_t*)u;

    log_msg(LOG_DEBUG, 0, "Parsing Delete Session RSP.");

    /*  Delete node info*/
    self->rTEID = 0;
}


void s11u_setS11fteid(gpointer u, gpointer fteid_h){
	struct fteid_t* fteid = (struct fteid_t*) fteid_h;
	S11_user_t *self = (S11_user_t*)u;

	if(fteid->iface != S11S4_SGW)
		g_error("Unexpected interface type, S11S4_SGW expected");

	/*TODO validate addr*/

	self->rTEID = fteid->teid;
}