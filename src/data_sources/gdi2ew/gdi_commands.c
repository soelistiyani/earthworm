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
 * gdi_commands.c
 *
 *  Created on: 3 Nov 2016
 *      Author: pgrabalski
 */

#include "project.h"

// ////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
// ////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * \brief Changes byte order in 16bit value.
 *
 * This is a simple re-implementation of htons/ntohs function.
 *
 * @param v Value to be reversed.
 * @return Reversed \p v value.
 */
static uint16_t reverse16(uint16_t v)
{
    return ( (v & 0xFF00) >> 8
           | (v & 0x00FF) << 8);
}

/**
 * \brief Changes byte order in 32bit value.
 *
 * This is a simple re-implementation of htonl/ntohl function.
 *
 * @param v Value to be reversed.
 * @return Reversed \p v value.
 */
static uint32_t reverse32(uint32_t v)
{
    return ( (v & 0xFF000000) >> 24
           | (v & 0x00FF0000) >> 8
           | (v & 0x0000FF00) << 8
           | (v & 0x000000FF) << 24);
}

/**
 * \brief Changes byte order in 64bit value.
 * @param v Value to be reversed.
 * @return Reversed \p v value.
 */
static uint64_t reverse64(uint64_t v)
{
    return ( (v & 0xFF00000000000000) >> 56
           | (v & 0x00FF000000000000) >> 40
           | (v & 0x0000FF0000000000) >> 24
           | (v & 0x000000FF00000000) >> 8
           | (v & 0x00000000FF000000) << 8
           | (v & 0x0000000000FF0000) << 24
           | (v & 0x000000000000FF00) << 40
           | (v & 0x00000000000000FF) << 56 );
}

/**
 * \brief Calculates size required for a string.
 *
 * This function will calculate a size needed by the string in GDI command.
 * String in GDI commands is described as 4 bytes long string length, followed
 * by the string characters:
 * \code
 * {
 *   uint32_t len;
 *   char *   string;
 * }
 * \endcode
 *
 * @param str
 * @return
 */
static int gdi_sizeof_str(char * str)
{
    return (  sizeof(uint32_t) /* length holder */
            + strlen(str) /* actual length of string */);
}

/**
 * \brief Writes 32bit value to a buffer provided.
 * @param buf Pointer to a buffer
 * @param value Value to be copied
 * @return Pointer advanced by size of uint32_t
 */
static char * gdi_w32bit(char * buf, uint32_t value)
{
    memcpy(buf, &value, sizeof(uint32_t));
    return buf+sizeof(uint32_t);
}

/**
 * \brief Writes GDI String to a buffer provided.
 * @param buf Pointer to a buffer
 * @param str GDI String to be added
 * @return Pointer advanced by size of GDI string
 */
