#
#                     Pick_gm's Configuration File
#
MyModId       MOD_PICK_GM      # This instance of pick_FP
StaFile       "sta_kma24.last"    # File containing station name/pin# info
InRing           WAVE_RING     # Transport ring to find waveform data on,
OutRing          HYPO_RING     # Write!!! Same ring to InRing,
HeartbeatInt            30     # Heartbeat interval, in seconds,
RestartLength         2000     # Number of samples to process for restart
MaxGap                  15     # Maximum gap to interpolate
Debug                    0     # If 1, print debugging message
OutputByFile             0     # If 1, write to file
OutputByConsole          0     # If 1, write to screen
OutputFolder             "D:\earthworm\earthworm_7.9\src_k\pick_gm\t" # Don''t add slash at the end. Write to file per minute, Applies if not offline_mode or Set ForceToOutputFolder to 1

# Specify which messages to look at with Getlogo commands.
#   GetLogo <installation_id> <module_id> <message_type>
# The message_type must be either TYPE_TRACEBUF or TYPE_TRACEBUF2.
# Use as many GetLogo commands as you need.
# If no GetLogo commands are given, pick_gm will look at all
# TYPE_TRACEBUF and TYPE_TRACEBUF2 messages in InRing.
#-----------------------------------------------------------------
GetLogo  INST_WILDCARD  MOD_WILDCARD  TYPE_TRACEBUF2
