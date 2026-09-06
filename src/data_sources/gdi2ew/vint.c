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

#include "vint.h"
#include <stdint.h>
#include <string.h>

int vint_pack_u16(uint16_t val, char* buf)
{
    uint8_t x;
    int pos = 0;

    do {
        x = val & 0x7F;
        val >>= 7;
        if(val) x |= 0x80;
        buf[pos++] = x;
    }while(val);

    return pos;
}

int vint_pack_s16(int16_t val, char* buf)
{
    uint16_t uval;
    int sign = 0;

    memcpy(&uval, &val, sizeof(val));
    if(val < 0) {
        uval = ~uval + 1;
        sign = 1;
    }

    if((uval & 0x3F) == uval) {
        *buf = (uval << 1) | sign;
        return 1;
    }

    *buf = 0x80 | ((uval & 0x3F) << 1) | sign;
    return vint_pack_u16(uval >> 6, buf + 1) + 1;
}


int vint_pack_u32(uint32_t val, char* buf)
{
    uint8_t x;
    int pos = 0;

    do {
        x = val & 0x7F;
        val >>= 7;
        if(val) x |= 0x80;
        buf[pos++] = x;
    }while(val);

    return pos;
}

int vint_pack_s32(int32_t val, char* buf)
{
    uint32_t uval;
    int sign = 0;

    memcpy(&uval, &val, sizeof(val));
    if(val < 0) {
        uval = ~uval + 1;
        sign = 1;
    }

    if((uval & 0x3F) == uval) {
        *buf = (uval << 1) | sign;
        return 1;
    }

    *buf = 0x80 | ((uval & 0x3F) << 1) | sign;
    return vint_pack_u32(uval >> 6, buf + 1) + 1;
}


int vint_unpack_u16(const char* buf, int buf_len, uint16_t* val)
{
    uint16_t y = 0;
    uint8_t x;
    int pos = 0, shift = 0;

    do {
        if(shift >= 16) return -1; /* overflow */
        if(pos >= buf_len) return -1; /* underflow */
        x = buf[pos++];
        if(!x && pos != 1) return -1; /* bad packing */

        if(shift == 14 && (x & 0x7C)) return -1; /* overflow */
        y |= ((uint16_t)(x & 0x7F)) << shift;
        shift += 7;
    }while(x & 0x80);

    *val = y;
    return pos;
}



int vint_unpack_u32(const char* buf, int buf_len, uint32_t* val)
{
    uint32_t y = 0;
    uint8_t x;
    int pos = 0, shift = 0;

    do {
        if(shift >= 32) return -1; /* overflow */
        if(pos >= buf_len) return -1; /* underflow */
        x = buf[pos++];
        if(!x && pos != 1) return -1; /* bad packing */

        if(shift == 28 && (x & 0x70)) return -1; /* overflow */
        y |= ((uint32_t)(x & 0x7F)) << shift;
        shift += 7;
    }while(x & 0x80);

    *val = y;
    return pos;
}



int vint_unpack_u64(const char* buf, int buf_len, uint64_t* val)
{
    uint64_t y = 0;
    uint8_t x;
    int pos = 0, shift = 0;

    do {
        if(shift >= 64) return -1; /* overflow */
        if(pos >= buf_len) return -1; /* underflow */
        x = buf[pos++];
        if(!x && pos != 1) return -1; /* bad packing */

        if(shift == 63 && (x & 0x7E)) return -1; /* overflow */
        y |= ((uint64_t)(x & 0x7F)) << shift;
        shift += 7;
    }while(x & 0x80);

    *val = y;
    return pos;
}



int vint_unpack_s16(const char* buf, int buf_len, int16_t* val)
{
    uint8_t x;
    uint16_t y;
    int ret;

    x = *buf;
    if(!(x & 0x80)) {
        y = (x >> 1);
        if(x & 1) y = ~y + 1;
        memcpy(val, &y, sizeof(y));
        return 1;
    }

    ret = vint_unpack_u16(buf + 1, buf_len - 1, &y);
    if(ret == -1) return -1;

    /* check for overflow */
    if(y > 0x400) return -1;
    if(y == 0x400) {
        if(x == 0x81) {
            *val = -1;
            return ret + 1;
        } else {
            return -1;
        }
    }

    y <<= 6;
    y |= (x & 0x7F) >> 1;
    if(x & 1) y = ~y + 1;
    memcpy(val, &y, sizeof(y));
    return ret + 1;
}



int vint_unpack_s32(const char* buf, int buf_len, int32_t* val)
{
    uint8_t x;
    uint32_t y;
    int ret;

    x = *buf;
    if(!(x & 0x80)) {
        y = (x >> 1);
        if(x & 1) y = ~y + 1;
        memcpy(val, &y, sizeof(y));
        return 1;
    }

    ret = vint_unpack_u32(buf + 1, buf_len - 1, &y);
    if(ret == -1) return -1;

    /* check for overflow */
    if(y > 0x4000000) return -1;
    if(y == 0x4000000) {
        if(x == 0x81) {
            *val = -1;
            return ret + 1;
        } else {
            return -1;
        }
    }

    y <<= 6;
    y |= (x & 0x7F) >> 1;
    if(x & 1) y = ~y + 1;
    memcpy(val, &y, sizeof(y));
    return ret + 1;
}



int vint_unpack_s64(const char* buf, int buf_len, int64_t* val)
{
    uint8_t x;
    uint64_t y;
    int ret;

    x = *buf;
    if(!(x & 0x80)) {
        y = (x >> 1);
        if(x & 1) y = ~y + 1;
        memcpy(val, &y, sizeof(y));
        return 1;
    }

    ret = vint_unpack_u64(buf + 1, buf_len - 1, &y);
    if(ret == -1) return -1;

    /* check for overflow */
    if(y > 0x400000000000000ULL) return -1;
    if(y == 0x400000000000000ULL) {
        if(x == 0x81) {
            *val = -1;
            return ret + 1;
        } else {
            return -1;
        }
    }

    y <<= 6;
    y |= (x & 0x7F) >> 1;
    if(x & 1) y = ~y + 1;
    memcpy(val, &y, sizeof(y));
    return ret + 1;
}

