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
 * gdi_defs.h
 *
 *      Author: pgrabalski (pgrabalski@guralp.com)
 */

#ifndef INC_GDI_DEFS_H_
#define INC_GDI_DEFS_H_

#include <platform.h>

/*
 * This file contains various definitions used in this GDI implementation.
 *
 * Sections that can be found in this file:
 * - Constants              - A global defines of constant values
 * - Logging                - Logging macros
 * - Enumerator types       - Enumerator types containing known values such as:
 *                            > GDI error codes, or
 *                            > GDI connection states
 * - Function prototypes    - Function type definitions
 * - Structures             - Definition of various structures used in implementation
 *                            > GDI_Command
 *                            > GDI_String
 *                            > GDI_Server
 *                            > GDI_Channel
 *                            > GDI_Segment
 *                            > GDI_ConfigurationOpt
 *                            > GDI_ChannelMetadata
 */

// CONSTANTS SECTION ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define GDI_DEFAULT_PORT 1565   //!< Defines default GDI port as 1565

#define GDI_MAX_NAME 80         //!< Defines maximum server name length
#define GDI_MAX_SEGMENTS 500    //!< Defines maximum number of segments kept in Channel structure
#define GDI_MAX_METADATA 100    //!< Defines maximum number of metadata that can be kept in Channel structure
#define GDI_MAX_CHANNELS 500    //!< Defines maximum number of channels that this GDI implementation can work with
#define GDI_MAX_OPT 50          //!< Defines maximum number of options sent by channel
#define GDI_STRING_MAX_LEN 512  //!< Defines maximum length of string that this GDI implementation can work with
#define GDI_DATAPOOL_SIZE 4000  //!< Defines size of local server data pool

#define GDI_EPOCHDAY (719528L)      //!< offset between GDI time and EW (epoch) time
#define GDI_SECINDAY (86400L)       //!< Number of seconds in a standard day (excl. leap seconds of course)

// ENUMERATOR TYPES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * \brief Defines known GDI errors.
 */
typedef enum
{
    GDI_OK = 0,                         //!< No Error
    GDI_ERR_CannotConnect,              //!< Client could not make a connection to server
    GDI_ERR_AlreadyConnected,           //!< Client attempt to connect when it already has been connected
    GDI_ERR_UninitialisedParameter,     //!< Uninitialised parameter passed to function
    GDI_ERR_CouldntSend,                //!< Client failed on sending data
    GDI_ERR_WrongCommandProcessor,      //!< Command has been passed to wrong command processing function
    GDI_ERR_CommandNotFullyReceived,    //!< Command length is greater than received buffer
    GDI_ERR_NonServerConnection,        //!< Client connected to non server device - wrong negotiation TX magic received
    GDI_ERR_SizeLimitReached,           //!< Size limit reached
    GDI_ERR_Disconnected,               //!< Client is in disconnected state
    GDI_CMD_Rejected,                   //!< Command rejected
    GDI_CMD_NothingToParseInBuffer,     //!< Empty buffer
} GDI_Error;

/**
 * \brief Defines known GDI Client states.
 */
typedef enum
{
    GDI_State_Send_Negotiation = 0,         //!< GDI Client sends negotiation step
    GDI_State_Await_Negotiation,            //!< GDI Client awaits for negotiation from server
    GDI_State_Send_Configuration,           //!< GDI Client sends configuration
    GDI_State_Await_Configuration_Approval, //!< GDI Client awaits for configuration approval
    GDI_State_Await_Configuration,          //!< GDI Client awaits for configuration from server
    GDI_State_Send_Configuration_Approval,  //!< GDI Client sends configuration approval
    GDI_State_Connection_Negotiated,        //!< GDI Client is in working state - Negotiation done
    GDI_State_Out_Of_Sync,                  //!< GDI Client is in undefined state because of loosing sync with server

    GDI_State_Close_Connection              //!< GDI Client connection closed
} GDI_ConnectionState;

// to satisfy compiler
struct GDI_Channel;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * \brief GDI Sample handler function definition.
 *
 * GDI sample handler function should take as parameters:
 * \p segment ID - to look up in stored segment describtions for sampling rate and last sample time
 * \p data - a pointer to data
 * \p number of samples - number of samples to be processed
 * \return This type definition will return GDI error code (0 if all good)
 */
typedef GDI_Error (*gdi_sample_handler)(uint32_t segment_id, int32_t * data, int no_samples);

/**
 * \brief GDI New channel handler function definition.
 *
 * GDI new channel handler function should take as parameters:
 * \p chan pointer to GDI_Channel struct for the newly added channel
 * \return This type definition will return GDI error code (0 if all good)
 */
typedef GDI_Error (*gdi_new_channel_handler)(struct GDI_Channel* chan);

