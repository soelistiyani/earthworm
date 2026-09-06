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
 * gdi_protocol.h
 *
 *      Author: pgrabalski (pgrabalski@guralp.com)
 */

#ifndef GDI_PROTOCOL_H_
#define GDI_PROTOCOL_H_

#include <stdint.h>


/*
 * This file contains all of the commands specified by protocol.
 */

/**
    NEGOTIATION
    ----------------

    Type: 0x00000000
    Value:  [uint64] Protocol magic
            [string] Identifying name

    Magic sent by rx module: 0xB50E2A92A2DB136B
    Magic sent by tx module: 0x1E33C911D720291A

    This packet is sent by each peer upon establishment of the TCP link. The magic
    number serves to ensure that the two modules are, in fact, talking the same
    protocol, and that an rx module is connected to a tx module and not another rx
    module (or vice-versa); the identifying name is used solely for human-readable
    identification purposes.
 */
#define GDI_CMD_NEGOTIATION 0x00000000

// negotiation RX magic
#define GDI_NEGOTIATION_RX_MAGIC 0xB50E2A92A2DB136BULL
// negotiation TX magic
#define GDI_NEGOTIATION_TX_MAGIC 0x1E33C911D720291AULL

/**
    CONFIG
    ----------------

    Type: 0x00000001
    Value:  n * {
                [uint24] option
                [uint8 ] word_length {w}
                [p*char] value (where p = {w} * 4)
            }

    This packet is sent by a module once it has received its peer's NEGOTIATION
    packet. It contains a list of capabilities which the peer considers will be
    active for the session.

    The payload consists of zero or more capability blocks. It may be empty. Each
    block has a 32-bit header which includes the length of the capability field,
    allowing a peer which does not understand the capability to skip over it. The
    length is given in 32-bit words (so a length value of '3' would mean 12 bytes).

    Note that early gdi-link receivers had a bug where they would not expect a
    CONFIG packet to be received after a CONFIG_ACK. (The same is true for
    transmitters, but was fixed hopefully long enough before the introduction of
    any receiver capability that it is extremely unlikely there will be a problem).

    Currently defined capabilities are:

        TERM_PORTNAME

        Type: 0x00000100
        Payload: none
        Sent by: transmitter

        Specifies that the transmitter may use TERMINAL_NEW2 messages, which have
        an additional port name field.

        SCTL2

        Type: 0x00000200
        Payload: none
        Sent by: transmitter

        Specifies that the transmitter may use SCTL2 messages, implementing a new
        extensible sensor control subsystem that is not tied to the DM24 terminal.
 */
#define GDI_CMD_CONFIG 0x00000001LL

// capability TERM_PORTNAME
#define GDI_CONFIG_CAPABILITY_TERM_PORTNAME  0x00000100
// capability SCTL2
#define GDI_CONFIG_CAPABILITY_SCTL2          0x00000200

/**
    CONFIG_NAK
    ----------------

    Type: 0x00000002
    Value:  [uint24] option
            [uint8 ] word_length
            [string] reason for rejection

    This packet is sent by a module if it rejects its peer's CONFIG. If the
    rejection was due to the presence (or perhaps absence) of a given option, that
    option can be identified; otherwise, set 'option' to 0xFFFFFF.

    Upon receipt of a CONFIG_NAK, a peer may choose to close the link or it may
    choose to issue a revised CONFIG.
 */
#define GDI_CMD_CONFIG_NAK 0x00000002

/**
    CONFIG_ACK
    ----------------

    Type: 0x0000000E
    Value:  (no payload)

    Sent by a module once it has accepted its peers CONFIG. The peer may now send
    other packets.
 */
#define GDI_CMD_CONFIG_ACK 0x0000000E

