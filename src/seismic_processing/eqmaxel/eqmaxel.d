MyModuleId  MOD_EQMAXEL
RingName    HYPO_RING   # ignored if in run once test mode

LogFile       1         # 0=log to stderr/stdout only
                         # 1=log to disk and stderr/stdout
                         # 2=log to disk only
 
LogArcMsg     1         # Optional command; default LogArcMsg=0
                         # 0=do not log output TYPE_HYP2000ARC msg
                         # non-zero=write ARC msg to log file

LabelAsBinder 0         # 0=label phases as generic P and S;
                         # non-zero = label phases as binder did (Pg, Pn etc)

LabelVersion  0         # Optional command; default LabelVersion=1
                         # 0 = write a blank in the version field of the 
                         #   summary line of the TYPE_HYP2000ARC msg 
                         # non-zero = use the version number passed from
                         #   eqproc,eqprelim on the summary line.

#UsePhaseWeight           # Optional command to create weight based on pick quality, otherwise all phases
			 # are given a weight of 1.0

SourceCode     M         # a char to put in the summary card position 81 to say what source of solution is

Verbose 2 	# passes through to MAXEL code verbosity, 
                # setting of 1 provides useful output about MLE and eqmaxel execution

# MAXEL algorithm required inputs
#
site_file		korea.hinv		# hypoinverse station file
CenterLongitude        127.5  			# Xc in MAXEL
CenterLatitude         35.80  			# Yc in MAXEL
#TravelTimeTable        Ktrtime.tbl  		# trtable in MAXEL  (use bldtrtable helper program 
					        # with binder_ew velocity model file)
# or use the velocity model commands
@korea_vmodel.d