static char * gdi_wStr(char * buf, GDI_String * str)
{
    uint32_t value = reverse32(str->len);
    memcpy(buf, &value, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    memcpy(buf, str->str, str->len);
    return buf + str->len;
}

/**
 * \brief Gets 32bit value from a buffer and advance the buffer.
 * @param buf Pointer to a buffer poitner.
 * @return Read value
 */
static uint32_t gdi_r32bit(char ** buf)
{
    uint32_t value = 0;
    memcpy(&value, *buf, sizeof(uint32_t));
    *buf = *buf + sizeof(uint32_t);
    return reverse32(value);
}

/**
 * \brief Gets 24bit value from a buffer and advance the buffer.
 * @param buf Pointer to a buffer pointer.
 * @return Read value
 */
static uint32_t gdi_r24bit(char ** buf)
{
    uint32_t value = 0;
    memcpy(&value, *buf, 3);
    *buf = *buf + 3;
    return reverse32(value) >> 8;
}

/**
 * \brief Gets 16bit value from a buffer and advance the buffer.
 * @param buf Pointer to a buffer pointer.
 * @return Read value
 */
static uint32_t gdi_r16bit(char ** buf)
{
    uint32_t value = 0;
    memcpy(&value, *buf, sizeof(uint16_t));
    *buf = *buf + sizeof(uint16_t);
    return reverse16(value);
}

/**
 * \brief Gets 8bit value from a buffer and advance the buffer.
 * @param buf Pointer to a buffer pointer.
 * @return Read value
 */
static uint8_t gdi_r8bit(char ** buf)
{
    uint8_t value = (uint8_t)(**buf);
    *buf = *buf + 1;
    return value;
}

/**
 * \brief Gets 64bit value from a buffer and advance the buffer.
 * @param buf Pointer to a buffer poitner.
 * @return Read value
 */
static uint64_t gdi_r64bit(char ** buf)
{
    uint64_t value = 0;
    memcpy(&value, *buf, sizeof(uint64_t));
    *buf = *buf + sizeof(uint64_t);
    return reverse64(value);
}

/**
 * \brief Gets string value from a buffer and advance the buffer.
 *
 * This function will read GDI string from input buffer and store it
 * in \p str parameter.
 *
 * @param buf Pointer to a buffer poitner.
 * @param str Pointer to GDI String instance that will contain read value.
 * @return Read value as a newly allocated GDI_String pointer
 */
static GDI_String * gdi_rStr(char ** buf, GDI_String * str)
{
    str->len = gdi_r32bit(buf);
    memset(str->str, 0, GDI_STRING_MAX_LEN);
    memcpy(str->str, *buf, str->len > GDI_STRING_MAX_LEN ? GDI_STRING_MAX_LEN : str->len);
    *buf = (*buf)+str->len;
    if (str->len > GDI_STRING_MAX_LEN)
    {
         return NULL;
    }
    return str;
}

// ////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
// ////////////////////////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS - COMMANDS PREPARATION FUNCTIONS
// Functions in this section prepares the commands to be send to the server.
// ////////////////////////////////////////////////////////////////////////////////////////////////

GDI_Command * gdi_prep_cmd_negotiation(char * name)
{
    // initialise
    GDI_Command * cmd = (GDI_Command *)calloc(1, sizeof(GDI_Command));
    char * ptr = NULL;
    uint64_t value64 = 0;

    // calculate length (parameters only)
    cmd->len = 2*sizeof(uint32_t) /* command type and length */
             + sizeof(uint64_t) /* protocol magic number */
             + gdi_sizeof_str(name); /* length of name */

    // allocate command buffer
    cmd->cmd = (char *)malloc(cmd->len * sizeof(char));
    ptr = cmd->cmd;

    // build command accordingly to protocol definition
    //
    // command type
    ptr = gdi_w32bit(ptr, reverse32(GDI_CMD_NEGOTIATION));

    // command length (parameters only)
    ptr = gdi_w32bit(ptr, reverse32(cmd->len-(2*sizeof(uint32_t))));

    // 1st parameter of negotiation command - protocol magic
    value64 = reverse64(GDI_NEGOTIATION_RX_MAGIC); // receiver magic as this is client
    memcpy(ptr, &value64, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    // 2nd parameter of negotiation command - identifying name
    // GDI string parameter is build from length of the string followed by the actual string
    GDI_String str;
    str.len = strlen(name);
    strcpy(str.str, name);
    ptr = gdi_wStr(ptr, &str);

    // return command
    return cmd;
}

GDI_Command * gdi_prep_cmd_configuration(GDI_ConfigurationOpt * options, int no_options)
{
    // initialise
    GDI_Command * cmd = (GDI_Command *)calloc(1, sizeof(GDI_Command));
    char * ptr = NULL;
    uint32_t value = 0;
    int counter = 0;
    int opt_len = 0;

    // calculate length (parameters only)
    cmd->len = 8;
    for (counter = 0; counter < no_options; counter++)
    {
        cmd->len += sizeof(uint32_t) /* configuration option (uint24) + configuration word_length(uint8_t) = uint32_t */
                 +  ((options[counter].option_and_wlen & 0xFF) * 4); // number of words (last byte from option_and_wlen) * 4 bytes word length
    }

    // allocate command buffer
    cmd->cmd = (char *)malloc(cmd->len * sizeof(char));
    ptr = cmd->cmd;

    // build command accordingly to protocol definition
    //
    // command type
    ptr = gdi_w32bit(ptr, reverse32(GDI_CMD_CONFIG));

    // command length (parameters only)
    ptr = gdi_w32bit(ptr, reverse32(cmd->len-(2*sizeof(uint32_t))));


    // add all of the options specified
    for (counter = 0; counter < no_options; ++counter)
    {
        opt_len = ( (options[counter].option_and_wlen & 0xFF) * 4 /* 4 bytes word length */);
        value = (reverse32(options[counter].option_and_wlen >> 8)) << 8; /* reverse 24bit option first and shift back*/
        value |= options[counter].option_and_wlen & 0xFF; /* add 8bits of wlen */
        memcpy(ptr, &options[counter].value, opt_len * sizeof(int8_t)); /* copy the option value */
        ptr+=sizeof(uint8_t) * opt_len; /* advance offset */
    }

    // return command
    return cmd;
}

GDI_Command * gdi_prep_cmd_configuration_nak(GDI_ConfigurationOpt * rejected_option)
{
    if (rejected_option == NULL)
    {
        // looks rejection reason is missing
        return NULL;
    }

    // initialise
    GDI_Command * cmd = (GDI_Command *)calloc(1, sizeof(GDI_Command));
    char * ptr = NULL;

    // calculate length (parameters only)
    cmd->len = 2*sizeof(uint32_t) /* command type and length */
             + sizeof(uint32_t) /* configuration option (24bits) + word length (8bits) = 32bits value */
             + ((rejected_option->option_and_wlen & 0xFF) * 4); /* option length in words * 4bytes of single word length */


    // allocate command buffer
    cmd->cmd = (char *)malloc(cmd->len * sizeof(char));
    ptr = cmd->cmd;

    // build command accordingly to protocol definition
    //
    // command type
    ptr = gdi_w32bit(ptr, reverse32(GDI_CMD_CONFIG_NAK));

    // command length (parameters only)
    ptr = gdi_w32bit(ptr, reverse32(cmd->len-(2*sizeof(uint32_t))));

    // copy rejected option
    memcpy(ptr, (char *)(rejected_option), cmd->len);

    // return command
    return cmd;
}

GDI_Command * gdi_prep_cmd_configuration_ack()
{
    // initialise
    GDI_Command * cmd = (GDI_Command *)calloc(1, sizeof(GDI_Command));
    char * ptr = NULL;

    // calculate length (parameters only)
    cmd->len = 2*sizeof(uint32_t) /* command type and length */
             + 0; // no payload for configuration ACK command

    // allocate command buffer
    cmd->cmd = (char *)malloc(cmd->len * sizeof(char));
    ptr = cmd->cmd;

    // build command accordingly to protocol definition
    //
    // command type
    ptr = gdi_w32bit(ptr, reverse32(GDI_CMD_CONFIG_ACK));

    // command length (parameters only)
    ptr = gdi_w32bit(ptr, reverse32(cmd->len-(2*sizeof(uint32_t))));

    // return command
    return cmd;
}

GDI_Command * gdi_prep_cmd_subscribe_channel(int channel_id)
{
    // initialise
    GDI_Command * cmd = (GDI_Command *)calloc(1, sizeof(GDI_Command));
    char * ptr = NULL;

    // calculate length (parameters only)
    cmd->len = 2*sizeof(uint32_t) /* command type and length */
             + sizeof(uint32_t); /* size of channel ID to subscribe to */

    // allocate command buffer
    cmd->cmd = (char *)malloc(cmd->len * sizeof(char));
    ptr = cmd->cmd;

    // build command accordingly to protocol definition
    //
    // command type
    ptr = gdi_w32bit(ptr, reverse32(GDI_CMD_SUBSCRIBE_CHANNEL));

    // command length (parameters only)
    ptr = gdi_w32bit(ptr, reverse32(cmd->len-(2*sizeof(uint32_t))));

    // add channel ID of subscribed channel
    ptr = gdi_w32bit(ptr, reverse32(channel_id));

    // return command
    return cmd;
}

GDI_Command * gdi_prep_cmd_keep_alive()
{
    // initialise
    GDI_Command * cmd = (GDI_Command *)calloc(1, sizeof(GDI_Command));
    char * ptr = NULL;

    // calculate length (parameters only)
    cmd->len = 2*sizeof(uint32_t) /* command type and length */
             + 0; // no payload for keep alive command

    // allocate command buffer
    cmd->cmd = (char *)malloc(cmd->len * sizeof(char));
    ptr = cmd->cmd;

    // build command accordingly to protocol definition
    //
    // command type
    ptr = gdi_w32bit(ptr, reverse32(GDI_CMD_KEEPALIVE));

    // command length (parameters only)
    ptr = gdi_w32bit(ptr, reverse32(cmd->len-(2*sizeof(uint32_t))));

    // return command
    return cmd;
}

// ////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS - COMMAND PROCESSING FUNCTIONS
// Functions in this section processes commands provided and applies the result of processing to
// the server instance from which the command came from.
// ////////////////////////////////////////////////////////////////////////////////////////////////

GDI_Error gdi_proc_cmd_negotiation(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t cmd_len = 0;
    uint32_t value = 0;
    uint64_t value64 = 0;
    GDI_String srv_name;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_NEGOTIATION)
    {
        // wrong command
        error(("GDI_ERR_WrongCommandProcessor\n"));
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        error(("GDI_ERR_CommandNotFullyReceived\n"));
        return GDI_ERR_CommandNotFullyReceived;
    }

    // get 1st command parameter - protocol magic
    value64 = gdi_r64bit(&ptr);
    if (value64 != GDI_NEGOTIATION_TX_MAGIC)
    {
        // not a connection to server
        error(("GDI_ERR_NonServerConnection\n"));
        return GDI_ERR_NonServerConnection;
    }

    // get 2nd command parameter - identifying name (of server)    
    gdi_rStr(&ptr, &srv_name);
    memset(srv->name, 0, GDI_MAX_NAME);
    strncpy(srv->name, srv_name.str, GDI_MAX_NAME);

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_configuration(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t cmd_len = 0;
    uint32_t option = 0;
    uint32_t wlen = 0;
    uint32_t len_left = 0;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_CONFIG)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }
    len_left = cmd_len;
    // clear previous options if exists

    srv->no_options = 0;

    // parse options
    while (len_left > 0)
    {
        // read option and word len
        value = gdi_r32bit(&ptr);
        option = value & 0x00FFFFFF; // after byte reverse option is held at 0x00FFFFFF mask
        wlen = value >> 24;          // after byte reverse wlen is held at 0xFF000000 mask
        if (srv->no_options+1 > GDI_MAX_OPT)
        {
            error(("Exceeded number of allowed options.\n"));
        }
        srv->options[srv->no_options].option_and_wlen = option | (wlen << 24);
        srv->options[srv->no_options].value = (int8_t *)calloc(wlen, 4);
        memcpy(srv->options[srv->no_options].value, ptr, wlen*4);
        ptr += wlen*4;
        srv->no_options++;
        len_left -= sizeof(uint32_t) /* option and wlen */ + (wlen*4) /* actuall option length */;
    }

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_configuration_nak(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t cmd_len = 0;
    uint32_t option = 0;
    uint32_t wlen = 0;
    GDI_String rejection_reason;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_CONFIG_NAK)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    // parse rejected option

    // read option and word len
    value = gdi_r32bit(&ptr);
    option = value & 0x00FFFFFF; // after byte reverse option is held at 0x00FFFFFF mask
    wlen = value >> 24;          // after byte reverse wlen is held at 0xFF000000 mask
    gdi_rStr(&ptr, &rejection_reason);

    error(("Option %08X (of word length %d) has been rejected by server. Reason given: %s\n", option, wlen, rejection_reason.str));

    srv->state = GDI_State_Close_Connection;

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_configuration_ack(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t cmd_len = 0;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_CONFIG_ACK)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    srv->state = GDI_State_Connection_Negotiated;

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_new_channel(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t cmd_len = 0;
    int32_t len_left = 0;
    GDI_String str;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_NEW_CHANNEL)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (uint32_t)(*size) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    // add new channel to server instance
    if (srv->no_channels+1 > GDI_MAX_CHANNELS)
    {
        return GDI_ERR_SizeLimitReached;
    }
    // process data and add it as last channel
    srv->channels[srv->no_channels].id = gdi_r32bit(&ptr);  // read channel ID

    gdi_rStr(&ptr, &str);   // read channel name
    srv->channels[srv->no_channels].name = strdup(str.str);
    srv->channels[srv->no_channels].sample_rate_num = gdi_r32bit(&ptr); // read sample rate number
    srv->channels[srv->no_channels].sample_rate_div = gdi_r32bit(&ptr); // read sample rate divisor
    srv->channels[srv->no_channels].sample_format = gdi_r32bit(&ptr); // read number of samples

    len_left = cmd_len - 20 - strlen(srv->channels[srv->no_channels].name) ;

    // get metadata
    while (len_left > 0)
    {
        if (srv->channels[srv->no_channels].no_metadata+1 > GDI_MAX_METADATA)
        {
            // cannot accept more metadata
            break;
        }

        // get metadata field (key) value
        gdi_rStr(&ptr, &str);
        srv->channels[srv->no_channels].metadata[srv->channels[srv->no_channels].no_metadata].key = strdup(str.str);
        len_left -= (str.len+4);

        // get metadata value
        gdi_rStr(&ptr, &str);
        srv->channels[srv->no_channels].metadata[srv->channels[srv->no_channels].no_metadata].value = strdup(str.str);
        len_left -= (str.len+4);

        // advance metadata counter
        srv->channels[srv->no_channels].no_metadata++;
    }

    // increment channels counter
    srv->no_channels++;

    if ( srv->new_channel_handler )
        (*srv->new_channel_handler)( &(srv->channels[srv->no_channels-1]) );

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_new_metadata(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t channel_id = 0;
    uint32_t cmd_len = 0;
    uint32_t len_left = 0;
    GDI_String str;
    GDI_Channel * chan = NULL;
    GDI_ChannelMetadata meta;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_NEW_METADATA)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }
    len_left = cmd_len - sizeof(uint32_t); // size of channel id

    // get 1str parameter - channel id
    channel_id = gdi_r32bit(&ptr);

    // get channel from server instance
    chan = gdi_srv_get_channel(srv, channel_id);

    // if channel is registered in server instance
    if (NULL != chan)
    {
        // start parsing
        while (len_left > 0)
        {
            // get metadata key
            if (NULL == gdi_rStr(&ptr, &str))
            {
                break;
            }
            meta.key = strdup(str.str);
            len_left -= (str.len-sizeof(uint32_t));

            // get metadata value
            if (NULL == gdi_rStr(&ptr, &str))
            {
                break;
            }
            meta.value = strdup(str.str);
            len_left -= (str.len-sizeof(uint32_t));

            // add/update metadata on server
            gdi_srv_add_metadata(srv, chan, &meta);

            // free metadata entities
            free(meta.key);
            free(meta.value);
        }
    }

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_new_segment(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t channel_id = 0;
    uint32_t cmd_len = 0;
    GDI_Channel * chan = NULL;
    GDI_Segment * segment = NULL;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_NEW_SEGMENT)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    // get 1str parameter - channel id
    channel_id = gdi_r32bit(&ptr);

    // get channel from server instance
    chan = gdi_srv_get_channel(srv, channel_id);

    // if channel is registered in server instance
    if (NULL != chan)
    {
        value = gdi_r32bit(&ptr); // segment id

        // get segment from server instance
        segment = gdi_srv_get_segment(srv, channel_id, value);

        if (segment == NULL)
        {
            // no segment, lets create new one
            if (chan->no_segment+1 > GDI_MAX_SEGMENTS)
            {
                segment = &chan->segment[chan->no_segment-1];
            } else {
                segment = &chan->segment[chan->no_segment];
                chan->no_segment++;
            }
        }

        segment->id = value;
        segment->start_day = gdi_r32bit(&ptr);
        segment->start_sec = gdi_r32bit(&ptr);
        segment->start_nsec = gdi_r32bit(&ptr);
        segment->last_sample_day = segment->start_day;
        segment->last_sample_sec = segment->start_sec;
        segment->last_sample_nsec = segment->start_nsec;

        // if extended new segment command
        if (cmd_len > 5*sizeof(uint32_t))
        {
            segment->sample_rate_num = gdi_r32bit(&ptr);
            segment->sample_rate_div = gdi_r32bit(&ptr);
        } else {
            // otherwise set as in channel
            segment->sample_rate_num = chan->sample_rate_num;
            segment->sample_rate_div = chan->sample_rate_div;
        }

        segment->completed = 0;
        segment->parent_channel = chan;

    }

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_end_segment(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t cmd_len = 0;
    GDI_Segment * segment = NULL;

    // get first value from command buffer - command type
    value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_END_SEGMENT)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    // get parameter - segment id
    value = gdi_r32bit(&ptr);

    // get channel from server instance
    segment = gdi_srv_get_segment_by_id(srv, value);

    if (segment != NULL)
    {
        segment->completed = 1;
    }

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_keep_alive(char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;

    if (*size < (int)(2*sizeof(uint32_t)))
    {
        // not fully received
        return GDI_ERR_CommandNotFullyReceived;
    }

    // get first value from command buffer - command type
    value = gdi_r32bit (&ptr);
    if (value != GDI_CMD_KEEPALIVE)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_new_data(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    uint32_t value = 0;
    uint32_t cmd_len = 0;
    uint32_t segment_id = 0;

    // get first value from command buffer - command type
    value = gdi_r32bit (&ptr);
    if (!(value & GDI_CMD_NEW_DATA_ID))
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    segment_id = (value & 0x0FFFFFFF);

    // get second value from command buffer - command length
    cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    if (cmd_len == 4)
    {
        // alternative new data - only one, uncompressed sample in the buffer
        int32_t val = gdi_r32bit(&ptr);
        if (gdi_srv_is_accepting_segment(srv, segment_id) && srv->sample_handler != NULL)
        {
            (*srv->sample_handler)(segment_id, &val, 1);
        }
        gdi_srv_advance_segment_time(srv, segment_id, 1);
    } else {
        // regular new data command with compressed samples in the buffer and SOH information at the beginning
        uint8_t soh_words = gdi_r8bit(&ptr);
        uint32_t no_samples = gdi_r24bit(&ptr);

        char * soh = NULL;
        if (soh_words > 0)
        {
            soh = (char *)calloc(soh_words*4, sizeof(char));
            char * soh_ptr = soh;
            while (soh_words > 0)
            {
                value = gdi_r32bit(&ptr);
                memcpy(soh_ptr, &value, 4);
                soh_ptr+=4;
                soh_words--;
            }
        }
        int32_t * samples = (int32_t*)calloc(no_samples, sizeof(int32_t));
        uint32_t i = 0;
        int32_t val = 0;
        int32_t ref = 0;
        // data send by GDI is a difference between the following samples
        for (i=0; i<no_samples; ++i)
        {
            // unpack 32 signed sample
            ptr += vint_unpack_s32(ptr, cmd_len-4, &val);
            ref += val; // the sample is a difference of previous and current
            samples[i] = ref;
        }
        if (gdi_srv_is_accepting_segment(srv, segment_id) && srv->sample_handler != NULL)
        {
            (*srv->sample_handler)(segment_id, samples, no_samples);
        }
        free(samples);
        gdi_srv_advance_segment_time(srv, segment_id, no_samples);
    }

    return GDI_OK;
}

GDI_Error gdi_proc_cmd_marker(GDI_Server * srv, char ** data, int *size)
{
    char * ptr = *data;
    GDI_String str;

    // get first value from command buffer - command type
    uint32_t value = gdi_r32bit(&ptr);
    if (value != GDI_CMD_MARKER)
    {
        // wrong command
        return GDI_ERR_WrongCommandProcessor;
    }

    // get second value from command buffer - command length
    uint32_t cmd_len = gdi_r32bit(&ptr);
    if (cmd_len > (*size)-(2*sizeof(uint32_t)) ) // if read length is greater than received size - length of type - length of length
    {
        // looks like there is not enough received yet
        return GDI_ERR_CommandNotFullyReceived;
    }

    uint32_t start_day = gdi_r32bit(&ptr);
    uint32_t start_sec = gdi_r32bit(&ptr);
    uint32_t start_nsec = gdi_r32bit(&ptr);
    uint16_t event_code = gdi_r16bit(&ptr);
    uint16_t num_chans = gdi_r16bit(&ptr);
    char* chan_list = ptr; // save this reference for later
    ptr += sizeof(uint32_t)*num_chans; // skip channel list for now
    gdi_rStr(&ptr, &str);

    // FUTURE expansion - pass this marker on somehow. For now, just log as info.
    info1 (("GDI Marker received from '%s', code=%d, comment='%s'", srv->name, event_code, str.str));

    return GDI_OK;
}


// ////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS - HELPER FUNCTIONS
// ////////////////////////////////////////////////////////////////////////////////////////////////

int gdi_get_command_type(char * data, int size)
{
    if (size < 4)
    {
        return 0xFFFFFFFF;
    }
    char * ptr = data;
    return gdi_r32bit(&ptr);
}


uint32_t gdi_get_command_length(char *data, int size)
{
    if (size < 8)
    {
        return 0;
    }
    char * ptr = data+4;
    return gdi_r32bit(&ptr);
}
