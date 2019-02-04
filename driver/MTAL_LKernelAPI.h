/****************************************************************************
*
*  Module Name    : MTAL_LKernelAPI.h
*  Version        :
*
*  Abstract       : MT Abstraction Layer for Linux Kernel
*
*  Written by     : Beguec Frederic, Baume Florian
*  Date           : 16/03/2016
*  Modified by    : 
*  Date           :
*  Modification   :
*  Known problems : None
*
* Copyright(C) 2017 Merging Technologies
*
* RAVENNA/AES67 ALSA LKM is free software; you can redistribute it and / or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* RAVENNA/AES67 ALSA LKM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with RAVAENNA ALSA LKM ; if not, see <http://www.gnu.org/licenses/>.
*
****************************************************************************/

#ifndef __MTAL_LKERNELAPI_H__
#define __MTAL_LKERNELAPI_H__

#include "MTAL_TargetPlatform.h"

#if defined(MTAL_LINUX) && defined(MTAL_KERNEL)

#if	defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

//typedef unsigned long long uint64_t;

/*#ifndef KERN_INFO
    #define KERN_INFO "\001" "6"
#endif // KERN_INFO*/


#include "MTAL_stdint.h"

#ifndef NULL
    #define NULL 0UL
#endif // NULL

extern void *my_alloc(size_t size);
extern void my_free(void *p);
extern void *malloc (size_t __size);
extern void *calloc (size_t __nmemb, size_t __size);
//extern void *realloc (void *__ptr, size_t __size);
extern void free (void *__ptr);


extern int snprintf(char *buf, size_t size, const char *fmt, ...);
extern int MTAL_LK_print(const char *fmt, ...);

typedef void* MTAL_LK_spinlock_ptr;

extern MTAL_LK_spinlock_ptr MTAL_LK_spinlock_alloc(void);
extern void MTAL_LK_spinlock_free(MTAL_LK_spinlock_ptr lock);
extern void MTAL_LK_spin_lock_init(MTAL_LK_spinlock_ptr lock);
extern void MTAL_LK_spin_lock_irqsave(MTAL_LK_spinlock_ptr lock, unsigned long* flags);
extern void MTAL_LK_spin_unlock_irqrestore(MTAL_LK_spinlock_ptr lock, unsigned long flags);
extern void MTAL_LK_spin_lock(MTAL_LK_spinlock_ptr lock);
extern void MTAL_LK_spin_unlock(MTAL_LK_spinlock_ptr lock);


typedef void* MTAL_LK_rwlock_ptr;

extern MTAL_LK_rwlock_ptr MTAL_LK_rwlock_alloc(void);
extern void MTAL_LK_rwlock_free(MTAL_LK_rwlock_ptr rwlock);
extern void MTAL_LK_rwlock_init(MTAL_LK_rwlock_ptr rwlock);
extern void MTAL_LK_read_lock_irqsave(MTAL_LK_rwlock_ptr rwlock, unsigned long* flags);
extern void MTAL_LK_read_unlock_irqrestore(MTAL_LK_rwlock_ptr rwlock, unsigned long flags);
extern void MTAL_LK_write_lock_irqsave(MTAL_LK_rwlock_ptr rwlock, unsigned long* flags);
extern void MTAL_LK_write_unlock_irqrestore(MTAL_LK_rwlock_ptr rwlock, unsigned long flags);

extern uint64_t MTAL_LK_GetCounterTime(void); // 100 ns precision
extern uint64_t MTAL_LK_GetCounterFreq(void); // not sure what's the point of that one
extern uint64_t MTAL_LK_GetSystemTime(void); // 100 ns precision

#if	defined(__cplusplus)
}
#endif // defined(__cplusplus)

#endif // defined(MTAL_LINUX) && defined(MTAL_KERNEL)
#endif //__MTAL_LKERNELAPI_H__
