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

static char *file;
static int line;
static char *what;
static int where;


void dowhere (char *_file, int _line, char *_what, int _where)
{
    what = _what;
    file = _file;
    line = _line;
    where = _where;
}


void doinfo (char *fmt, ...)
{
    va_list ap;
    char buf[2048], mfmt[2048], *ptr, *fptr;

    /* Step 1 is to grok out the %m s in the format string*/

    ptr = fmt;

    /*sprintf returns int, but someone seems to have defined it */
    /*to return char? */
#if 0
    fptr =
            mfmt + sprintf (mfmt, "gdi2ew: %s line %d: %s ", file, line, what);
#else
    sprintf (mfmt, "%s ", what);
    fptr = mfmt + strlen (mfmt);
#endif

    while (*ptr)
    {
        switch (*ptr)
        {
            case '\n':
                break;
            case '%':
                switch (*(ptr + 1))
                {
                    case 'm':
                        ptr++;
#if 0
                        fptr +=
                                sprintf (fptr, "(errno=%d %s)", errno, strerror (errno));
#else
                        sprintf (fptr, "(errno=%d %s)", errno, strerror (errno));
                        fptr += strlen (fptr);
#endif
#ifdef WIN32
                        sprintf (fptr, " [WSAERROR=%d]", WSAGetLastError ());
                        fptr += strlen (fptr);
#endif
                        break;
                    default:
                        *(fptr++) = *ptr;
                }
                break;
            default:
                *(fptr++) = *ptr;
                ;
        }
        ptr++;
    }
    *fptr = 0;

    ptr = buf;

    va_start (ap, fmt);

#if 0
    ptr += vsprintf (ptr, mfmt, ap);
#else
    vsprintf (ptr, mfmt, ap);
    ptr += strlen (ptr);
#endif
    va_end (ap);

    *ptr = 0;

    if (where & MSG_EWLOGIT)
        logit ("et", "%s\n", buf);
    if (where & MSG_CONSOLE)
        fprintf (stderr, "%s\n", buf);
}


void domsg (char *fmt, ...)
{
    va_list ap;
    char buf[2048], mfmt[2048], *ptr, *fptr;

    /* Step 1 is to grok out the %m s in the format string*/

    ptr = fmt;

    /*sprintf returns int, but someone seems to have defined it */
    /*to return char? */
#if 0
    fptr =
            mfmt + sprintf (mfmt, "gdi2ew: %s line %d: %s ", file, line, what);
#else
    sprintf (mfmt, "gdi2ew: %s line %d: %s ", file, line, what);
    fptr = mfmt + strlen (mfmt);
#endif

    while (*ptr)
    {
        switch (*ptr)
        {
            case '\n':
                break;
            case '%':
                switch (*(ptr + 1))
                {
                    case 'm':
                        ptr++;
#if 0
                        fptr +=
                                sprintf (fptr, "(errno=%d %s)", errno, strerror (errno));
#else
                        sprintf (fptr, "(errno=%d %s)", errno, strerror (errno));
                        fptr += strlen (fptr);
#endif
#ifdef WIN32
                        sprintf (fptr, " [WSAERROR=%d]", WSAGetLastError ());
                        fptr += strlen (fptr);
#endif
                        break;
                    default:
                        *(fptr++) = *ptr;
                }
                break;
            default:
                *(fptr++) = *ptr;
                ;
        }
        ptr++;
    }
    *fptr = 0;

    ptr = buf;

    va_start (ap, fmt);

#if 0
    ptr += vsprintf (ptr, mfmt, ap);
#else
    vsprintf (ptr, mfmt, ap);
    ptr += strlen (ptr);
#endif
    va_end (ap);

    *ptr = 0;

    if (where & MSG_EWLOGIT)
        logit ("et", "%s\n", buf);
    if (where & MSG_CONSOLE)
        fprintf (stderr, "%s\n", buf);
}


int complete_read (SOCKET fd, char *buf, int n)
{
    int c = 0;
    int r;

    while (n)
    {
        r = recv (fd, buf, n, 0);
        if (r < 0)
            return r;
        if (!r)
            return c;

        n -= r;
        buf += r;
        c += r;
    }

    return c;
}


char* str_null_dup(char* str)
{
    return str ? strdup(str) : str;
}
