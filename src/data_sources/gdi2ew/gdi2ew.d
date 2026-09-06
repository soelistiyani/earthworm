################################################################################
# Copyright (c) 2017, Guralp Systems Limited. All rights reserved.
# 
# The following are the licensing terms and conditions (the “License Agreement”)
# under which Guralp Systems Limited (“GSL”) grants access to, use of and
# redistribution of the “Code” (as defined below) to a recipient (the “Licensee”).
# Any use or redistribution of the Code by the Licensee shall be deemed to be
# acceptance of the terms and conditions of this License Agreement. In the event
# of inconsistency or conflict between this License Agreement and any other
# license for the Code then the terms of this License Agreement shall prevail.
# 
# The Code is defined as each and every file in any previous or current
# distribution of the source-code and compiled executables comprising the gdi2ew
# distributable (inclusive of all supporting and embedded documentation) and
# subsequent releases thereof as may be made available by GSL from time to time.
# 
# 1. The License. GSL grants to the Licensee (and Sub-Licensee if applicable)
# a non-exclusive perpetual (subject to termination by GSL in accordance with
# paragraph 4) license (the “License”) to use (“Use”) the Code either alone or
# in conjunction with other code to produce one or more applications (each a
# Derived Product) and/or redistribute the Code or Derived Product
# (Redistribution”) to a third party (each being a “Sub-Licensee”), in each case
# strictly in accordance with the terms and conditions of this License Agreement.  
# 
# 2. Redistribution Conditions. Redistribution and Use of the Code, with or
# without modification, is permitted under the terms of this License Agreement
# provided that the following conditions are met by the Licensee and any Sub
# Licensee: 
# 
# a) Redistribution of the Code must include within the documentation and/or
# other materials provided with the Redistribution the copyright notice
# “Copyright ©2017, Guralp Systems Limited. All rights reserved”.
# 
# b) The Licensee and any Sub-Licensee is responsible for ensuring that any
# party to whom the Code is redistributed is bound by the terms of this License
# as a “Sub-Licensee” and will therefore make Use of the Code on the basis of
# understanding and accepting this Licence Agreement.
# 
# c) Neither the name of Guralp Systems, nor the Guralp logo, nor the names of
# GSL’s contributors may be used to endorse or promote products derived from the
# Code without specific prior written permission from GSL.
# 
# d) Neither the Licensee nor any Sub-Licensee may charge any form of fee or
# royalty for providing the Code to a third party other than as embedded as a
# proportionate element of the fee or royalty charged for a Derived Product.
# 
# e) A Licensee or Sub-licensee may charge a fee or royalty for a Derived
# Product.  
# 
# 3. DISCLAIMER. EXCEPT AS EXPRESSLY PROVIDED IN THIS LICENSE, GSL HEREBY
# EXCLUDES ANY IMPLIED CONDITION OR WARRANTY CONCERNING THE MERCHANTABILITY OR
# QUALITY OR FITNESS FOR PURPOSE OF THE CODE, WHETHER SUCH CONDITION OR WARRANTY
# IS IMPLIED BY STATUTE OR COMMON LAW. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLAR
# , OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS CODE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 
# 4. Term and Termination. This License Agreement shall commence on acceptance
# of these terms by the Licensee (or Sub-Licensee as applicable) and shall
# continue unless terminated by GSL for cause in the event that the Licensee (or
# Sub-Licensee as applicable) commits any material breach of this License
# Agreement and fails to remedy that breach within 30 days of being given
# written notice of that breach by GSL.  
# 
# 5. Law and Jurisdiction. This License Agreement is governed by the laws of
# England and Wales, and is subject to the exclusive jurisdictions of the
# English courts.
################################################################################
#
# This is gdi2ew's parameter file
#
# Please check that line endings are set appropriately for your platform.
#

#  Basic Earthworm setup:
#
MyModuleId         MOD_GDI2EW     # module id for this instance of gdi2ew 
RingName           WAVE_RING      # shared memory ring for input/output
LogFile            1              # 0 to turn off disk log file; if 1, do log.
                                  # if 2, log to module log but not stderr/stdout
Verbose            1              # 0->4 from no debug to VERY noisy
HeartBeatInterval  30             # seconds between heartbeats
#
#
Server             10.30.0.39     # IP address of the GDI server
#Server				10.30.0.56

PortNumber         1565			  # port number of the GDI server (default is 1565)

# To get a list of channels, their SEED names, and group memberships, run
# gdi2ew without any Subscribe or SubscribeGroup settings.

# The SubscribeGroup sets the group level of channels to subscribe to. Any
# channels with this or lower group number will be subscribed.
# e.g.
# "SubscribeGroup 1" will collect only group 1 channels (main ones), whereas
# "SubscribeGroup 4" will collect group 1,2,3 and 4 channels.
# Using a large number will automatically subscribe to everything.
#
#SubscribeGroup	1

#
# The Subscribe lines list which individual channels to subscribe to (receive).
# The first token should be the GDI channel name (as listed when there are no
# subscriptions configured).
# The SCNL will be automatically extracted from the GDI metadata, if available.
# Alternatively, you can specify the SCNL fields on the Subscribe line after the
# GDI channel name. All four must be specified, or they will all be ignored.
# examples:
#Subscribe 6F55.0VELZC MUR HHZ GS --
#Subscribe 6F55.0VELNC
#Subscribe 6F55.0VELEC


# GDI data can arrive in variable size packets, possibly even a stream of single
# samples. Therefore, to pack into a fixed length TRACEBUF, some sort of size
# needs to be specified. The default is 1000 if none is configured, but use the
# "TraceBufSize" option to control this. Smaller values make for lower latency,
# at the cost of inefficiency (the header/data ratio drops). Ultimately, it's up
# to the EW implementor to decide the optimum tradeoff.
TraceBufSize	100

# In addition to "TraceBufSize", the "LowLatency" flag can force an output of a 
# TRACEBUF packet even if it is not full after we have processed the GDI data
# packets. In effect, with this mode the "TraceBufSize" sets the maximum packet
# size, but it can be smaller if GDI sends smaller amounts - i.e. the data is
# presented to EW without delay, at the cost of possible variable length packets.
# For example, if "TraceBufSize" is set to 50, and a GDI packet arrives with 123
# samples, then three TRACEBUF packets are generated, size 50,50,23
LowLatency	1

# The receive timeout for the TCP connection to the server. If no data is
# received in this time, the connection is assumed to have died, and is dropped,
# then a new connection is initiated. Value is in milliseconds, default=60000.
#Timeout	60000
