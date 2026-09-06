#ifndef _MISC_H
#define _MISC_H

#define VER_NO "1.0.7 -  2019.130"
/* keep track of version notes here */
/*
Mario Aranha, Berkley Seismological Laboratory, University of California, Berkeley.
For support: mario@seismo.berkeley.edu

2019/05/10   1.0.7 -  2019.130
- Fixed station name bug in output tracebuf2 header: when a DIF (5-char station name) is
  followed by a PPP (4-char station names) message in the input geoJSON stream, there's a
  left over character due to message buffers not being completely zero'd out between messages.

2016/05/13   1.0.3 - 2016.133
- Added support for ALL RabbitMQ exchange type connections: fanout, direct, topic, headers.
- Upward compatible with previous version that supported named queues only.
- Updated sample 'geojson2ew.d' file.  Existing *.d file(s) should work.
- Changed File(s): misc.h json_conn.h json_conn.c getconfig.c geojson2ew.d

2016/05/24   1.0.4 - 2016.145
- Merged in 'geojson_socket'. Now also extensible to other message server types, e.g., ActiveMQ.
- Added optional SERVERTYPE keyword to config file; possible values are: socket, rabbitMQ.
- Code is now extensible to other message servers, e.g., activeMQ
- Existing *.d file(s) for 'geojson_socket' should work.
- Fixed bugs in message framing for socket server mode.
- Fixed bug in options parsing when -v or -h is used without a config file name.
- Added conditional compilation flag SOCKET_ONLY.
Changed File(s): README misc.h json_conn.h json_conn.c getconfig.c geojson2ew.d options.c makefile.unix

2017/03/28  1.0.5 - 2017.087
- Fixed problems causing crashes and core dumps.
- Added timestamp flag "et" to most logit messages; added/improved messages.
- Improvements multi-threading.
- Fixed geojson2ew_die(), message_send() for possible race conditions and graceful exit.
- Moved EW ring detachment from geojson2ew_die() to end of main program.
- Minimized and mutex'd any changes to 'ShutMeDown' global var.
- Check if attached to EW ring before sending HB and Error messages.
- Re-connect with JSON server (socket, or RabbitMQ) now in a loop; EW 'restartMe' only for crashes.
- Fixed bug(s) in json_read_message().
- Added DATA_DUMPFILE config parameter; useful for debugging and data noise analysis.
- Added full 'blocking' (default) for rabbitMQ reads so 'heartbeats' can be properly utilized.
- Added sanity checks for timeout config parameters.
- Fine tuned various default timeout parameter values.
- Ran static analysis with 'cppcheck' and fixed mem allocation calls.
- Extensive real-time testing over long-haul network between CWU and UCB.
- Update sample "geojson2ew.d" with comments on all possible parameters.
- Updated Copyright notice.
- Misc comments, fixes and cleanups.

2017/08/31  1.0.6 - 2017.243
- Fixed bug that caused crashes with inline comments on MAP_CHAN lines in *.d file.
  Reported and tested by walter@Geology.cwu.EDU.
  Changed File(s): README geojson2ew.d geojson_process.c misc.h
*/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) x
#endif

#endif 
