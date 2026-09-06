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
 * gdi_trhead.c
 *
 *  Created on: 3 Nov 2016
 *      Author: pgrabalski
 */

#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GDI_COMMAND_IMPOSSIBLE_LENGTH 10000 // defines surely impossible length of GDI command
#define GDI_TYPE_AND_LENGTH_SIZE 8          // defines size of GDI Command type and length (as size of 2 32bit values)
#define GDI_KEEPALIVE_INTERVAL 20           // interval in number of seconds between keepalive commands - should not be more than every 30 seconds
#define GDI_THREAD_SLEEP 5                  // defines thread sleep in miliseconds

/**
 * @brief Shifts data buffer by given length.
 *
 * This function is moving data buffer to skip first \p cmd_len bytes.
 *
 * @param data Data buffer
 * @param size Size of data buffer
 * @param cmd_len Length to be shifted
 */
static void gdi_shift_databuffer(char ** data, int * size, uint32_t cmd_len)
{
    info4(("Shift databuffer by %d (old size %d, new size %d)\n", cmd_len, *size, *size - cmd_len));
    char * ptr = *data;
    memmove(ptr, ptr+cmd_len, (*size)-cmd_len);
    *size = *size - cmd_len;
}


void* gdi_client(void * server)
{
    // check if server provided exists
    if (server == NULL)
    {
        error(("No server provided\n"));
        KillSelfThread();
    }

    // initialise some values
    GDI_Server * srv = (GDI_Server *)(server);
    char * buffer = &(srv->datapool[0]);
    int running = 1;
    int err = 0;
    int rcv_size = 0;
    int prev_rcv = 0;
    uint32_t cmd = 0;
    uint32_t cmd_len = 0;
    time_t last_keepalive = time(NULL);

    // get hostname for this client identification
    enum { HOSTNAME_LEN = 50 };
    char hostname[HOSTNAME_LEN];
    gethostname(hostname, HOSTNAME_LEN);

    RequestSpecificMutex(&(srv->mutex));
    // send negotiation command
    err = gdi_send(srv, gdi_prep_cmd_negotiation(hostname));
    if (err != GDI_OK) {
        error(("gdi_send() failed\n"));
    }
    ReleaseSpecificMutex(&(srv->mutex));

    // start thread loop
    while (running == 1)
    {
        RequestSpecificMutex(&(srv->mutex));

        // keep track of what has been received already
        prev_rcv = rcv_size;
        buffer = &(srv->datapool[prev_rcv]);

        // receive from socket if anything is pending
        info4(("gdi_client calling recv() ... "));
        rcv_size = recv_ew(srv->conn_handle, buffer, GDI_DATAPOOL_SIZE-prev_rcv, 0, config.timeout);
        info4(("%i bytes.\n",rcv_size));
        if (rcv_size <= 0)
        {
            ReleaseSpecificMutex(&(srv->mutex));
            info4(( "Error code %d",socketGetError_ew() ));
            running = 0;
            continue;
        }

        rcv_size += prev_rcv;
        buffer = &(srv->datapool[0]);

        while (rcv_size >= GDI_TYPE_AND_LENGTH_SIZE)
        {
            cmd = gdi_get_command_type(buffer, rcv_size);
            cmd_len = gdi_get_command_length(buffer, rcv_size) + GDI_TYPE_AND_LENGTH_SIZE;


            if (cmd_len > GDI_COMMAND_IMPOSSIBLE_LENGTH)
            {
                // command length is out of sync
                // should close and reopen the connection
                // to get back in sync again
                running = 0;
                srv->state = GDI_State_Out_Of_Sync;
                break;
            }

            if (cmd_len > (uint32_t)rcv_size)
            {
                break;
            }


            info3(("Server state: (%d) %s\n", srv->state, gdi_srv_state_to_pretty(srv->state)));
            info4(("CMD Received: %08X (size left %d)\n", cmd, rcv_size));

            // process current state
            switch (cmd)
            {
                case GDI_CMD_NEGOTIATION:
                    // negotiation command received
                    if (GDI_OK == gdi_proc_cmd_negotiation(srv, &buffer, &rcv_size))
                    {
                        info3(("Command: GDI_CMD_NEGOTIATION received. Sending GDI_CMD_CONFIG\n"));
                        // successfully processed command
                        // send configuration
                        err = gdi_send(srv, gdi_prep_cmd_configuration(NULL, 0));
                    }
                    break;
                case GDI_CMD_CONFIG:
                    // configuration comman dreceived
                    if (GDI_OK == gdi_proc_cmd_configuration(srv, &buffer, &rcv_size))
                    {
                        info3(("Command: GDI_CMD_CONFIG received. Sending GDI_CMD_CONFIG_ACK\n"));
                        // configuration accepted, send ACK
                        err = gdi_send(srv, gdi_prep_cmd_configuration_ack());
                    } else {
                        info3(("Command: GDI_CMD_CONFIG received. Sending GDI_CMD_CONFIG_NAK\n"));
                        // configuration rejected, send NAK
                        err = gdi_send(srv, gdi_prep_cmd_configuration_nak(NULL));
                    }
                    break;
                case GDI_CMD_CONFIG_ACK:
                    info3(("Command: GDI_CMD_CONFIG_ACK received. \n"));
                    // config acknowledged
                    err = gdi_proc_cmd_configuration_ack(srv, &buffer, &rcv_size);
                    break;
                case GDI_CMD_CONFIG_NAK:
                    info3(("Command: GDI_CMD_CONFIG_NAK received. \n"));
                    // config rejected
                    err = gdi_proc_cmd_configuration_nak(srv, &buffer, &rcv_size);
                    break;
                case GDI_CMD_NEW_CHANNEL:
                    info3(("Command: GDI_CMD_NEW_CHANNEL received. \n"));
                    // new channel notification
                    // server sends channel information which should be stored
                    // in case subscription will be required
                    err = gdi_proc_cmd_new_channel(srv, &buffer, &rcv_size);
                    break;
                case GDI_CMD_NEW_METADATA:
                    info3(("Command: GDI_CMD_NEW_METADATA received. \n"));
                    // new channel metadata arrived
                    err = gdi_proc_cmd_new_metadata(srv, &buffer, &rcv_size);
                    break;
                case GDI_CMD_NEW_SEGMENT:
                    info3(("Command: GDI_CMD_NEW_SEGMENT received. \n"));
                    // new segment notification
                    // server sends new segment information which should be stored
                    // to keep track of timing since new segment command contains
                    // a start time of data that is coming in
                    err = gdi_proc_cmd_new_segment(srv, &buffer, &rcv_size);
                    break;
                case GDI_CMD_END_SEGMENT:
                    info3(("Command: GDI_CMD_END_SEGMENT received. \n"));
                    // end segment notification
                    // server sends end segment notification to mark segments stored
                    // by client as completed
                    err = gdi_proc_cmd_end_segment(srv, &buffer, &rcv_size);
                    break;
                case GDI_CMD_KEEPALIVE:
                    info3(("Command: GDI_CMD_KEEPALIVE received. Sending GDI_CMD_KEEPALIVE\n"));
                    // keep alive ping received - send back
                    err = gdi_proc_cmd_keep_alive(&buffer, &rcv_size);
                    err = gdi_send(srv, gdi_prep_cmd_keep_alive());
                    last_keepalive = time(NULL);
                    break;
                case GDI_CMD_MARKER:
                    info3(("Command: GDI_CMD_MARKER received.\n"));
                    err = gdi_proc_cmd_marker(srv, &buffer, &rcv_size);
                    break;
                default:
                    if (cmd & GDI_CMD_NEW_DATA_ID)
                    {
                        info3(("Command: GDI_CMD_NEW_DATA received. \n"));
                        err = gdi_proc_cmd_new_data(srv, &buffer, &rcv_size);
                    } else {
                        // unknown command
                    }
                    break;
            }
            gdi_shift_databuffer(&buffer, &rcv_size, cmd_len);


            if (err != GDI_OK)
            {
                error(("GDI Error: Error-code %d\n", err));
                srv->state = GDI_State_Out_Of_Sync;
                running = 0;
                break;
            }

            if (srv->state == GDI_State_Close_Connection)
            {
                running = 0;
                break;
            }
        }

        // if last keep alive command has been received longer than 20 seconds ago
        // client should notify server that it is still alive (timeout has to be less
        // than 30 seconds).
        if (time(NULL) - last_keepalive > GDI_KEEPALIVE_INTERVAL)
        {
            last_keepalive = time(NULL);
            err = gdi_send(srv, gdi_prep_cmd_keep_alive());
            if (err != GDI_OK)
            {
                error(("GDI Error: Error-code %d\n", err));
                srv->state = GDI_State_Out_Of_Sync;
                running = 0;
                ReleaseSpecificMutex(&(srv->mutex));
                break;
            }
        }

        if (srv->state == GDI_State_Close_Connection)
        {
            running = 0;
            ReleaseSpecificMutex(&(srv->mutex));
            break;
        }

        ReleaseSpecificMutex(&(srv->mutex));

        sleep_ew(GDI_THREAD_SLEEP);
    }

    info3(("GDI Client stopped\n"));

    buffer=NULL;

    if ( srv->on_thread_terminated )
        (*srv->on_thread_terminated)( srv );

    KillSelfThread();
    // thread end, socket disconnection is done in
    // main thread, lets return
    return NULL;
}

