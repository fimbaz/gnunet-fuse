/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2012 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file src/fuse/mutex.h
 * @brief pthreads wapper and thread related services
 *
 * @author Christian Grothoff
 * @author Krista Bennett
 * @author Gerd Knorr <kraxel@bytesex.org>
 * @author Ioana Patrascu
 * @author Tzvetan Horozov
 */

#ifndef MUTEX_H
#define MUTEX_H

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif


/**
 * @brief Structure for MUTual EXclusion (Mutex).
 */
struct GNUNET_Mutex;


struct GNUNET_Mutex *
GNUNET_mutex_create (int isRecursive);


void 
GNUNET_mutex_destroy (struct GNUNET_Mutex *mutex);


void 
GNUNET_mutex_lock (struct GNUNET_Mutex *mutex);


void 
GNUNET_mutex_unlock (struct GNUNET_Mutex *mutex);


#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

/* ifndef MUTEX_H */
#endif
/* end of mutex.h */