/**
 * \brief GDI thread terminated notification function definition.
 *
 * GDI thread terminated notification should take as parameters:
 * \p server The server object that was given to this thread on startup
 */
typedef struct GDI_Server GDI_Server;
typedef void (*gdi_thread_terminated)(GDI_Server* server);

// STRUCTURE DEFINITIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * \brief Definition of GDI Command used for sending.
 *
 * Parameters:
 * int len      - Length of command buffer
 * char * cmd   - Dynamically allocated command buffer
 *
 */
typedef struct
{
    int    len; //!< Length of \p cmd command buffer
    char * cmd; //!< Dynamically allocated command buffer. Has to be freed after use.
} GDI_Command;

/**
 * \brief Defines GDI String structure.
 */
typedef struct
{
    uint32_t len;                   //!< Length of string held
    char str[GDI_STRING_MAX_LEN];   //!< String held
} GDI_String;

/**
 * \brief Defines GDI Configuration option structure.
 */
typedef struct
{
    uint32_t option_and_wlen;           //!< Defines uint24_t option and uint8_t word length
    int8_t *value;                      //!< Defines a value buffer
} GDI_ConfigurationOpt;

/**
 * \brief Defines GDI Channel metadata structure.
 */
typedef struct
{
    char * key;         //!< Key
    char * value;       //!< Value
} GDI_ChannelMetadata;

/**
 * \brief Defines GDI Segment details.
 */
typedef struct
{
    uint32_t id;                            //!< Segment ID
    uint32_t start_day;                     //!< Segment start time [DAY]
    uint32_t start_sec;                     //!< Segment start time [SECONDS]
    uint32_t start_nsec;                    //!< Segment start time [NANOSECONDS]
    uint32_t sample_rate_num;               //!< Sampling rate of this segment [NUMERATOR]
    uint32_t sample_rate_div;               //!< Sampling rate of this segment [DIVISOR]
    uint8_t completed;                      //!< Defines if segment is completed - will not receive any more data
    uint32_t last_sample_day;               //!< Last sample time [DAY] - adjusted accordingly to samples received in life transmition
    uint32_t last_sample_sec;               //!< Last sample time [SECONDS] - adjusted accordingly to samples received in life transmition
    uint32_t last_sample_nsec;              //!< Last sample time [NANOSECONDS] - adjusted accordingly to samples received in life transmition
    struct GDI_Channel * parent_channel;    //!< Pointer to parent channel of this segment
} GDI_Segment;

/**
 * \brief Defines GDI Channel details.
 */
typedef struct GDI_Channel
{
    uint32_t id;                                    //!< Channel ID
    char * name;                                    //!< Channel name
    uint32_t sample_rate_div;                       //!< General sampling rate divisor in channel - if not specified differently in segment
    uint32_t sample_rate_num;                       //!< General sampling rate numerator in channel - if not specified differently in segment
    uint32_t sample_format;                         //!< Samples format
    GDI_ChannelMetadata metadata[GDI_MAX_METADATA]; //!< Metadata of this channel
    int no_metadata;                                //!< Number of metadata entries
    GDI_Segment segment[GDI_MAX_SEGMENTS];          //!< Segments held by this channel
    int no_segment;                                 //!< Number of segments held
    uint8_t subscribed;                             //!< Specifies if this channel is subscribed by client
    int list_index;                                 //!< Help field defines index of this channel in list
    void* user_ptr;                                 //!< User-definable pointer, not used by GDI-lib.
} GDI_Channel;

/**
 * \brief Defines GDI Server connection structure.
 */
struct GDI_Server
{
    char name[GDI_MAX_NAME];                     //!< Server name - usually a hostname
    GDI_Channel channels[GDI_MAX_CHANNELS];      //!< Channels provided by this server
    int no_channels;                             //!< Number of channels used
    GDI_ConfigurationOpt options[GDI_MAX_OPT];   //!< Configuration options provided by this server
    int no_options;                              //!< Number of options
    struct sockaddr_in conn;                     //!< Connection socket
    int conn_handle;                             //!< Connection address
    GDI_ConnectionState state;                   //!< Client/Server relation state
    //thread_t thread;                             //!< GDI client thread
    ew_thread_t thread_id;                      //!< Keeps thread ID
    mutex_t mutex;                               //!< This server connection mutex
    gdi_sample_handler sample_handler;           //!< Sample handler registered for this server connection
    gdi_new_channel_handler new_channel_handler; //!< New channel handler registered for this server connection
    gdi_thread_terminated on_thread_terminated;  //!< callback when thread is terminating

    char datapool[GDI_DATAPOOL_SIZE];            //!< Defines this connection data pool
};


#endif /* INC_GDI_DEFS_H_ */