/**
    CHANNEL
    ----------------

    Type: 0x00000003
    Value:  [uint32] chan_id
            [string] chan_name
            [uint32] sample_rate.num
            [uint32] sample_rate.div
            [uint32] format
            n * {
                [string] metadata_field
                [string] metadata_value
            }

    Asynchronous notification of a new channel. Only one will ever be seen per
    chan_id (although note that the chan_ids may not be contiguous). The chan_id
    will be used to identify the channel in all further packets from either peer.

    The format is enumerated as follows:
        0x00000000              32-bit integer samples
        0x00000001              24-bit integer samples (24-bit is nominal range)
        0x00000002              16-bit integer samples (16-bit is nominal range)
        0x00000003              32-bit IEEE floating point samples
        0x00000004              Text/opaque data

    The chan_id for text channels is used as an identifier in NEW_TEXT packets; it
    is therefore limited to 28 bits. The likelihood of a real gdi system ever
    reaching 2^28 active channels is considered negligible.
 */
#define GDI_CMD_NEW_CHANNEL 0x00000003

// 32-bit integer samples format
#define GDI_CHANNEL_FORMAT_INT32       0x00000000
// 24-bit integer samples format
#define GDI_CHANNEL_FORMAT_INT24       0x00000001
// 16-bit integer samples format
#define GDI_CHANNEL_FORMAT_INT16       0x00000002
// 32-bit IEEE floating point samples format
#define GDI_CHANNEL_FORMAT_IEEE32FLOAT 0x00000003
// Text/opaque data samples format
#define GDI_CHANNEL_FORMAT_TEXT        0x00000004

/**
    SUBSCRIBE_CHANNEL
    ----------------

    Type 0x00000004
    Value:  [uint32] chan_id

    Sent by an rx client to subscribe to realtime data for a given channel.
 */
#define GDI_CMD_SUBSCRIBE_CHANNEL 0x00000004

/**
    NEW_METADATA
    ----------------

    Type: 0x00000005
    Value:  [uint32] chan_id
            n * {
                [string] metadata_field
                [string] metadata_value
            }

    Sent by a tx client whenever a channel to which its peer has subscribed gets
    updated metadata.
 */
#define GDI_CMD_NEW_METADATA 0x00000005

/**
    NEW_SEGMENT
    ----------------

    Type: 0x00000006
    Value:  [uint32] chan_id
            [uint32] seg_id
            [uint32] day
            [uint32] sec
            [uint32] nsec
            [uint32] sample_rate_num        // extended version
            [uint32] sample_rate_div        // extended version

    Sent by a tx client whenever a segment for a channel to which it has subscribed
    is created (either due to real-time data flow or backfill). seg_id is a global
    segment identifier which will be used to identify this segment, and its
    associated channel, in all future packets.

    Note that seg_ids are contiguous per link (starting at 0) and are not reused
    for different chan_ids, unlike gdi itself. The seg_id is used as part of the
    packet type in the rest of the protocol; this limits it to 28 bits. If a link
    instance ever reaches its 2^28th segment, the IDs would wrap around; the link
    should be dropped to avoid this. This will probably never happen in practice.

    Segment time is defined as day, second and nanoseconds since date 0 (0.0.0 00:00:00).

    Sample rate fields are available in extended version, the length of new segment
    packet is then equal to 7 * sizeof(uint32) which is build from 5 standard fields,
    plus 2 additional ones.
 */
#define GDI_CMD_NEW_SEGMENT 0x00000006

/**
    END_SEGMENT
    ----------------

    Type: 0x0000000D
    Value:  [uint32] seg_id

    Sent by the tx client to signify the end of a segment.
 */
#define GDI_CMD_END_SEGMENT 0x0000000D

