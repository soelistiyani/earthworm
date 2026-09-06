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

#include <stdio.h>
#include "project.h"

GDI_Server * gdi_connect(const char * host, int port)
{
    // create and initialise server object
    GDI_Server * server = (GDI_Server*)calloc(1, sizeof(GDI_Server));
    server->no_channels = 0;
    server->state = GDI_State_Send_Negotiation;

    // allocate socket
    SocketSysInit ();
    server->conn_handle = socket_ew(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->conn_handle == -1)
    {
        free(server);
        server = NULL;
        int err = socketGetError_ew();
        error(("Cannot open socket, code %i.\n",err));
        return NULL;
    }

    // prepare for connection
    server->conn.sin_addr.s_addr = inet_addr(host);
    server->conn.sin_family = AF_INET;
    server->conn.sin_port = htons(port);

    // try connect
    if ( connect_ew(server->conn_handle, (struct sockaddr *)(&(server->conn)), sizeof(server->conn), 10000) )
    {
        free(server);
        server = NULL;
        error(("Connection error.\n"));
        return NULL;
    }

    // connection is open
    // lets launch client from some other place

    return server;
}

GDI_Error gdi_disconnect(GDI_Server * server)
{
    // set server to close connection
    // this will stop thread from running
    info1(("GDI disconnect requested"));
    if (server != NULL)
    {
        // stop the thread
        RequestSpecificMutex(&(server->mutex));
        server->state = GDI_State_Close_Connection;
        ReleaseSpecificMutex(&(server->mutex));
        sleep_ew(200);

        int err = 0;
        // shutdown the socket
#ifdef _WINNT
        if ( (err = shutdown(server->conn_handle, 2)) != 0)
#elif LINUX
        if ( (err = shutdown(server->conn_handle, SHUT_RDWR)) != 0)
#else
        if ( (err = shutdown(server->conn_handle, SHUT_RDWR)) != 0)
#endif
        {
            error(("Error on socket shutdown. \n"));
        }
        // close socket
        if (err == 0 && (err = closesocket(server->conn_handle)) != 0)
        {
            error(("Error on socket close. \n"));
        }
        if (err == 0)
        {
            // destroy the server
            gdi_destroy_server(server);
            server = NULL;
        }
    }

    return GDI_OK;
}

GDI_Error gdi_subscribe(GDI_Server * server, GDI_Channel * channel)
{
    // sanity checks
    if (!server)
    {
        error(("Cannot subscribe to channel since no server is provided.\n"));
        return GDI_ERR_UninitialisedParameter;
    }
    if (!channel)
    {
        error(("Cannot subscribe to channel since no channel is provided.\n"));
        return GDI_ERR_UninitialisedParameter;
    }
    // if already subscribed - skip it
    if (channel->subscribed == 1)
    {
        info2(("Already subscribed to this channel.\n"));
        return GDI_OK;
    }

    // mark as subscribed
    channel->subscribed = 1;
    server->channels[channel->list_index].subscribed = 1;

    // send the command
    gdi_send(server, gdi_prep_cmd_subscribe_channel(channel->id));

    return GDI_OK;
}

void gdi_launch(GDI_Server * server)
{
    // init mutex
    CreateSpecificMutex(&(server->mutex));
    // detach server to a thread
    StartThreadWithArg(gdi_client, server, 0, &(server->thread_id));
}

void gdi_destroy_server(GDI_Server * server)
{
    // join thread
    WaitThread(&(server->thread_id));
    CloseSpecificMutex(&(server->mutex));
    // remove all dynamic entities from server structure
    int i = 0, j = 0;
    // go through all of the channels
    for (i=0; i<server->no_channels; ++i)
    {
        // remove name
        free(server->channels[i].name);
        for (j=0; j<server->channels[i].no_metadata; ++j)
        {
            // remove key/value metadata entry
            free(server->channels[i].metadata[j].key);
            free(server->channels[i].metadata[j].value);
        }
        // remove metadata array
        server->channels[i].no_metadata = 0;
    }
    // go through all of the options
    for (i=0; i<server->no_options; ++i)
    {
        // remove option value
        free(server->options[i].value);
    }
    // remove server
    free(server);
    server = NULL;
}

char *gdi_lookup_metadata(GDI_Channel *chan, char *key)
{
    int i;
    for (i=0; i<chan->no_metadata; i++)
        if (! strcmp(chan->metadata[i].key, key))
            return chan->metadata[i].value;
    return NULL;
}
