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
 * gdi_server.h
 *
 *      Author: pgrabalski (pgrabalski@guralp.com)
 */

#ifndef INC_GDI_SERVER_H_
#define INC_GDI_SERVER_H_

#include "gdi_defs.h"

/*
 * This file contains GDI server related function declarations.
 *
 * GDI server related functions can be used to extract certain
 * information from server instance or to modify server details.
 */

/**
 * @brief GDI Server - Get channel information.
 *
 * This function will look for a channel described by \p channel_id
 * and return a pointer to the describing structure.
 *
 * @param srv GDI Server to look within.
 * @param channel_id Channel id
 * @return Channel details structure or NULL if channel is not available.
 */
GDI_Channel * gdi_srv_get_channel(GDI_Server * srv, uint32_t channel_id);

/**
 * @brief GDI Server - Get segment information.
 *
 * This function will look for a segment of \p segment_id id, in channel
 * of \p channel_id id and will return a pointer to GDI_Segment structure
 * if found.
 *
 * @param srv GDI Server to look within.
 * @param channel_id Channel id
 * @param segment_id Segment id
 * @return Segment details structure or NULL if segment has not been found.
 */
GDI_Segment * gdi_srv_get_segment(GDI_Server * srv, uint32_t channel_id, uint32_t segment_id);

/**
 * @brief GDI Server - Get segment by ID.
 *
 * This function will look for a segment of \p segment_id id, regardless
 * to it's parent channel, and will return a pointer to GDI_Segment structre
 * if segment has been found.
 *
 * @param srv GDI Server to look within.
 * @param segment_id Segment ID
 * @return Segment details structure or NULL if segment has not been found.
 */
GDI_Segment * gdi_srv_get_segment_by_id(GDI_Server * srv, uint32_t segment_id);

/**
 * @brief GDI Server - Checks if client is subscribed to a channel containing specified segment ID.
 *
 * This function will look for a channel which contains a segment of \p segment_id ID
 * and return 1 if client is subscribed to this channel, or 0 if not.
 *
 * @param srv GDI Server instance.
 * @param segment_id Segment ID to check.
 * @return 1 if client is subscribing this segment/channel, or 0 if not.
 */
uint8_t gdi_srv_is_accepting_segment(GDI_Server * srv, uint32_t segment_id);

/**
 * @brief GDI Server - Advance segment time by number of samples received.
 *
 * This function will adjust the last sample receive time by calculating the time
 * offset from previous sample to the last of currently processed ones.
 *
 * Routine is taking the previous last sample time, and adds the number of nanoseconds
 * based on number of samples received and sampling rate.
 *
 * @code
 * uint64_t nsecs = (uint64_t)(sample_rate_div*1e9*number_of_samples)/(uint64_t)sample_rate_num;
 * // where:
 * // sample_rate_div is a sampling rate divisor of channel/segment
 * // sample_rate_num is a sampling rate numerator of channel/segment
 * // number_of_samples is number of samples received
 * // 1e9 is NANO second multiplier
 * @endcode
 *
 * @param srv
 * @param segment_id
 * @param number_of_samples
 */
void gdi_srv_advance_segment_time(GDI_Server * srv, uint32_t segment_id, uint32_t number_of_samples);

/**
 * @brief GDI Server - Add metatada to channel
 *
 * This function is adding or modifying existing metadata to a channel.
 *
 * @param srv GDI Server instance.
 * @param channel Pointer to channel description structure.
 * @param metadata Pointer to metadata structure which has to be added to the channel.
 */
void gdi_srv_add_metadata(GDI_Server * srv, GDI_Channel * channel, GDI_ChannelMetadata * metadata);

/**
 * @brief GDI Server - Translates server stat to pretty string.
 * @param state Server state
 * @return String state representation.
 */
char * gdi_srv_state_to_pretty(GDI_ConnectionState state);

/**
 * @brief GDI Server - GDI Advance time function.
 *
 * This function is applying the time advance to given GDI time
 * described by \p day \p sec and \p nsec values.
 * Time advance is calculated basing on sampling rate provided in
 * \p sample_rate_num \p sample_rate_div and \p number_of_samples
 * parameters.
 *
 * Function will calculate time that should elapse during receiving
 * \p number_of_samples samples and adjust \p day \p sec and \p nsec
 * parameters accordingly.
 *
 * Calculation is done same way as in gdi_srv_advance_segment_time function.
 *
 * @code
 * uint64_t nsecs = (uint64_t)(sample_rate_div*1e9*number_of_samples)/(uint64_t)sample_rate_num;
 * // where:
 * // sample_rate_div is a sampling rate divisor of channel/segment
 * // sample_rate_num is a sampling rate numerator of channel/segment
 * // number_of_samples is number of samples received
 * // 1e9 is NANO second multiplier
 * @endcode
 *
 *
 * @param day Pointer to day value
 * @param sec Pointer to seconds value
 * @param nsec Pointer to nanoseconds value
 * @param sample_rate_num Sample rate numerator
 * @param sample_rate_div Sample rate divisor
 * @param number_of_samples Number of samples
 */
void gdi_advance_time(uint32_t * day, uint32_t * sec, uint32_t * nsec, uint32_t sample_rate_num, uint32_t sample_rate_div, uint32_t number_of_samples);

#endif /* INC_GDI_SERVER_H_ */