/**
    NEW_DATA
    ----------------

    Type: 0x1SSSSSSS
    Value:  [uint8 ] soh_word_length {w}
            [uint24] num_samples
            [p*char] SOH (where p = {w} * 4)
            [n*char] samples (where n = remainder of the packet)

    Sent by a tx client whenever sample/SOH data becomes available for a channel.
    The message type contains the seg_id in the bottom 28 bits.

    The first byte records the number of SOH words in the packet; the next three
    contain the number of samples. This 32-bit header is followed by any SOH data
    and then the sample data making up the remainder of the packet.

    [Future plan: for floating point data channels, the samples are transmitted as
    uncompressed IEEE floating point values. For integer channels, samples will be
    compressed using Canadian compression, with unused samples being given the
    value 0. At present this is not implemented; when it is implemented in future
    it will be negotiated via the CONFIG mechanism.]

    Currently samples are first interpreted as 32-bit signed integers (even
    treating floating point numbers as such), and then compressed using a variable
    length packing scheme as defined in libgslutil. The first sample is written
    verbatim to the data section. Then (n-1) differences between samples are
    written. If the samples are y(t) then what is written for n samples is:

    [packed y(0)]
    [packed y(1)-y(0)]
    [packed y(2)-y(1)]
    ...
    [packed y(n-1)-y(n-2)]

    Text reproduced from libgslutil documentation regarding the packing:

    This module provides a set of functions for packing and unpacking integers
    to/from an efficient form which contains only the bits of the number that are
    actually used. The resulting packed objects are a variable number of bytes long
    (from 1 to 10, for a full 64-bit number). This packing is most efficient for
    integers of small numerical magnitude.

    The first bit of each byte is the continuation bit. If it is set, then another
    byte of data follows. If it is unset, then this is the last byte of data. In a
    multi-byte number, the last byte can never be 0x00; this means that there is no
    ambiguity of representation.

    For an unsigned number, the first byte contains the 7 least significant bits of
    the number. Each byte thereafter contains the 7 next most significant bits.
    Once all bits set in the original number have been stored in bytes, the packing
    is complete.

    Signed numbers are not transmitted as 2's complement (as this would defeat the
    short packing scheme used above), but instead as a sign bit and magnitude. The
    sign bit is transmitted along with the least significant byte in the result,
    which means the first byte contains only the 6 least significant bits of the
    number. The sign bit is the least significant bit of the first byte.

    The decoding algorithm for an unsigned number is as follows:

        * Initialise Y (output) to 0, and S (shift bits) to 0
        * Read byte of input into X
        * Store and strip off continuation bit from X
        * Left shift X by S
        * Bitwise OR X into Y
        * Increase S by 7
        * If continuation bit was set in X, go to step 2

    The decoding algorithm for a signed number is as follows:

        * Initialise Y (output) to 0, and S (shift bits) to 0
        * Read byte of input into X
        * Store and strip off continuation bit from X
        * Store least significant bit of X as sign bit
        * Right shift X by 1
        * Store X into Y
        * Increase S by 6
        * If continuation bit was set, move to unsigned decoder
        * If sign bit was set, multiply result by -1

    A diagram show the structure of the coded numbers is below:

    Unsigned, only 7 bits:

    byte    (MSB) bit meanings (LSB)
     0       [0]: continuation bit clear    [xxx xxxx]: data

    Unsigned, > 7 bits:

    byte    (MSB) bit meanings (LSB)
     0       [1]: continuation bit set      [xxx xxxx]: data least significant 7 bits
    ...      [1]: continuation bit set      [xxx xxxx]: data next most significant 7 bits
     n       [0]: continuation bit clear    [xxx xxxx]: data most significant 7 bits

    Signed, only 6 bits:

    byte    (MSB) bit meanings (LSB)
     0       [0]: continuation bit clear    [xxx xxx]: data     [s]: sign

    Signed, > 6 bits:

    byte    (MSB) bit meanings (LSB)
     0       [1]: continuation bit set      [xxx xxx]: data     [s]: sign
    ...      [1]: continuation bit set      [xxx xxxx]: data next most significant 7 bits
     n       [0]: continuation bit clear    [xxx xxxx]: data most significant 7 bits

    Alternative NEW_DATA
    --------------------

    This is a special form of the NEW_DATA packet where the payload length is fixed
    at 4 bytes, used to transmit a single sample with no SOH.

    Type: 0x1SSSSSSS
    Value:  [uint32] sample
     - or -
    Value:  [float ] sample

    If a NEW_DATA packet is received with payload length 4 bytes, it should be
    interpreted as a single, uncompressed sample (either integer or IEEE floating
    point, depending on the channel's format).

    This type of packet is used to reduce bandwidth usage in the case of low-sample
    rate data, e.g. CMG-DM24 strong motion channels.
 */
