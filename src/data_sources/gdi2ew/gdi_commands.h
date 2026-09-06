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
 * gdi_commands.h
 *
 *      Author: pgrabalski (pgrabalski@guralp.com)
 */

#ifndef INC_GDI_COMMANDS_H_
#define INC_GDI_COMMANDS_H_

#include "gdi_defs.h"

/*
 * This file contains declarations of GDI commands creators and handlers.
 * Implementation can be found in src/gdi_commands.c file.
 *
 * GDI command creators are used to build commands as defined in the protocol
 * specification. All of prepare command functions will return a newly allocated
 * GDI_Command structure which contains actual command length (not a command
 * payload defined in protocol specification) and a command data ready to send.
 * Note that all prepare command function are dynamically allocating memory
 * so memory should be released. Currently gdi_send function is freeing the
 * memory after send through socket.
 *
 * GDI command processors are used to process received buffer and apply any
 * changes provided by the commands to the server instance passed in the
 * function parameter. All of processor functions will return GDI_OK (0)
 * error code on successful command handle.
 *
 */

/**
 * \brief Prepares negotiation command.
 * @param name Identification name of this client
 * @return Prepared command structure containing length of the command and it's data. Buffer has to be freed after send.
 */
GDI_Command * gdi_prep_cmd_negotiation(char * name);

/**
 * \brief Prepares configuration command.
 * @param options A pointer to list of options provided/required by this client.
 * @param no_options Number of options in the list.
 * @return Prepared command structure containing length of the command and it's data. Buffer has to be freed after send.
 */
GDI_Command * gdi_prep_cmd_configuration(GDI_ConfigurationOpt * options, int no_options);

/**
 * \brief Prepares negotiation rejected command.
 * @return Prepared command structure containing length of the command and it's data. Buffer has to be freed after send.
 */
GDI_Command * gdi_prep_cmd_configuration_nak();

/**
 * \brief Prepares negotiation accepted command.
 * @return Prepared command structure containing length of the command and it's data. Buffer has to be freed after send.
 */
GDI_Command * gdi_prep_cmd_configuration_ack();

/**
 * \brief Prepares subscribe to channel command.
 * @param channel_id Id of channel to subscribe to.
 * @return Prepared command structure containing length of the command and it's data. Buffer has to be freed after send.
 */
GDI_Command * gdi_prep_cmd_subscribe_channel(int channel_id);

/**
 * \brief Prepares keep alive command.
 * @return Prepared command structure containing length of the command and it's data. Buffer has to be freed after send.
 */
GDI_Command * gdi_prep_cmd_keep_alive();

/**
 * \brief Negotiation command processing function.
 *
 * This command will process Negotiation command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_negotiation(GDI_Server * srv, char ** data, int *size);

/**
 * \brief Configuration command processing function.
 *
 * This command will process Configuration command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_configuration(GDI_Server * srv, char ** data, int *size);

/**
 * \brief Configuration NAK command processing function.
 *
 * This command will process Configuration NAK command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_configuration_nak(GDI_Server * srv, char ** data, int *size);

/**
 * \brief Configuration ACK command processing function.
 *
 * This command will process Configuration ACK command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_configuration_ack(GDI_Server * srv, char ** data, int *size);

/**
 * \brief New channel command processing function.
 *
 * This command will process New Channel command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_new_channel(GDI_Server * srv, char ** data, int *size);

/**
 * \brief New channel metadata command processing function.
 *
 * This command will process New Channel Metadata command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_new_metadata(GDI_Server * srv, char ** data, int *size);

/**
 * \brief New segment command processing function.
 *
 * This command will process New Segment command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_new_segment(GDI_Server * srv, char ** data, int *size);

/**
 * \brief End segment command processing function.
 *
 * This command will process End Segment command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_end_segment(GDI_Server * srv, char ** data, int *size);

/**
 * \brief Keep alive command processing function.
 *
 * This command will process Keep Alive command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * If input \p data buffer contains more than required command, handled command will be removed from
 * the beginning of the rx buffer so it will be prepared for next processing cycle.
 *
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_keep_alive(char ** data, int *size);

/**
 * \brief New data command processing function.
 *
 * This command will process New Data command and apply processing result to the server instance.
 *
 * If required command is not at the beginning of the received buffer (for example if there are some
 * leftovers from last receive cycle), the irrelevant commands from the beginning will be removed
 * from the buffer.
 *
 * Input \p data buffer will be processed until there are new data commands available.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_new_data(GDI_Server * srv, char ** data, int *size);

/**
 * \brief Marker command processing function.
 *
 * This command will process a Marker command.
 *
 * @param srv Pointer to server from which command comes from.
 * @param data Pointer to received buffer data pointer.
 * @param size Pointer to received buffer size.
 * @return GDI_OK (0) if succeeded, otherwise relevant error code.
 */
GDI_Error gdi_proc_cmd_marker(GDI_Server * srv, char ** data, int *size);

/**
 * \brief Decode GDI command type from input data.
 *
 * This command will decode command type from input data.
 *
 * @param data Data buffer to check the command type in.
 * @param size Size of data buffer.
 * @return GDI data command as defined in gdi_protocol.h
 */
int gdi_get_command_type(char * data, int size);

/**
 * \brief Decode GDI command length from input data.
 *
 * This command will decode command length from input data assuming
 * the command is formated correctly so first 4 bytes is a command type
 * and second 4 bytes is the length.
 *
 * @param data Data buffer to check the command type in.
 * @param size Size of data buffer.
 * @return GDI data command length
 */
uint32_t gdi_get_command_length(char * data, int size);


#endif /* INC_GDI_COMMANDS_H_ */
