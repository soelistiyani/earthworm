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

#ifndef ___VINT_H_
#define ___VINT_H_

#include <stdint.h>

/*
 * This file contains a declaration of variable-length integer packing functions.
 */

/*! \defgroup serialise_vint Variable-length integer packing
\ingroup serialise

This module provides a set of functions for packing and unpacking integers
to/from an efficient form which contains only the bits of the number that are
actually used. The resulting packed objects are a variable number of bytes long
(from 1 to 10, for a full 64-bit number). This packing is most efficient for
integers of small numerical magnitude.

The first bit of each byte is the continuation bit. If it is set, then another
byte of data follows.  If it is unset, then this is the last byte of data. In a
multi-byte number, the last byte can never be 0x00; this means that there is no
ambiguity of representation.

For an unsigned number, the first byte contains the 7 least significant bits of
the number. Each byte thereafter contains the 7 next most significant bits.
Once all bits set in the original number have been stored in bytes, the packing
is complete.

Signed numbers are \em not transmitted as 2's complement (as this would defeat
the short packing scheme used above), but instead as a sign bit and magnitude.
The sign bit is transmitted along with the least significant byte in the
result, which means the first byte contains only the 6 least significant bits
of the number.  The sign bit is the least significant bit of the first byte.

The decoding algorithm for an unsigned number is as follows:

\li Initialise Y (output) to 0, and S (shift bits) to 0
\li Read byte of input into X
\li Store and strip off continuation bit from X
\li Left shift X by S
\li Bitwise OR X into Y
\li Increase S by 7
\li If continuation bit was set in X, go to step 2

The decoding algorithm for a signed number is as follows:

\li Initialise Y (output) to 0, and S (shift bits) to 0
\li Read byte of input into X
\li Store and strip off continuation bit from X
\li Store least significant bit of X as sign bit
\li Right shift X by 1
\li Store X into Y
\li Increase S by 6
\li If continuation bit was set, move to unsigned decoder
\li If sign bit was set, multiply result by -1

A diagram show the structure of the coded numbers is below:

<pre>
Unsigned, only 7 bits
---------------------
byte    (MSB) bit meanings (LSB)
 0       [0]: continuation bit clear        [xxx xxxx]: data

Unsigned, &gt; 7 bits
------------------
byte    (MSB) bit meanings (LSB)
 0       [1]: continuation bit set          [xxx xxxx]: data least significant 7 bits
...      [1]: continuation bit set          [xxx xxxx]: data next most significant 7 bits
 n       [0]: continuation bit clear        [xxx xxxx]: data most significant 7 bits

Signed, only 6 bits
-------------------
byte    (MSB) bit meanings (LSB)
 0       [0]: continuation bit clear        [xxx xxx]: data     [s]: sign

Signed, > 6 bits
----------------
byte    (MSB) bit meanings (LSB)
 0       [1]: continuation bit set          [xxx xxx]: data     [s]: sign
...      [1]: continuation bit set          [xxx xxxx]: data next most significant 7 bits
 n       [0]: continuation bit clear        [xxx xxxx]: data most significant 7 bits
</pre>

*/
/*!@{*/


/** @brief Packs unsigned 16bit int.
 *
 * This function will pack unsigned 16bit int value provided
 * in \p val parameter to output \p buf buffer and will return
 * the size/length of packed value.
 *
 * @param val uint16 value to be packed.
 * @param buf Output buffer.
 * @return Length of output buffer.
 */
int vint_pack_u16(uint16_t val, char* buf);

/** @brief Packs signed 16bit int.
 *
 * This function will pack signed 16bit int value provided
 * in \p val parameter to output \p buf buffer and will return
 * the size/length of packed value.
 *
 * @param val int16 value to be packed.
 * @param buf Output buffer.
 * @return Length of output buffer.
 */
int vint_pack_s16(int16_t val, char* buf);

/** @brief Packs unsigned 32bit int.
 *
 * This function will pack unsigned 32bit int value provided
 * in \p val parameter to output \p buf buffer and will return
 * the size/length of packed value.
 *
 * @param val uint32 value to be packed.
 * @param buf Output buffer.
 * @return Length of output buffer.
 */
int vint_pack_u32(uint32_t val, char* buf);

/** @brief Packs signed 32bit int.
 *
 * This function will pack signed 32bit int value provided
 * in \p val parameter to output \p buf buffer and will return
 * the size/length of packed value.
 *
 * @param val int32 value to be packed.
 * @param buf Output buffer.
 * @return Length of output buffer.
 */
int vint_pack_s32(int32_t val, char* buf);

/** @brief Unpacks unsigned 16bit int.
 *
 * This function will unpack unsigned 16bit int value provided
 * in \p buf parameter to output \p val value and will return
 * the position within input buffer when done.
 *
 * @param buf Input buffer.
 * @param buf_len length of the buffer.
 * @param val Output value
 * @return Position within input buffer where unpacker finished.
 */
int vint_unpack_u16(const char* buf, int buf_len, uint16_t* val);

/** @brief Unpacks unsigned 32bit int.
 *
 * This function will unpack unsigned 32bit int value provided
 * in \p buf parameter to output \p val value and will return
 * the position within input buffer when done.
 *
 * @param buf Input buffer.
 * @param buf_len length of the buffer.
 * @param val Output value
 * @return Position within input buffer where unpacker finished.
 */
int vint_unpack_u32(const char* buf, int buf_len, uint32_t* val);

/** @brief Unpacks unsigned 64bit int.
 *
 * This function will unpack unsigned 64bit int value provided
 * in \p buf parameter to output \p val value and will return
 * the position within input buffer when done.
 *
 * @param buf Input buffer.
 * @param buf_len length of the buffer.
 * @param val Output value
 * @return Position within input buffer where unpacker finished.
 */
int vint_unpack_u64(const char* buf, int buf_len, uint64_t* val);

/** @brief Unpacks signed 16bit int.
 *
 * This function will unpack signed 16bit int value provided
 * in \p buf parameter to output \p val value and will return
 * the position within input buffer when done.
 *
 * @param buf Input buffer.
 * @param buf_len length of the buffer.
 * @param val Output value
 * @return Position within input buffer where unpacker finished.
 */
int vint_unpack_s16(const char* buf, int buf_len, int16_t* val);

/** @brief Unpacks signed 32bit int.
 *
 * This function will unpack signed 32bit int value provided
 * in \p buf parameter to output \p val value and will return
 * the position within input buffer when done.
 *
 * @param buf Input buffer.
 * @param buf_len length of the buffer.
 * @param val Output value
 * @return Position within input buffer where unpacker finished.
 */
int vint_unpack_s32(const char* buf, int buf_len, int32_t* val);

/** @brief Unpacks signed 64bit int.
 *
 * This function will unpack signed 64bit int value provided
 * in \p buf parameter to output \p val value and will return
 * the position within input buffer when done.
 *
 * @param buf Input buffer.
 * @param buf_len length of the buffer.
 * @param val Output value
 * @return Position within input buffer where unpacker finished.
 */
int vint_unpack_s64(const char* buf, int buf_len, int64_t* val);

#endif /* ___VINT_H_ */