#define GDI_CMD_NEW_DATA_ID 0x10000000
#define GDI_CMD_NEW_DATA(segId) (GDI_CMD_NEW_DATA_ID | (segId & 0x0FFFFFFF))

/**
    NEW_TEXT
    ----------------

    Type: 0x2CCCCCCC
    Value:  [uint32] length {n}
            [uint32] start.day
            [uint32] start.sec
            [uint32] start.nsec
            [n*char] text

    Sent by a tx client whenever text data is available for a channel. The message
    type contains the chan_id in the bottom 28 bits.
 */
#define GDI_CMD_NEW_TEXT(chanId) (0x20000000 | (chanId & 0x0FFFFFFF))

/**
    BACKFILL_REQUEST
    ----------------

    Type: 0x00000007
    Value:  [uint32] chan_id
            [uint32] start.day
            [uint32] start.sec
            [uint32] start.nsec
            [uint32] end.day
            [uint32] end.sec
            [uint32] end.nsec

    Set by an rx client whenever it wants to receive data for a time period that it
    missed.
 */
#define GDI_CMD_BACKFILL_REQUEST 0x00000007

/**
    BACKFILL_UNAVAILABLE
    --------------------

    Type: 0x00000008
    Value:  [uint32] chan_id
            [uint32] start.day
            [uint32] start.sec
            [uint32] start.nsec
            [uint32] end.day
            [uint32] end.sec
            [uint32] end.nsec

    Sent by a tx client if it receives a BACKFILL_REQUEST from its peer which
    includes time ranges that no data is available for. The rx client should mark
    these time ranges as having been received so that it no longer requests those
    ranges in future.
 */
#define GDI_CMD_BACKFILL_UNAVAILABLE 0x00000008

/**
    BACKFILL_FINISHED
    -----------------

    Type: 0x0000000F
    Value:  [uint32] chan_id
            [uint32] start.day
            [uint32] start.sec
            [uint32] start.nsec
            [uint32] end.day
            [uint32] end.sec
            [uint32] end.nsec

    Sent by a tx client whenever a backfill request has been completed (i.e. the
    entire requested time range has been covered with either data or unavailable
    responses).
 */
#define GDI_CMD_BACKFILL_FINISHED 0x0000000F

/**
    TERMINAL_NEW
    ------------

    Type: 0x00000011
    Value:  [string] terminal_name
            [string] type

    Sent by a tx module to notify its peers of a terminal. The type may be
    "DIRECT", "FORTH", etc.
 */
#define GDI_CMD_TERMINAL_NEW 0x00000011

/**
    TERMINAL_NEW2
    ------------

    Requires config: TERM_PORTNAME
    Type: 0x00000013
    Value:  [string] terminal_name
            [string] type
            [string] port_name

    Sent by a tx module to notify its peers of a terminal. The type may be
    "DIRECT", "FORTH", etc. Similar to TERMINAL_NEW, but with an additional field
    for the port name, which should contain something like "Port B". This can only
    be sent if GDI_LINKCFG_TERM_PORTNAME was negotiated.
 */
#define GDI_CMD_TERMINAL_NEW2 0x00000013

/**
    TERMINAL_DESTROYED
    ------------------

    Type: 0x00000012
    Value:  [string] terminal_name

    Sent by a tx module to notify its peers that a previously-sent terminal is no
    longer available on the system.
 */
#define GDI_CMD_TERMINAL_DESTROYED 0x00000012

/**
    TERMINAL_REQUEST
    ----------------

    Type: 0x00000009
    Value:  [uint32] term_id
            [string] terminal_name

    Sent by an rx client to request a terminal link. The term_id field is arbitrary
    but must be unique (i.e. must not correspond to any other open terminal on this
    link).
 */
#define GDI_CMD_TERMINAL_REQUEST 0x00000009

/**
    TERMINAL_DATA
    -------------

    Type: 0x0000000A
    Value:  [uint32] term_id
            [string] data

    Data to/from instrument terminal. Can be sent by either rx or tx client.
 */
#define GDI_CMD_TERMINAL_DATA 0x0000000A

