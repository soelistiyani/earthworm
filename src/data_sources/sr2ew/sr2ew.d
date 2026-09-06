# FILE: sr2ew.d                   Copyright (c), Symmetric Research, 2018
#
# This is the sr2ew parameter file for acquiring data with a Symmetric 
# Research USBxCH 24 bit A/D board and putting that data onto an Earthworm
# ring.  See below for more parameter details and a general usage overview.
#

#------------------------------------------------------------------------
#
# PARAMETER VALUES
#
#------------------------------------------------------------------------

#
# Simple parameters:
#

MyModuleId      MOD_SRUSB2EW     # module id for this instance of sr2ew
RingName        WAVE_RING        # send acquired data to this shared EW memory ring
HeartBeatInt    60               # seconds between heartbeats
OutputMsgType   TYPE_TRACEBUF2   # use Station/Channel/Network/Location (SCNL)
LogSwitch       3                # logging ( 0=NONE, 1=EW, 2=SR, 3=BOTH )
Verbosity       0                # amount of debug info sent to log file, 0-4
SummaryInterval 600              # seconds between summary info in log file

#
# Acquired data identification parameters:
#

#   A/D          4     3     2    2
# Channel       Sta   Comp  Net  Loc
# -------       ---   ----  ---  ---
Channel0       "SRHQ  HNZ   SR   C0"
Channel1       "SRHQ  HNZ   SR   C1"
Channel2       "SRHQ  HNZ   SR   C2"
Channel3       "SRHQ  HNZ   SR   C3"
Channel4       "SRHQ  HNZ   SR   C4"
Channel5       "SRHQ  HNZ   SR   C5"
Channel6       "SRHQ  HNZ   SR   C6"
Channel7       "SRHQ  HNZ   SR   C7"
Channel8       "SRHQ  DIG   SR   C8"
Channel9       "SRHQ  DIG   SR   C9"
Channel10      "SRHQ  DIG   SR   CA"
Channel11      "SRHQ  DIG   SR   CB"
Channel12      "SRHQ  GPS   SR   CC"
Channel13      "SRHQ  PWR   SR   CD"
Channel14      "SRHQ  DEG   SR   CE"
Channel15      "SRHQ  NUM   SR   CF"

# NOTE: Special channels are GPS pulse per second, Power good flag, 
#       Temperature in Degrees, Number of satellites



#------------------------------------------------------------------------
#
# PARAMETER DETAILS
#
#------------------------------------------------------------------------
#
# MyModuleId:
# This is the module ID for this instance of sr2ew.  It would be 
# MOD_SR2EW if that were defined in the earthworm.d file.  Instead we
# use MOD_SRUSB2EW. MOD_ADSEND_A, MOD_ADSEND_B, and MOD_ADSEND_C would
# also be reasonable choices.
#
# RingName:
# This is the Earthworm shared memory region where acquired data is
# sent.  Typically this would be WAVE_RING which is used for processing
# waveforms.
#
# OutputMsgType: 
# Describes how waveforms are identified.  Older style waveforms were
# identified only by Station, Channel, and Network (SCN), while the newer style 
# also includes Location.  For Earthworm v7.0 and greater, use TYPE_TRACEBUF2 
# (recommended).  If you need the older style, use TYPE_TRACEBUF.
#
# HeartBeatInt:
# This specifies the interval in seconds between sending heartbeat
# messages to EW so it knows sr2ew is still alive.  A typical value is
# 60.  Just make sure to send more often than the listener checks.  This
# parameter is required in a .d file for statmgr.
#
# LogSwitch:
# Controls where logging messages are sent.  Options are:
#
#       0  no logging
#       1  output to EW log file in log directory
#       2  output to SR report file in data\YMDHMS directory
#       3  output to both files (default)
#
# Most users will be happy with one log or the other, but don't need both.
# There are only three small differences between them.  First the EW log file 
# is started slightly sooner than the SR report file since the latter can't be
# opened until the YMDHMS directory is started.  Second the SR report file can
# contain messages from the custom pipe library.  And finally, the timestamp
# format varies slightly between the two.
#
# Verbosity: 
# This controls the amount of debugging info sent to the log file.  Warnings and
# errors always given.  Allowed values range from 0 to 8 with levels 5 and above
# being mostly for code debugging.
#
#        0 adds startup info (default)
#        1 adds more configuration details
#        2 adds trace data message info every second
#        3 adds function calls
#        4 adds heartbeat calls and every data point
#
# SummaryInterval:
# Interval in seconds between summary information being output to the
# log file.  Summary info includes minimum and maximum values for each
# input over the summary interval.  Typical values are 600 which means
# every 10 minutes and 3600 which means every hour.
#
# Channel#:
# These refer to the USBxCH A/D board channels or inputs and identify
# the acquired data waveforms.  Each input is described by its SCNL
# information where SCNL stands for Station, Channel (or Component),
# Network, and Location.  Reasonable values might be N=SR for the SR
# network, S=SRHQ for the USBxCH stationed at SR Headquarters, L=C# 
# with # being a hex digit from 0 to F corresponding to each of the 16
# available USBxCH inputs, and C=HNZ indicating the data is the vertical
# component (Z) of an 80-250 Hz high broadband station (H) recording
# from a geophone or accelerometer (N).  The EW pin number is hardwired
# to the USBxCH input channel number.
# 
# The SCNL location seems to refer to the location of the individual
# sensors and not the location of the station itself.  The other point
# of terminology confusion surrounds the word channel.  Most SR
# documentation refers to the USBxCH A/D board as having many channels
# meaning many separate inputs which is similar to the EW Location
# value.  But the EW Channel or Component value seems to provide more
# sensor type information.




