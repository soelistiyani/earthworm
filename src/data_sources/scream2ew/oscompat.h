/*
 * oscompat.h:
 *
 * Copyright (c) 2003 Guralp Systems Limited
 * Author James McKenzie, contact <software@guralp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * $Id: oscompat.h 7425 2018-05-22 21:39:26Z baker $
 */

/*
 * $Log$
 * Revision 1.5  2007/07/20 14:15:21  paulf
 * more fixes related to linux
 *
 * Revision 1.4  2007/07/20 13:45:14  paulf
 * fixed a byte order issue for linux
 *
 * Revision 1.3  2007/01/24 03:17:06  stefan
 * fix for solaris intel defined not define
 *
 * Revision 1.2  2006/05/16 17:14:35  paulf
 * removed all Windows issues from the .c and .h and updated the makefile.sol
 *
 * Revision 1.1  2003/03/27 18:07:18  alex
 * Initial revision
 *
 * Revision 1.6  2003/02/19 16:00:18  root
 * #
 *
 */

#ifndef __OSCOMPAT_H__
#define __OSCOMPAT_H__

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#define snprintf	_snprintf
#define strcasecmp	_strcmpi
#endif

#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif

#endif /* __OSCOMPAT_H__ */
