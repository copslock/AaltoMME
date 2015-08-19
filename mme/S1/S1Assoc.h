/* This application was initially developed as a Final Project by
 *     Vicent Ferrer Guasch (vicent.ferrerguasch@aalto.fi)
 * under the supervision of,
 *     Jukka Manner (jukka.manner@aalto.fi)
 *     Jose Costa-Requena (jose.costa@aalto.fi)
 * in AALTO University and partially funded by EIT ICT labs.
 */

/**
 * @file   ECMSession.h
 * @Author Vicent Ferrer
 * @date   August, 2015
 * @brief  ECM logic
 *
 * This Module implements the ECM session logic, the user associated messages
 * of S1AP
 */

#ifndef S1ASSOC_HFILE
#define S1ASSOC_HFILE

#include <glib.h>

#include "MME.h"
#include "MME_S1.h"
#include "S1AP.h"

typedef gpointer S1Assoc;

/* API to MME_S1 */
S1Assoc s1Assoc_init(S1 s1);

void s1Assoc_free(gpointer h);

/**@brief Accept a new eNB sctp association
 * @param [in] h  S1 association handler
 * @param [in] ss SCTP Server Socket
 *
 * Accept a new SCTP connection towards a eNB and store the
 * information on the handler
 */
void s1Assoc_accept(S1Assoc h, int ss);


/* ************************************************** */
/*                      Accessors                     */
/* ************************************************** */

/**@brief Get pointer to file descriptor
 * @param [in] h  S1 association handler
 * @return file descriptor pointer
 * 
 * Function used to get the internal address of the fd, used as a key
 */
int *s1Assoc_getfd_p(const S1Assoc h);


/**@brief Get file descriptor
 * @param [in] h  S1 association handler
 * @return file descriptor
 * 
 * Function used to get the internal file descriptor
 */
const int s1Assoc_getfd(const S1Assoc h);

#endif /* S1ASSOC_HFILE */