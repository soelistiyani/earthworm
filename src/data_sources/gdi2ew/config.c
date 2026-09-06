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

struct config_struct config = { 0 };
subscribe_struct* sub_chans;

subscribe_struct* sub_add(char* name)
{
    subscribe_struct* sub = sub_chans;
    // scan the existing subscriptions for a duplicate
    while (sub)
    {
        if ( strcmp(name, sub->gdi_name)==0 )
            return sub;
        sub=sub->next;
    }

    // add a new subscription to the list.
    sub = calloc(sizeof(subscribe_struct), 1);

    sub->gdi_name = str_null_dup(name);
    sub->next = sub_chans;
    sub_chans = sub;

    return sub;
}

subscribe_struct *sub_add_with_override(char *name)
{
    subscribe_struct* sub = sub_add(name);

    sub->ovr_sta = str_null_dup(k_str());
    sub->ovr_chan = str_null_dup(k_str());
    sub->ovr_net = str_null_dup(k_str());
    sub->ovr_loc = str_null_dup(k_str());

    return sub;
}

subscribe_struct* sub_lookup(GDI_Channel* channel)
{
    subscribe_struct* sub = sub_chans;
    while (sub)
    {
        if (sub->channel == channel)
            return sub;
        sub = sub->next;
    }
    return NULL;
}

void parse_config (const char *filename)
{
    int fh;
    char *tok;

    fh = k_open ((char *) filename);

    if (!fh)
        fatal (("Can't open configuration file %s"));


    while (fh)
    {
        while (k_rd ())
        {
            tok = k_str ();


            if ((!tok) || (*tok == '#'))
                continue;


            if (*tok == '@')
            {
                fh = k_open (tok + 1);
                if (!fh)
                    fatal (("Can't open configuration file %s"));

                continue;
            }

            if (k_its ("LogFile"))
            {
                config.writelog = k_int ();
            }
            else if (k_its ("MyModuleId"))
            {
                char *arg = k_str ();
                if (arg)
                    config.modulename = strdup (arg);
            }
            else if (k_its ("RingName"))
            {
                char *arg = k_str ();
                if (arg)
                    config.ringname = strdup (arg);
            }
            else if (k_its ("HeartBeatInterval"))
            {
                config.heartbeatinterval = (double) k_long ();
            }
            else if (k_its ("Server"))
            {
                char *arg = k_str ();
                if (arg)
                    config.server = strdup (arg);
            }
            else if (k_its ("PortNumber"))
            {
                config.port = k_int();
            }
            else if (k_its ("Subscribe"))
            {
                char *arg = k_str();
                if (!arg)
                    warning (("empty Subscribe line, ignoring.\n"));
                else
                    sub_add_with_override(arg);
            }
            else if (k_its ("SubscribeGroup"))
            {
                config.sub_group = k_int();
            }
            else if (k_its ("Verbose"))
            {
                config.verbose=k_int();
            }
            else if (k_its ("TraceBufSize"))
            {
                config.tracebufsize = k_int();
            }
            else if (k_its ("LowLatency"))
            {
                config.lowlatency = (k_int() != 0);
            }
            else if (k_its ("Timeout"))
            {
                config.timeout = k_int();
            }

            else
            {
                fatal (("Unknown command %s in configuration file", tok));
            }
        }

        fh = k_close ();
    }


    if (!config.modulename)
        fatal (("Missing MyModuleId in configuration file"));

    if (!config.server)
        fatal (("No Server specified in the configuration file"));

    if (!config.port)
    {
        config.port = GDI_DEFAULT_PORT;
        warning (("'PortNumber' not specificed in configuration file, defaulting to %d\n",config.port));
    }

    if (!config.tracebufsize)
    {
        config.tracebufsize = 1000;
        warning (("'TraceBufSize' not specified, defaulting to %d\n",config.tracebufsize));
    }

    if (!config.timeout)
    {
        config.timeout = 60000;
        warning (("'Timeout' not specified, defaulting to %d ms\n",config.timeout));
    }
}
