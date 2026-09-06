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

#include "project.h"
#include "time_ew.h"

static GDI_Server * server = NULL;

void send_tracebuf(TRACE2_HEADER* tb)
{
    tb->endtime = tb->starttime + (tb->nsamp-1) / tb->samprate;
    int len = (tb->nsamp * sizeof (int32_t));
    // send ...
    info2(("Sending %s.%s.%s.%s %d samples",tb->sta, tb->chan, tb->net, tb->loc, tb->nsamp));
    ewc_send ((uint8_t *)(tb), len + sizeof (TRACE2_HEADER), SEND_TYPE_TRACE);
    // ... and reset ...
    tb->starttime = tb->starttime + tb->nsamp / tb->samprate;
    tb->nsamp = 0;
}

// called by the receiver thread when new data is available for a given segment.
static GDI_Error data_handler(uint32_t segment_id, int32_t * data, int no_samples)
{
    if (server == NULL)
    {
        return GDI_ERR_UninitialisedParameter;
    }

    // get the segment object from the numerical ID
    GDI_Segment * seg = gdi_srv_get_segment_by_id(server, segment_id);
    if (seg == NULL)
    {
        return GDI_ERR_UninitialisedParameter;
    }

    // get the channel object, and ensure we have a cross-reference to the subscription
    GDI_Channel* chan = seg->parent_channel;
    if (!chan->user_ptr) {
        chan->user_ptr = sub_lookup(chan);
    }

    // check we have actually subscribed to this channel
    subscribe_struct* sub = chan->user_ptr;
    if (!sub)
    {
        error(("channel %s not found in subscription list.\n",chan->name));
        return GDI_ERR_UninitialisedParameter;
    }

    TRACE2_HEADER* tb = sub->tracebuf;

    info4 (("new data. no_samples=%d, segment=%d, chan_id=%d, chan_name=%s\n",
                       no_samples,    segment_id, chan->id,   chan->name));

    // is this the first time we have data for this channel? Set up the tracebuf fields.
    if (!tb) {
        info2 (("new tracebuf required for %s\n",sub->gdi_name));
        int len = (config.tracebufsize * sizeof (int32_t));
        tb = sub->tracebuf = calloc (sizeof (TRACE2_HEADER) + len, 1);

        if (!tb) {
            error(("failed to allocate tracebuf memory for %s",chan->name));
            return GDI_ERR_SizeLimitReached;
        }

        // set up tracebuf static fields, including the SCNL overrides if specified by config file.
        tb->samprate = (double) sub->channel->sample_rate_num / (double) sub->channel->sample_rate_div;
        if ((sub->ovr_sta) && (sub->ovr_net) && (sub->ovr_chan) && (sub->ovr_loc))
        {
            strcpy(tb->sta, sub->ovr_sta);
            strcpy(tb->chan, sub->ovr_chan);
            strcpy(tb->net, sub->ovr_net);
            strcpy(tb->loc, sub->ovr_loc);
        }
        else
        {
            char* seed_name = gdi_lookup_metadata(chan, "M-SEEDNAME");
            sscanf(seed_name, "%[^.].%[^.].%[^.].%[^.]", tb->net, tb->sta, tb->loc, tb->chan);
        }
        tb->version[0] = TRACE2_VERSION0;
        tb->version[1] = TRACE2_VERSION1;
        strcpy (tb->datatype, GDI2EW_DATATYPE);
        info1 (("Setup conversion for channel %s -> SCNL %s.%s.%s.%s\n", sub->gdi_name, tb->sta, tb->chan, tb->net, tb->loc));
    }

    if ( segment_id != sub->current_segment )
    {
        // we have changed segment, flush and reset.
        info1(("new segment for %s, resetting time.\n",sub->gdi_name));
        if ( tb->nsamp )
            send_tracebuf(tb);

        // convert sample time to EW epoch
        double ew_time = GDI_SECINDAY * (seg->last_sample_day - GDI_EPOCHDAY) + seg->last_sample_sec;
        ew_time += seg->last_sample_nsec/1e9;

        tb->starttime = ew_time;
        tb->endtime = ew_time;
        tb->nsamp = 0;
        sub->current_segment = segment_id;
    }

    int32_t *samps = (int32_t*) (tb +1);
    int i;
    // copy the samples into the tracebuf. When it gets full, dispatch and reset.
    for (i=0; i<no_samples; i++)
    {
        samps[tb->nsamp++] = data[i];
        if ( tb->nsamp >= config.tracebufsize )
            send_tracebuf(tb);
    }

    // If the config is for "low latency", and there are samples left in the tracebuf, just send them.
    if (config.lowlatency && (tb->nsamp>0))
        send_tracebuf(tb);

    return GDI_OK;
}

// called by the receiver thread when the server notifies us that a new channel is available.
static GDI_Error new_channel_handler( GDI_Channel* chan)
{
    static int first_pass = 1;
    int group = atoi( gdi_lookup_metadata(chan, "M-GROUP") );

    if ((!sub_chans) && (config.sub_group==0))
    {
        if (first_pass)
        {
            info0(("No 'Subscribe' or 'SubscribeGroup' specified in config, just listing channels."));
            info0(("--------------------------------------------------------------"));
            info0(("|             GDI name |     Seed name (NSLC) | GROUP |  SPS |"));
            info0(("|----------------------|----------------------|-------|------|"));
            first_pass=0;
        }
        char* seed_name = gdi_lookup_metadata(chan, "M-SEEDNAME");
        info0(("| %20s | %20s | %5i | %4i |\n", chan->name, seed_name, group, chan->sample_rate_num/chan->sample_rate_div));
        return GDI_OK;
    }

    // subscribe to channels based on group level
    info2(("checking for SubscribeGroup - config=%d, channel=%d\n",config.sub_group, group));
    if ( (config.sub_group>0) && (group>0) && (group <= config.sub_group) )
    {
        subscribe_struct* sub = sub_add(chan->name);
        sub->channel = chan;
        chan->user_ptr = sub;
        info1(("SubscribeGroup match for %s\n", chan->name));
        gdi_subscribe(server, chan);
        return GDI_OK;
    }

    // subscribe to individually specified channels
    subscribe_struct* sub = sub_chans;
    while (sub)
    {
        if ( !strcmp(sub->gdi_name, chan->name) )
        {
            chan->user_ptr = sub;
            sub->channel = chan;
            gdi_subscribe(server, chan);
            return GDI_OK;
        }
        sub = sub->next;
    }

    return GDI_OK;
}

void thread_terminated(GDI_Server* srv)
{
    free(server);
    server = NULL;

    // reset all active subscriptions and tracebufs
    subscribe_struct* sub = sub_chans;
    while (sub)
    {
        if ( (sub->tracebuf) && (sub->tracebuf->nsamp) )
            send_tracebuf(sub->tracebuf);
        sub->current_segment = -1;
        sub = sub->next;
    }
    info1(("GDI thread terminated"));

    // The re-connect will occur in the main thread loop
}

void check_gdi_connect(char* host, int port)
{
    // if already connected, nothing to do
    if (server) return;

    info1(("GDI connecting to %s:%d ... ",host,port));
    server = gdi_connect(host, port);

    // if server created and connected then launch a thread
    if (server != NULL)
    {
        info1((" ... connected.\n"));
        server->new_channel_handler = new_channel_handler;
        server->sample_handler = data_handler;
        server->on_thread_terminated = thread_terminated;
        gdi_launch(server);
    }
    else
    {
        error(("Server not connected.\n"));
    }
}