#------------------------------------------------------------------------
#
# USAGE OVERVIEW
#
#------------------------------------------------------------------------
#
# To acquire and work with USBxCH data you will also need to edit other EW .d
# files.  A simple configuration where the USBxCH files are acquired, saved, and
# viewed on one computer would require making edits similar to those shown below
# for the startstop.d, wave_server.d, and wave_viewer.d files.
# 
# startstop.d:
# 
# Use this file to tell earthworm to run sr2ew to acquire data by adding lines
# like these for Windows:
#
#   Process          "sr2ew"
#   PriorityClass     Normal
#   ThreadPriority    Normal
#   Display           NewConsole
#
# Or like these for Linux:
#
#   Process          "sr2ew"
#   Class/Priority   RR 5
#
# Note: The configuration file is hardwired to come from "%EW_PARAMS%/sr2ew.d".
#   
#
# wave_serverV.d:
# 
# Use this file to save the acquired USBxCH data into tanks.  You will need to
# add lines giving the channel SCNL information, size, and tank location.  The
# proper size is determined by the sampling rate.  Each USBxCH record will be
# about 1 second long so the size needed is:
# 
#     (SamplingRate+2)*sizeof(long)+sizeof(old tracebuf header).
#
#        For example, at   78.1 sps ->   80*4+67 =  387
#        and          at  130.2 sps ->  132*4+67 =  595
#        and          at  651.0 sps ->  653*4+67 = 2679
#        and          at 1302.1 sps -> 1304*4+67 = 5283 -> 2675
#
# This last value is greater than the max tracebuf size of 4096, so the data is
# split into two half second buffers of 1304/2*4+67 = 2675 each.  You may need to
# increase MaxMsgSize to accomodate the larger values.  For the USB4CH at 78 sps
# one might add lines like these.
#
#   Tank    SRHQ HNZ SR C0 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\CH00.tnk
#   Tank    SRHQ HNZ SR C1 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\CH01.tnk
#   Tank    SRHQ HNZ SR C2 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\CH02.tnk
#   Tank    SRHQ HNZ SR C3 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\CH03.tnk
#   Tank    SRHQ DIG SR C8 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\DIG.tnk
#   Tank    SRHQ GPS SR CC 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\PPS.tnk
#   Tank    SRHQ PWR SR CD 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\PWR.tnk
#   Tank    SRHQ DEG SR CE 400   INST_UNKNOWN  MOD_WILDCARD       1         10000         c:\earthworm\run\data\TEMP.tnk
# 
# wave_viewer.d:
#
# Use this file to view the acquired USBxCH data.  You will need to give the IP
# address and port for the wave_server storing the data.  You may also want to
# add group lines identifying the channels to display.
#
#   group USB4CH_Analog
#          SRHQ HNZ SR C0
#          SRHQ HNZ SR C1
#          SRHQ HNZ SR C2
#          SRHQ HNZ SR C3
#
#   group USB4CH_Other
#          SRHQ DIG SR C8
#          SRHQ GPS SR CC
#          SRHQ PWR SR CD
#          SRHQ DEG SR CE
#
# A more complicated configuration would involve acquiring the USBxCH data on
# one computer and sending it to another for saving, processing, and viewing.
# This can be done using the standard earthworm import_generic/export (or
# export_scnl) functions to transfer USBxCH tracebuf data from a local WAVE_RING
# to the WAVE_RING on another machine.
#
# For this case, you will need to edit startstop.d, sr2ew.d and export_scnl.d, on
# the acquiring computer and startstop.d, import_generic.d, wave_serverV.d and
# wave_viewer.d on the processing/viewing computer.
#
# For the export/import pair, you will need to set their IP addresses to
# point to each other, set up the heartbeat strings so that each looks for 
# the string the other is sending, and set the timeouts so that each sender
# sends a heartbeat more often than the listener requires.  You will also 
# want to include lines identifying the channels to send.  For example
#
# Send_scnl       SRHQ HNZ SR C0                # send this specific channel
# Send_scnl       SRHQ HNZ SR C1                # send this specific channel
# Send_scnl       SRHQ DIG SR C8                # send this specific channel
#
# If you have data from two different USBxCH boards arriving at the same ring,
# you must ensure that each is using a different ModuleId.  Otherwise, the two
# sets of data will appear to be scrambled together which leads to lots of
# message sequence number errors.  To use MOD_SR2EW for a ModuleId instead of
# MOD_SRUSB2EW, you must first define it in your earthworm.d file.  Other
# reasonable already defined choices would be MOD_ADSEND, MOD_ADSEND_A,
# MOD_ADSEND_B, or MOD_ADSEND_C.
#
# Thank you for using the Symmetric Research USBxCH data acquisition boards. 
# We hope they work well for your acquisition projects.
#