/**
    TERMINAL_CLOSED
    ---------------

    Type: 0x0000000B
    Value:  [uint32] term_id

    Specific terminal closed. Can be sent by either rx or tx client. The peer
    receiving this first should send a corresponding TERMINAL_CLOSED packet.
 */
#define GDI_CMD_TERMINAL_CLOSED 0x0000000B

/**
    ERROR_MESSAGE
    -------------

    Type: 0x0000000C
    Value:  [string] message

    Sends a human-readable error message to a peer. This could be logged or
    displayed to the user in some manner. Can be used to notify the client of
    fatal or non-fatal problems.
 */
#define GDI_CMD_ERROR_MESSAGE 0x0000000C

/**
    SEGMENT_INFO_REQUEST
    -------------

    Type: 0x000000FF
    Value:  [uint32] seg_id

    Send by client when segment information has been lost.

    Server sends SEGMENT_INFO command in response if information found,
    otherwise SEGMENT_INFO_UNAVAILABLE.
 */
#define GDI_CMD_SEGMENT_INFO_REQUEST 0x000000FF

/**
    SEGMENT_INFO
    -------------

    Type: 0x000000FE
    Value:  [uint32] chan_id
            [uint32] seg_id
            [uint32] day
            [uint32] sec
            [uint32] nsec

    Send by server on client request.
 */
#define GDI_CMD_SEGMENT_INFO 0x000000FE

/**
    SEGMENT_INFO_UNAVAILABLE
    -------------

    Type: 0x000000FD
    Value:  [uint32] seg_id

    Send by server when requested segment info couldn't be found.
 */
#define GDI_CMD_SEGMENT_INFO_UNAVAILABLE 0x000000FD

/**
    KEEPALIVE
    ---------

    Type: 0x00000010
    Value:  (none)

    Sent by a peer to signal that it is alive to a remote host.
 */
#define GDI_CMD_KEEPALIVE 0x00000010

/**
    SCTL2_NEW
    ---------

    Type: 0x00000014
    Value:  [string] short_name
            [string] long_name
            [string] capabilities
            [uint32] polarity

    Sent by the transmitter to notify its peer of a sensor that may be controlled.
    The short_name is an ID suitable for display and for further messages about the
    sensor. The long_name is a verbose description of the sensor.

    The capabilities string is a space-separated list of functions supported by the
    sensor. See SCTL2_CMD for a list of known commands. An example of this string
    for a sensor that just supports lock and unlock: "mass_lock mass_unlock".

    The polarity field is one of the following values:
        0 - configurable control line polarity not supported by digitiser
        1 - digitiser will auto-detect polarity
        2 - active low
        3 - active high

    Note that this command is designed to be somewhat extensible; if useful but
    not essential data can be added it will be added to the end of the payload. A
    receiver that does not understand these fields can simply ignore them. If data
    to be added is in fact essential then a new message type (and negotiation
    option) will have to be added.
 */
#define GDI_CMD_SCTL2_NEW 0x00000014

// polarity definitions
#define GDI_SCTL2_NEW_POLARITY_NOT_SUPPORTED 0
#define GDI_SCTL2_NEW_POLARITY_AUTO_DETECT   1
#define GDI_SCTL2_NEW_POLARITY_ACTIVE_LOW    2
#define GDI_SCTL2_NEW_POLARITY_ACTIVE_HIGH   3

/**
    SCTL2_DROP
    ----------

    Type: 0x00000015
    Value:  [string] short_name

    Sent by the transmitter to notify its peer that a sensor can no longer be
    controlled (e.g. was disconnected).

    Note we will also see SCTL2_DROP followed by SCTL2_NEW if some option about the
    sensor is changed (e.g. long_name or polarity).
 */
#define GDI_CMD_SCTL2_DROP 0x00000015

