/*******************************************************************************
* 
* Copyright (c) 2017, Guralp Systems Limited. All rights reserved.
* 
* The following are the licensing terms and conditions (the “License Agreement”)
* under which Guralp Systems Limited (“GSL”) grants access to, use of and
* redistribution of the “Code” (as defined below) to a recipient (the “Licensee”).
* Any use or redistribution of the Code by the Licensee shall be deemed to be
* acceptance of the terms and conditions of this License Agreement. In the event
* of inconsistency or conflict between this License Agreement and any other
* license for the Code then the terms of this License Agreement shall prevail.
* 
* The Code is defined as each and every file in any previous or current
* distribution of the source-code and compiled executables comprising the gdi2ew
* distributable (inclusive of all supporting and embedded documentation) and
* subsequent releases thereof as may be made available by GSL from time to time.
* 
* 1. The License. GSL grants to the Licensee (and Sub-Licensee if applicable)
* a non-exclusive perpetual (subject to termination by GSL in accordance with
* paragraph 4) license (the “License”) to use (“Use”) the Code either alone or
* in conjunction with other code to produce one or more applications (each a
* Derived Product) and/or redistribute the Code or Derived Product
* (Redistribution”) to a third party (each being a “Sub-Licensee”), in each case
* strictly in accordance with the terms and conditions of this License Agreement.  
* 
* 2. Redistribution Conditions. Redistribution and Use of the Code, with or
* without modification, is permitted under the terms of this License Agreement
* provided that the following conditions are met by the Licensee and any Sub
* Licensee: 
* 
* a) Redistribution of the Code must include within the documentation and/or
* other materials provided with the Redistribution the copyright notice
* “Copyright ©2017, Guralp Systems Limited. All rights reserved”.
* 
* b) The Licensee and any Sub-Licensee is responsible for ensuring that any
* party to whom the Code is redistributed is bound by the terms of this License
* as a “Sub-Licensee” and will therefore make Use of the Code on the basis of
* understanding and accepting this Licence Agreement.
* 
* c) Neither the name of Guralp Systems, nor the Guralp logo, nor the names of
* GSL’s contributors may be used to endorse or promote products derived from the
* Code without specific prior written permission from GSL.
* 
* d) Neither the Licensee nor any Sub-Licensee may charge any form of fee or
* royalty for providing the Code to a third party other than as embedded as a
* proportionate element of the fee or royalty charged for a Derived Product.
* 
* e) A Licensee or Sub-licensee may charge a fee or royalty for a Derived
* Product.  
* 
* 3. DISCLAIMER. EXCEPT AS EXPRESSLY PROVIDED IN THIS LICENSE, GSL HEREBY
* EXCLUDES ANY IMPLIED CONDITION OR WARRANTY CONCERNING THE MERCHANTABILITY OR
* QUALITY OR FITNESS FOR PURPOSE OF THE CODE, WHETHER SUCH CONDITION OR WARRANTY
* IS IMPLIED BY STATUTE OR COMMON LAW. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLAR
* , OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS CODE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
* 
* 4. Term and Termination. This License Agreement shall commence on acceptance
* of these terms by the Licensee (or Sub-Licensee as applicable) and shall
* continue unless terminated by GSL for cause in the event that the Licensee (or
* Sub-Licensee as applicable) commits any material breach of this License
* Agreement and fails to remedy that breach within 30 days of being given
* written notice of that breach by GSL.  
* 
* 5. Law and Jurisdiction. This License Agreement is governed by the laws of
* England and Wales, and is subject to the exclusive jurisdictions of the
* English courts.
* 
*******************************************************************************/

/*
 * gdi.h
 *
 *      Author: pgrabalski (pgrabalski@guralp.com)
 */

#ifndef GDI_H_
#define GDI_H_

#include "gdi_defs.h"

/*
 * This file contains declarations of general GDI functions.
 *
 * The most exposed functions of this GDI implementation are:
 * - gdi_connect                 - allows to connect to server of given IP address.
 * - gdi_disconnect              - to close connection and clean up server instance.
 * - gdi_subscribe               - subscribe a channel to start receiving its data.
 * - gdi_register_sample_handler - to register various data handlers which will
 *                                 process data received with NEW_DATA command.
 * - gdi_launch                  - starts GDI client thread which starts the
 *                                 negotiation process and - if successful and
 *                                 subscribed to a channel - receives data and pass
 *                                 it to registered sample handler.
 * - gdi_destroy_server          - server cleanup function to free all dynamically
 *                                 allocated memories.
 */

/**
 * \brief Connect function.
 *
 * This function does the socket connection to \p host provided.
 *
 * @param host IP address of host to connect to.
 * @param port Port on which we should connect (1565 by default)
 * @return Instance of GDI server or NULL if failed connecting.
 */
GDI_Server * gdi_connect(const char * host, int port);

/**
 * \brief Disconnect function.
 *
 * This function disconnects \p server from the network socket.
 *
 * @param server Pointer to the server instance.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_disconnect(GDI_Server * server);

/**
 * \brief Add channel subscription.
 *
 * This function will add a channel subscription so server
 * will start sending related data.
 *
 * @param server Pointer to the server instance.
 * @param channel Pointer to channel identification.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_subscribe(GDI_Server * server, GDI_Channel * channel);

/**
 * \brief Launch GDI communication processing thread.
 * @param server Server to launch for.
 */
void gdi_launch(GDI_Server * server);

/**
 * \brief Destroys server instance and frees allocated memory.
 * @param server Server to be destroyed
 */
void gdi_destroy_server(GDI_Server * server);

/**
 * \brief Lookup a key string in the metadata, return the value string. Returns NULL if no match.
 * @param chan channel for the metadata lookup
 * @param key key string to lookup
 */
char* gdi_lookup_metadata(GDI_Channel *chan, char* key);

#endif /* GDI_H_ */
