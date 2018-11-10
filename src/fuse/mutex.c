/*
     This file is part of GNUnet.
     Copyright (C) 2001, 2002, 2003, 2004, 2012 GNUnet e.V.

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/

/**
 * @file src/fuse/mutex.c
 * @brief implementation of mutual exclusion
 */
#include "gnunet-fuse.h"
#include "mutex.h"

#include <pthread.h>
#if SOMEBSD
# include <pthread_np.h>
#endif

#ifndef PTHREAD_MUTEX_NORMAL
#ifdef PTHREAD_MUTEX_TIMED_NP
#define PTHREAD_MUTEX_NORMAL PTHREAD_MUTEX_TIMED_NP
#else
#define PTHREAD_MUTEX_NORMAL NULL
#endif
#endif

/**
 * This prototype is somehow missing in various Linux pthread
 * include files. But we need it and it seems to be available
 * on all pthread-systems so far. Odd.
 */
#ifndef _MSC_VER
extern int pthread_mutexattr_setkind_np (pthread_mutexattr_t * attr,
                                         int kind);
#endif


/**
 * @brief Structure for MUTual EXclusion (Mutex).
 */
struct GNUNET_Mutex
{
  pthread_mutex_t pt;
};


struct GNUNET_Mutex *
GNUNET_mutex_create (int isRecursive)
{
  pthread_mutexattr_t attr;
  struct GNUNET_Mutex *mut;
#if WINDOWS
  attr = NULL;
#endif

  pthread_mutexattr_init (&attr);
  if (isRecursive)
    {
#if LINUX
      GNUNET_assert (0 == pthread_mutexattr_setkind_np
		     (&attr, PTHREAD_MUTEX_RECURSIVE_NP));
#elif SOMEBSD || GNUNET_freeBSD || GNUNET_freeBSD5
      GNUNET_assert (0 == pthread_mutexattr_setkind_np
		     (&attr, PTHREAD_MUTEX_RECURSIVE));
#elif SOLARIS || OSX || WINDOWS
      GNUNET_assert (0 == pthread_mutexattr_settype
		     (&attr, PTHREAD_MUTEX_RECURSIVE));
#endif
    }
  else
    {
#if LINUX
      GNUNET_assert (0 == pthread_mutexattr_setkind_np
		     (&attr, PTHREAD_MUTEX_ERRORCHECK_NP));
#else
      GNUNET_assert (0 == pthread_mutexattr_settype
		     (&attr, PTHREAD_MUTEX_ERRORCHECK));
#endif
    }
  mut = GNUNET_new (struct GNUNET_Mutex);
  GNUNET_assert (0 == pthread_mutex_init (&mut->pt, &attr));
  return mut;
}


void
GNUNET_mutex_destroy (struct GNUNET_Mutex * mutex)
{
  GNUNET_assert (0 == pthread_mutex_destroy (&mutex->pt));
  GNUNET_free (mutex);
}


void
GNUNET_mutex_lock (struct GNUNET_Mutex * mutex)
{
  if (0 != (errno = pthread_mutex_lock (&mutex->pt)))
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "pthread_mutex_unlock");  
    GNUNET_assert (0);
  }
}


void
GNUNET_mutex_unlock (struct GNUNET_Mutex * mutex)
{
  if (0 != (errno = pthread_mutex_unlock (&mutex->pt)))
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "pthread_mutex_unlock");  
    GNUNET_assert (0);
  }
}


/* end of mutex.c */
