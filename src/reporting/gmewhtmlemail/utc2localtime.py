from timezonefinder import TimezoneFinder
import datetime
import pytz

tf = TimezoneFinder()

import sys

try:
    date_arg = "00:00:00" if len(sys.argv)==4 else sys.argv[4]    
    event_time_utc = datetime.datetime.strptime( sys.argv[3]+" "+date_arg, "%Y.%m.%d %H:%M:%S" )
    event_lon = float(sys.argv[1] ) # -119.6542
    event_lat = float(sys.argv[2] ) # 39.9683
    tz = pytz.timezone( tf.timezone_at( lng=event_lon, lat=event_lat ) )
    event_time_local = pytz.utc.localize( event_time_utc, is_dst=None).astimezone( tz ) 

    print(event_time_local.strftime("%Y.%m.%d %H:%M:%S %z %Z"))
except:
    print("Usage: %s <longitude> <latitude> [<date> [<time>]]" % argv[0])
