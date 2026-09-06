#
#   ew2moledb generic configuration file for WAVE_RING
#
#
 MyModuleId     MOD_EW2MOLEDB_WAVERING # module id for this program
 InRing         WAVE_RING           # transport ring to use for input
 HeartBeatInt   30                  # EW internal heartbeat interval (sec)
 LogFile        1                   # If 0, don't write logfile
                                    # if 2, write to module log but not to
                                    # stderr/stdout .

# Unique name that indentifies the EW instance is usually declared as variable
EWInstanceName    ${EWMOLE_INSTANCENAME}
#
# Mole DB connection parameters
DBHostname        ${MOLEDBHOSTNAME}
DBName            mole
DBUsername        moleuser
DBPassword        molepass
# DBPort           0

# Milliseconds to wait after a DB error
WaitMSecAfterDBError 2000

# Flag to debug MySql instructions
DebugMySql 0

# Logos of messages to export to client systems
# Each message of a given logo will be written to a file with the specified
# File Suffix at the end of the file name folloing `.'.
# Do NOT include a `.' in the suffix.
# Use as many GetMsgLogo commands as you want.
#              Installation       Module       Message Type        File Suffix
 GetMsgLogo    INST_INGV        MOD_WILDCARD   TYPE_TRACEBUF2      tracebuf2
 
 MaxMsgSize        8192        # maximum size (bytes) for input msgs
 QueueSize         5000        # number of msgs to buffer

#   Specify the name of a computer to use as a mail server.
#   This system must be alive for mail to be sent out.
#   This parameter is used by Windows NT only.
MailServer  mail.server.domain

# Specify the "From" line for the email messages. (not required)
# If commented out, the email "From" field will be filled out
# with environment variables:  %USERNAME%@%COMPUTERNAME%.
# This parameter is used by Windows NT only.
# mailFrom username@thishost
mailFrom "account@server.domain"

#   Between 0 and 10 email recipients may be specified below.
#   These lines are optional.
#
#   Syntax
#     mail  <emailAddress1>
#     mail  <emailAddress2>
#             ...
#     mail  <emailAddressN>
#

# Mail program to use, e.g /usr/ucb/Mail (not required)
# If given, it must be a full pathname to a mail program
#
MailProgram /usr/ucb/Mail
# MailProgram "/usr/ucb/Mail -r account@server.domain"

