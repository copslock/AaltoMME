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
 * @file   S1Assoc_NotConfigured.c
 * @Author Vicent Ferrer
 * @date   August, 2015
 * @brief  S1 Assoc NotConfigured State
 *
 */

#include <glib.h>
#include "S1Assoc_NotConfigured.h"
#include "logmgr.h"
#include "S1Assoc_priv.h"
#include "S1Assoc_FSMConfig.h"
#include "MME_S1_priv.h"
#include "S1AP.h"

static void sendS1SetupReject_UnknownPLMN(S1Assoc_t *assoc);
static void sendS1SetupResponse(S1Assoc_t *assoc);

static void processMsg(gpointer _assoc, S1AP_Message_t *s1msg, int r_sid, GError** err){
    S1Assoc_t *assoc = (S1Assoc_t *)_assoc;
    ENBname_t       *eNBname;
    Global_ENB_ID_t *global_eNB_ID;
    struct mme_t * mme = s1_getMME(assoc->s1);

    assoc->nonue_rsid = r_sid;
    assoc->nonue_lsid = 0;

    /* Check Procedure*/
    if(s1msg->pdu->procedureCode != id_S1Setup &&
       s1msg->choice != initiating_message){
        s1Assoc_log(assoc, LOG_WARNING, 0, "Not a S1-Setup Request but %s, ignoring",
                elementaryProcedureName[s1msg->pdu->procedureCode]);
        return;
    }

    eNBname = s1ap_findIe(s1msg, id_eNBname);              /*OPTIONAL*/
    if(eNBname){
        g_string_assign(assoc->eNBname, eNBname->name);
    }

    global_eNB_ID = s1ap_findIe(s1msg, id_Global_ENB_ID);
    s1Assoc_setGlobalID(assoc, global_eNB_ID);

    assoc->supportedTAs     = s1ap_getIe(s1msg, id_SupportedTAs);
    /* assoc->cGS_IdList      = s1ap_getIe(s1msg, id_CSG_IdList); */

    CHECKIEPRESENCE(global_eNB_ID);
    CHECKIEPRESENCE(assoc->supportedTAs);

    mme_registerS1Assoc(mme, assoc);

    if(!mme_containsSupportedTAs(mme, assoc->supportedTAs)){
        sendS1SetupReject_UnknownPLMN(assoc);
        s1Assoc_log(assoc, LOG_INFO, 0, "S1-Setup Rejected: %s - Unknown PLMN", assoc->eNBname->str);
        g_set_error(err,
                    1,//s1Assoc_quark(),   // error domain
                    1,                 // error code
                    "Received Supported PLMN not available in this MME");
        return;
    }

    s1Assoc_log(assoc, LOG_INFO, 0, "S1-Setup : new eNB \"%s\", connection added", assoc->eNBname->str);
    sendS1SetupResponse(assoc);
    s1ChangeState(assoc, S1_Active);
}


void linkS1AssocNotConfigured(S1Assoc_State* s){
    s->processMsg = processMsg;
}

static void sendS1SetupResponse(S1Assoc_t *assoc){
    S1AP_Message_t *s1msg;
    S1AP_PROTOCOL_IES_t* ie;
    MMEname_t *name;
    ServedGUMMEIs_t *gummeis;
    RelativeMMECapacity_t* cap;

    struct mme_t * mme = s1_getMME(assoc->s1);
    /* Build response*/
    s1msg = S1AP_newMsg();
    s1msg->choice = successful_outcome;
    s1msg->pdu->procedureCode = id_S1Setup;
    s1msg->pdu->criticality = reject;

    /* MME Name (optional)*/
    if(mme->name != NULL){
        name = s1ap_newIE(s1msg, id_MMEname, optional, ignore);
        memcpy(name->name, mme->name->name, strlen(mme->name->name));
    }

    /* Served GUMMEIs*/
    ie=newProtocolIE();
    if(ie == NULL){
        s1Assoc_log(assoc, LOG_ERR, 0, "Coudn't allocate new Protocol IE structure");
    }

    /* gummeis = s1ap_newIE(s1msg, id_ServedGUMMEIs, optional, reject); */
    /* memcpy(gummeis, mme->servedGUMMEIs, sizeof(ServedGUMMEIs_t*)); */
    ie->id = id_ServedGUMMEIs;
    ie->presence = optional;
    ie->criticality = reject;
    ie->value = mme->servedGUMMEIs;
    ie->showValue = mme->servedGUMMEIs->showIE;
    s1msg->pdu->value->addIe(s1msg->pdu->value, ie);

    /* Relative MME Capacity*/
    cap = s1ap_newIE(s1msg, id_RelativeMMECapacity, optional, ignore);
    cap->cap = mme->relativeCapacity->cap;

    /* Send Response*/
    s1Assoc_sendNonUE(assoc, s1msg);

    /*s1msg->showmsg(s1msg);*/

    /* This function doesn't deallocate the IE stored on the mme structure,
     * because the freeValue callback attribute of the ProtocolIE structure is NULL */
    s1msg->freemsg(s1msg);

}


static void sendS1SetupReject_UnknownPLMN(S1Assoc_t *assoc){
    S1AP_Message_t *s1msg;
    struct mme_t * mme = s1_getMME(assoc->s1);
    Cause_t *c;

    /* Build response*/
    s1msg = S1AP_newMsg();
    s1msg->choice = unsuccessful_outcome;
    s1msg->pdu->procedureCode = id_S1Setup;
    s1msg->pdu->criticality = reject;

    /*Cause: Unknown PLMN*/
    c = s1ap_newIE(s1msg, id_Cause, mandatory, reject);
    c->choice = CauseMisc;
    c->cause.misc.cause.noext = CauseMisc_unknown_PLMN;

    /* Send Response*/
    s1Assoc_sendNonUE(assoc, s1msg);

    s1msg->freemsg(s1msg);

}
