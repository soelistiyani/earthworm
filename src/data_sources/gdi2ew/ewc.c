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
#include "ewc.h"

static SHM_INFO Region={0};

static unsigned char InstId;
static unsigned char ModId;
static unsigned char TypeHeartBeat;
static unsigned char TypeError;
static unsigned char TypeTraceBuf;
static unsigned char TypeSOH;

void
ewc_init (const char *ModName, const char *RingName)
{
    long RingKey;

    RingKey = GetKey ((char *) RingName);

    if (RingKey < 0)
        fatal (("Cannot get key for ring name `%s'", RingName));

    if (GetLocalInst (&InstId))
        fatal (("error getting local installation id"));

    if (GetModId ((char *) ModName, &ModId))
        fatal (("Cannot get ID for module name `%s'", ModName)); /*FIXME: module name */

    if (GetType ("TYPE_HEARTBEAT", &TypeHeartBeat))
        fatal (("Cannot get type number for TYPE_HEARTBEAT message"));

    if (GetType ("TYPE_ERROR", &TypeError))
        fatal (("Cannot get type number for TYPE_ERROR message"));

    if (GetType ("TYPE_TRACEBUF2", &TypeTraceBuf))
        fatal (("Cannot get type number for TYPE_TRACEBUF2 message"));

    tport_attach (&Region, RingKey); /*Why is this void? */

    /* The old code used to flush the input ring here - I can't see why
 * since we never use the input ring anywhere else
 */

    ewc_send_heartbeat ();

    return;
}



void
ewc_send_heartbeat (void)
{
    time_t now;
    MSG_LOGO msg;
    char buf[1024];
    int len;

    time (&now);

    len =
            snprintf (buf, sizeof (buf), "%ld %ld\n", (long) now, (long) getpid ());

    if (len < 0)
    {
        warning (("Buffer overflow"));
        return;
    }

    msg.instid = InstId;
    msg.mod = ModId;
    msg.type = TypeHeartBeat;

    if (tport_putmsg (&Region, &msg, len, buf) != PUT_OK)
        warning (("Error sending heartbeat."));

}


void
ewc_send_error (int errnum, char *text)
{
    time_t now;
    MSG_LOGO msg;
    char *buf;
    int len;

    time (&now);

    if (!text)
        return;

    buf = malloc ((len = 1024 + strlen (text)));
    if (!buf)
    {
        warning (("Malloc failed"));
        return;
    }

    len = snprintf (buf, len, "%ld %d %s\n", (long) now, errnum, text);

    if (len < 0)
    {
        free (buf);
        warning (("Buffer overflow"));
        return;
    }

    msg.instid = InstId;
    msg.mod = ModId;
    msg.type = TypeError;

    if (tport_putmsg (&Region, &msg, len, buf) != PUT_OK)
        warning (("Error sending error %d:%s", errnum, text));

}

int
ewc_terminate (void)
{
    int flag = tport_getflag (&Region);

    if (flag == TERMINATE)
        return 1;
    if (flag == getpid ())
        return 1;

    return 0;
}

void
ewc_shutdown (void)
{
    tport_detach (&Region);
}


void
ewc_send (uint8_t * buf, int len, int type)
{
    MSG_LOGO msg={0};

    msg.instid = InstId;
    msg.mod = ModId;
    switch(type) {
        case SEND_TYPE_TRACE:
            msg.type = TypeTraceBuf;
            break;
        case SEND_TYPE_SOH:
            msg.type = TypeSOH;
            break;
        default:
            warning (("Error sending data message, bad type specified to ewc_send"));
            return;
    }


    if (len > MAX_TRACEBUF_SIZ)
        fatal (("Assertion trace buf lengh <= MAX_TRACEBUF_SIZ failed"));

    if (tport_putmsg (&Region, &msg, len, (char *) buf) != PUT_OK)
        warning (("Error sending data message"));

}
