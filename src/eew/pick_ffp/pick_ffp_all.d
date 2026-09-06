#
#                     Pick_FP's Configuration File
#
MyModId       MOD_PICK_FFP      # This instance of pick_FP
StaFile       "D:\Dropbox\project\eqtoad\bin\params\pick_ffp.sta"    # Cofigured file for auto-picking
InRing           WAVE_RING     # Transport ring to find waveform data on,
OutRing          PICK_RING     # Transport ring to write output to,
HeartbeatInt            30     # Heartbeat interval, in seconds,
RestartLength          100     # Number of samples to process for restart
MaxGap                  15     # Maximum gap to interpolate
Debug                    0     # If 1, print debugging message

EEWFile		D:\Dropbox\project\eqtoad\bin\params\sta_kma24.last		   # Cofigured file of station location and gain factor
ProduceOldStylePick 0          # If 1, produce original pick message(TYPE_EEW)

WeightTable 0.02 0.05 0.50 1.00 #Pick weight table (maximum error in seconds for weight 0 up to weight 3)

# Specify which messages to look at with Getlogo commands.
#   GetLogo <installation_id> <module_id> <message_type>
# The message_type must be either TYPE_TRACEBUF or TYPE_TRACEBUF2.
# Use as many GetLogo commands as you need.
# If no GetLogo commands are given, pick_FP will look at all
# TYPE_TRACEBUF and TYPE_TRACEBUF2 messages in InRing.
#-----------------------------------------------------------------
GetLogo  INST_WILDCARD  MOD_WILDCARD  TYPE_TRACEBUF2
