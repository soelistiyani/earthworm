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
 * gdi_server.c
 *
 *  Created on: 10 Nov 2016
 *      Author: pgrabalski
 */

#include "gdi_server.h"
#include <string.h>


GDI_Channel * gdi_srv_get_channel(GDI_Server * srv, uint32_t channel_id)
{
    if (srv == NULL)
    {
        // if no server provided, no channel can be find
        return NULL;
    }

    int counter = 0;

    for (counter = 0; counter < srv->no_channels; ++counter)
    {
        if (srv->channels[counter].id == channel_id)
        {
            // found!
            return &(srv->channels[counter]);
        }
    }

    return NULL;
}

GDI_Segment * gdi_srv_get_segment(GDI_Server * srv, uint32_t channel_id, uint32_t segment_id)
{
    if (srv == NULL)
    {
        return NULL;
    }

    int counter = 0;
    GDI_Channel * channel = gdi_srv_get_channel(srv, channel_id);

    if (channel != NULL)
    {
        // channel found
        for (counter = 0; counter<channel->no_segment; ++counter)
        {
            if (channel->segment[counter].id == segment_id)
            {
                return &(channel->segment[counter]);
            }
        }
    }

    return NULL;
}

GDI_Segment * gdi_srv_get_segment_by_id(GDI_Server * srv, uint32_t segment_id)
{
    if (srv == NULL)
    {
        return NULL;
    }

    int i = 0;
    int j = 0;

    for (i = 0; i < srv->no_channels; ++i)
    {
        for (j=0; j<srv->channels[i].no_segment; ++j)
        {
            if (srv->channels[i].segment[j].id == segment_id)
            {
                return &(srv->channels[i].segment[j]);
            }
        }
    }

    return NULL;
}

uint8_t gdi_srv_is_accepting_segment(GDI_Server * srv, uint32_t segment_id)
{
    GDI_Segment * segment = gdi_srv_get_segment_by_id(srv, segment_id);
    if ( segment != NULL && segment->parent_channel != NULL)
    {
        return segment->parent_channel->subscribed;
    }

    return 0;
}


void gdi_srv_advance_segment_time(GDI_Server * srv, uint32_t segment_id, uint32_t number_of_samples)
{
    GDI_Segment * segment = gdi_srv_get_segment_by_id(srv, segment_id);
    if (segment != NULL)
    {
        gdi_advance_time( &segment->last_sample_day, &segment->last_sample_sec, &segment->last_sample_nsec,
                          segment->sample_rate_num, segment->sample_rate_div, number_of_samples);
    }
}

void gdi_srv_add_metadata(GDI_Server * srv, GDI_Channel * channel, GDI_ChannelMetadata * metadata)
{
    if (srv == NULL)
    {
        // no server provided
        return;
    }
    if (channel == NULL)
    {
        // no channel provided
        return;
    }
    if (metadata == NULL)
    {
        // no metadata provided
        return;
    }

    int counter = 0;
    uint8_t processed = 0;
    // check if update required
    for (counter = 0; counter<channel->no_metadata; ++counter)
    {
        if (strcmp(channel->metadata[counter].key, metadata->key) == 0)
        {
            // looks like metadata already exists
            free(channel->metadata[counter].value);
            channel->metadata[counter].value = strdup(metadata->value);
            processed = 1;
        }
    }

    // looks like metadata is a new entry
    if (processed == 0)
    {
        channel->metadata[channel->no_metadata].key = strdup(metadata->key);
        channel->metadata[channel->no_metadata].value = strdup(metadata->value);

        // increment number of metadata
        channel->no_metadata++;
    }
}

char * gdi_srv_state_to_pretty(GDI_ConnectionState state)
{
    switch (state)
    {
        case GDI_State_Send_Negotiation:
            return "GDI_State_Send_Negotiation";
            break;
        case GDI_State_Await_Negotiation:
            return "GDI_State_Await_Negotiation";
            break;
        case GDI_State_Send_Configuration:
            return "GDI_State_Send_Configuration";
            break;
        case GDI_State_Await_Configuration_Approval:
            return "GDI_State_Await_Configuration_Approval";
            break;
        case GDI_State_Await_Configuration:
            return "GDI_State_Await_Configuration";
            break;
        case GDI_State_Send_Configuration_Approval:
            return "GDI_State_Send_Configuration_Approval";
            break;
        case GDI_State_Connection_Negotiated:
            return "GDI_State_Connection_Negotiated";
            break;
        case GDI_State_Close_Connection:
            return "GDI_State_Close_Connection";
            break;
        default:
            return "Undefined";
            break;
    }
    return "Undefined";
}

void gdi_advance_time(uint32_t * day, uint32_t * sec, uint32_t * nsec, uint32_t sample_rate_num, uint32_t sample_rate_div, uint32_t number_of_samples)
{
    // calculate how many nanoseconds has to be added basing on segment sampling rate
    uint64_t nsecs = (uint64_t)(sample_rate_div*1e9*number_of_samples)/(uint64_t)sample_rate_num;
    nsecs += *nsec;
    *nsec = nsecs % 1000000000; // only nanoseconds
    nsecs /= 1e9; // switch to seconds
    nsecs += *sec;
    *sec = nsecs % 86400; // only seconds of a day (60 seconds * 60 minutes * 24 hours = 86400 seconds)
    nsecs /= 86400; // switch to days
    *day += nsecs; // just add
}