/**
    SCTL2_CMD
    ---------

    Type: 0x00000016
    Value:  [string] short_name
            [string] command
            [string] app_name
            ... various parameters ...

    Sent by the receiver when it wants to issue a command to a remote sensor. The
    app_name field is used in log messages on the remote side and should be less
    than 64 chars with ASCII characters 33–126 only. The supported commands are:

    mass_lock
    mass_unlock
    mass_centre
            These do not take any parameters.

    calib_sine_rel
        Sine wave calibration signal generation with relative amplitude as
        percentage (1–100). Parameters:
                [uint32] component
                [double] frequency_hz
                [uint32] amplitude_pct
                [uint32] duration_s

        Component value is:
                0 = all simultaneously
                1 = Z (up/down)
                2 = N (north/south)
                3 = E (east/west)

    calib_step_rel
    calib_bbnoise_rel
        Step signal or broadband noise calibration signal generation with
        relative amplitude as percentage (1–100). Parameters:
                [uint32] component
                [uint32] amplitude_pct
                [uint32] duration_s

    calib_sine_abs
        Sine wave calibration signal generation with absolute amplitude in
        volts. Parameters:
                [uint32] component
                [double] frequency_hz
                [uint32] amplitude_v
                [uint32] duration_s

    calib_step_abs
    calib_bbnoise_abs
        Step signal or broadband noise calibration signal generation with
        absolute amplitude in volts. Parameters:
                [uint32] component
                [uint32] amplitude_v
                [uint32] duration_s

    config_polarity
        Sets the control line polarity of the remote system. Parameters:
                [uint32] polarity
                        1 - digitiser will auto-detect polarity
                        2 - active low
                        3 - active high
 */
#define GDI_CMD_SCTL2_CMD 0x00000016


// component defs
#define GDI_SCTL2_COMPONENT_ALL_SIM 0 // all simultaneously
#define GDI_SCTL2_COMPONENT_Z       1 // up/down
#define GDI_SCTL2_COMPONENT_N       2 // north/south
#define GDI_SCTL2_COMPONENT_E       3 // east/west
// commands
#define GDI_SCTL2_CMD_MASS_LOCK         "mass_lock"
#define GDI_SCTL2_CMD_MASS_UNLOCK       "mass_unlock"
#define GDI_SCTL2_CMD_MASS_CENTRE       "mass_centre"
#define GDI_SCTL2_CMD_CALIB_SINE_REL    "calib_sine_rel"
#define GDI_SCTL2_CMD_CALIB_STEP_REL    "calib_step_rel"
#define GDI_SCTL2_CMD_CALIB_BBNOISE_REL "calib_bbnoise_rel"
#define GDI_SCTL2_CMD_CALIB_SINE_ABS    "calib_sine_abs"
#define GDI_SCTL2_CMD_CALIB_STEP_ABS    "calib_step_abs"
#define GDI_SCTL2_CMD_CALIB_BBNOISE_ABS "calib_bbnoise_abs"
#define GDI_SCTL2_CMD_CONFIG_POLARITY   "config_polarity"

/**
    SCTL2_RESULT
    ------------

    Type: 0x00000017
    Value:  [string] short_name
            [string] result

    Sent by the transmitter in response to a SCTL2_COMMAND. Asynchronous but for
    a given sensor received in the same order as commands were transmitted. If the
    result string is empty then the message indicates success; otherwise it
    indicates error with an error message.
 */
#define GDI_CMD_SCTL2_RESULT 0x00000017


/**
    MARKER
    ------

    Type: 0x00000018
    Value:  [uint32] day
            [uint32] sec
            [uint32] nsec
            [uint16] event_code
            [uint16] num.channels {n}
            n*[uint32] chan_ids
            [string] comment

    Added 2016/11/15 M.McGowan
    Sent by the transmitter to notify its peer that some system event has occurred.
    The event_code is intended to be a globally published value, which receivers can use to take appropriate action
    if required (e.g. send email/sms notifications, drive relays, display messages, etc).
    The values of "event_code" are split into two; from 0x0000 to 0x7FFF are reserved for standardisation in the
    protocol. values 0x8000 to 0xFFFF can be used in 3rd party implementations for application-specific events, if none
    of the standard ones are appropriate. A value of 0 is for "general" events, where the comment is the only
    significant element.

    A single message can be send for one or more channels, thus cutting redundancy and bandwidth in transmission when
    the same notification applies to multiple channels (e.g. system boot, or instrument function).
 */
#define GDI_CMD_MARKER 0x00000018

#endif /* GDI_PROTOCOL_H_ */
