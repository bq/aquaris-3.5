/*
 * Copyright (C) 2009 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Contains definition of structures, global variables, and implementation of
 * routines that are used by malloc leak detection code and other components in
 * the system. The trick is that some components expect these data and
 * routines to be defined / implemented in libc.so library, regardless
 * whether or not MALLOC_LEAK_CHECK macro is defined. To make things even
 * more tricky, malloc leak detection code, implemented in
 * libc_malloc_debug.so also requires access to these variables and routines
 * (to fill allocation entry hash table, for example). So, all relevant
 * variables and routines are defined / implemented here and exported
 * to all, leak detection code and other components via dynamic (libc.so),
 * or static (libc.a) linking.
 */

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "dlmalloc.h"
#include "malloc_debug_common.h"
#include "mmap_debug_common.h"

/*
  BEGIN mtk-added: back trace recording function for debug 16
*/
#if defined(HAVE_MALLOC_DEBUG_FEATURE) && defined(HAVE_MSPACE_DEBUG_FEATURE)
typedef void (*MspaceMallocStat)(void*, size_t);
typedef void (*MspaceFreeStat)(void*);
typedef int (*Debug15ExtraInitialize)(int, int, int);
MspaceMallocStat mspace_malloc_stat = NULL;
MspaceFreeStat mspace_free_stat = NULL;
#endif
/*
  END mtk-added
*/

/*
 * In a VM process, this is set to 1 after fork()ing out of zygote.
 */
int gMallocLeakZygoteChild = 0;

pthread_mutex_t gAllocationsMutex = PTHREAD_MUTEX_INITIALIZER;
HashTable gHashTable;
unsigned stack_start; // high
unsigned stack_end; // low

// =============================================================================
// output functions
// =============================================================================

static int hash_entry_compare(const void* arg1, const void* arg2) {
    int result;

    const HashEntry* e1 = *static_cast<HashEntry* const*>(arg1);
    const HashEntry* e2 = *static_cast<HashEntry* const*>(arg2);

    // if one or both arg pointers are null, deal gracefully
    if (e1 == NULL) {
        result = (e2 == NULL) ? 0 : 1;
    } else if (e2 == NULL) {
        result = -1;
    } else {
        size_t nbAlloc1 = e1->allocations;
        size_t nbAlloc2 = e2->allocations;
        size_t size1 = e1->size & ~SIZE_FLAG_MASK;
        size_t size2 = e2->size & ~SIZE_FLAG_MASK;
        size_t alloc1 = nbAlloc1 * size1;
        size_t alloc2 = nbAlloc2 * size2;

        // sort in descending order by:
        // 1) total size
        // 2) number of allocations
        //
        // This is used for sorting, not determination of equality, so we don't
        // need to compare the bit flags.
        if (alloc1 > alloc2) {
            result = -1;
        } else if (alloc1 < alloc2) {
            result = 1;
        } else {
            if (nbAlloc1 > nbAlloc2) {
                result = -1;
            } else if (nbAlloc1 < nbAlloc2) {
                result = 1;
            } else {
                result = 0;
            }
        }
    }
    return result;
}

/*
 * Retrieve native heap information.
 *
 * "*info" is set to a buffer we allocate
 * "*overallSize" is set to the size of the "info" buffer
 * "*infoSize" is set to the size of a single entry
 * "*totalMemory" is set to the sum of all allocations we're tracking; does
 *   not include heap overhead
 * "*backtraceSize" is set to the maximum number of entries in the back trace
 */
extern "C" void get_malloc_leak_info(uint8_t** info, size_t* overallSize,
        size_t* infoSize, size_t* totalMemory, size_t* backtraceSize) {
    // don't do anything if we have invalid arguments
    if (info == NULL || overallSize == NULL || infoSize == NULL ||
            totalMemory == NULL || backtraceSize == NULL) {
        return;
    }
    *totalMemory = 0;

    ScopedPthreadMutexLocker locker(&gAllocationsMutex);

    if (gHashTable.count == 0) {
        *info = NULL;
        *overallSize = 0;
        *infoSize = 0;
        *backtraceSize = 0;
        return;
    }

    HashEntry** list = static_cast<HashEntry**>(dlmalloc(sizeof(void*) * gHashTable.count));

    // get the entries into an array to be sorted
    int index = 0;
    for (size_t i = 0 ; i < HASHTABLE_SIZE ; ++i) {
        HashEntry* entry = gHashTable.slots[i];
        while (entry != NULL) {
            list[index] = entry;
            *totalMemory = *totalMemory +
                ((entry->size & ~SIZE_FLAG_MASK) * entry->allocations);
            index++;
            entry = entry->next;
        }
    }

    // XXX: the protocol doesn't allow variable size for the stack trace (yet)
    *infoSize = (sizeof(size_t) * 2) + (sizeof(intptr_t) * BACKTRACE_SIZE);
    *overallSize = *infoSize * gHashTable.count;
    *backtraceSize = BACKTRACE_SIZE;

    // now get a byte array big enough for this
    *info = static_cast<uint8_t*>(dlmalloc(*overallSize));

    if (*info == NULL) {
        *overallSize = 0;
        dlfree(list);
        return;
    }

    qsort(list, gHashTable.count, sizeof(void*), hash_entry_compare);

    uint8_t* head = *info;
    const int count = gHashTable.count;
    for (int i = 0 ; i < count ; ++i) {
        HashEntry* entry = list[i];
        size_t entrySize = (sizeof(size_t) * 2) + (sizeof(intptr_t) * entry->numEntries);
        if (entrySize < *infoSize) {
            /* we're writing less than a full entry, clear out the rest */
            memset(head + entrySize, 0, *infoSize - entrySize);
        } else {
            /* make sure the amount we're copying doesn't exceed the limit */
            entrySize = *infoSize;
        }
        memcpy(head, &(entry->size), entrySize);
        head += *infoSize;
    }

    dlfree(list);
}

extern "C" void free_malloc_leak_info(uint8_t* info) {
    dlfree(info);
}

extern "C" struct mallinfo mallinfo() {
    return dlmallinfo();
}

extern "C" size_t malloc_usable_size(void* mem) {
    return dlmalloc_usable_size(mem);
}

extern "C" void* valloc(size_t bytes) {
    return dlvalloc(bytes);
}

extern "C" void* pvalloc(size_t bytes) {
    return dlpvalloc(bytes);
}

extern "C" int posix_memalign(void** memptr, size_t alignment, size_t size) {
    return dlposix_memalign(memptr, alignment, size);
}

/* Support for malloc debugging.
 * Note that if USE_DL_PREFIX is not defined, it's assumed that memory
 * allocation routines are implemented somewhere else, so all our custom
 * malloc routines should not be compiled at all.
 */
#ifdef USE_DL_PREFIX

/* Table for dispatching malloc calls, initialized with default dispatchers. */
extern const MallocDebug __libc_malloc_default_dispatch;
const MallocDebug __libc_malloc_default_dispatch __attribute__((aligned(32))) = {
    dlmalloc, dlfree, dlcalloc, dlrealloc, dlmemalign
};

/* Selector of dispatch table to use for dispatching malloc calls. */
const MallocDebug* __libc_malloc_dispatch = &__libc_malloc_default_dispatch;

extern "C" void* malloc(size_t bytes) {
    return __libc_malloc_dispatch->malloc(bytes);
}

extern "C" void free(void* mem) {
    __libc_malloc_dispatch->free(mem);
}

extern "C" void* calloc(size_t n_elements, size_t elem_size) {
    return __libc_malloc_dispatch->calloc(n_elements, elem_size);
}

extern "C" void* realloc(void* oldMem, size_t bytes) {
    return __libc_malloc_dispatch->realloc(oldMem, bytes);
}

extern "C" void* memalign(size_t alignment, size_t bytes) {
    return __libc_malloc_dispatch->memalign(alignment, bytes);
}

/* We implement malloc debugging only in libc.so, so code bellow
 * must be excluded if we compile this file for static libc.a
 */
#ifndef LIBC_STATIC
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <stdio.h>
#include "logd.h"

/* Table for dispatching malloc calls, depending on environment. */
static MallocDebug gMallocUse __attribute__((aligned(32))) = {
    dlmalloc, dlfree, dlcalloc, dlrealloc, dlmemalign
};

extern char* __progname;

/* Handle to shared library where actual memory allocation is implemented.
 * This library is loaded and memory allocation calls are redirected there
 * when libc.debug.malloc environment variable contains value other than
 * zero:
 * 1  - For memory leak detections.
 * 5  - For filling allocated / freed memory with patterns defined by
 *      CHK_SENTINEL_VALUE, and CHK_FILL_FREE macros.
 * 10 - For adding pre-, and post- allocation stubs in order to detect
 *      buffer overruns.
 * Note that emulator's memory allocation instrumentation is not controlled by
 * libc.debug.malloc value, but rather by emulator, started with -memcheck
 * option. Note also, that if emulator has started with -memcheck option,
 * emulator's instrumented memory allocation will take over value saved in
 * libc.debug.malloc. In other words, if emulator has started with -memcheck
 * option, libc.debug.malloc value is ignored.
 * Actual functionality for debug levels 1-10 is implemented in
 * libc_malloc_debug_leak.so, while functionality for emultor's instrumented
 * allocations is implemented in libc_malloc_debug_qemu.so and can be run inside
  * the emulator only.
 */
static void* libc_malloc_impl_handle = NULL;

/* Make sure we have MALLOC_ALIGNMENT that matches the one that is
 * used in dlmalloc. Emulator's memchecker needs this value to properly
 * align its guarding zones.
 */
#ifndef MALLOC_ALIGNMENT
#define MALLOC_ALIGNMENT ((size_t)8U)
#endif  /* MALLOC_ALIGNMENT */

/* This variable is set to the value of property libc.debug.malloc.backlog,
 * when the value of libc.debug.malloc = 10.  It determines the size of the
 * backlog we use to detect multiple frees.  If the property is not set, the
 * backlog length defaults to an internal constant defined in
 * malloc_debug_check.cpp.
 */
unsigned int malloc_double_free_backlog;

static void InitMalloc(MallocDebug* table, int debug_level, const char* prefix) {
/*
  BEGIN mtk-modified: log for debug 15 and 16
*/
  if (debug_level == 15 || debug_level == 16) {
  	__libc_android_log_print(ANDROID_LOG_INFO, "libc", "%s: using MALLOC_DEBUG = %d\n",
                           __progname, debug_level);
  } else
  	__libc_android_log_print(ANDROID_LOG_INFO, "libc", "%s: using libc.debug.malloc %d (%s)\n",
                           __progname, debug_level, prefix);
/*
  END mtk-modified.
*/

  char symbol[128];

  snprintf(symbol, sizeof(symbol), "%s_malloc", prefix);
  table->malloc = reinterpret_cast<MallocDebugMalloc>(dlsym(libc_malloc_impl_handle, symbol));
  if (table->malloc == NULL) {
      error_log("%s: dlsym(\"%s\") failed", __progname, symbol);
  }

  snprintf(symbol, sizeof(symbol), "%s_free", prefix);
  table->free = reinterpret_cast<MallocDebugFree>(dlsym(libc_malloc_impl_handle, symbol));
  if (table->free == NULL) {
      error_log("%s: dlsym(\"%s\") failed", __progname, symbol);
  }

  snprintf(symbol, sizeof(symbol), "%s_calloc", prefix);
  table->calloc = reinterpret_cast<MallocDebugCalloc>(dlsym(libc_malloc_impl_handle, symbol));
  if (table->calloc == NULL) {
      error_log("%s: dlsym(\"%s\") failed", __progname, symbol);
  }

  snprintf(symbol, sizeof(symbol), "%s_realloc", prefix);
  table->realloc = reinterpret_cast<MallocDebugRealloc>(dlsym(libc_malloc_impl_handle, symbol));
  if (table->realloc == NULL) {
      error_log("%s: dlsym(\"%s\") failed", __progname, symbol);
  }

  snprintf(symbol, sizeof(symbol), "%s_memalign", prefix);
  table->memalign = reinterpret_cast<MallocDebugMemalign>(dlsym(libc_malloc_impl_handle, symbol));
  if (table->memalign == NULL) {
      error_log("%s: dlsym(\"%s\") failed", __progname, symbol);
  }
}

/*
  BEGIN mtk-added: initialize mspace_malloc/free_stat functions
*/
#if defined(HAVE_MALLOC_DEBUG_FEATURE) && defined(HAVE_MSPACE_DEBUG_FEATURE)
#define MSPACE_MALLOC_STAT_FUNC "mtk_mspace_malloc_stat"
#define MSPACE_FREE_STAT_FUNC "mtk_mspace_free_stat"
static int mspace_stat_init() {
       int ret = 0;

	mspace_malloc_stat = 
		reinterpret_cast<MspaceMallocStat>(dlsym(libc_malloc_impl_handle, MSPACE_MALLOC_STAT_FUNC));
	if (mspace_malloc_stat == NULL) {
		error_log("%s: dlsym(\"%s\") failed", __progname, MSPACE_MALLOC_STAT_FUNC);
		ret = -1;
	}
	
	mspace_free_stat = 
		reinterpret_cast<MspaceFreeStat>(dlsym(libc_malloc_impl_handle, MSPACE_FREE_STAT_FUNC));
	if (mspace_free_stat == NULL) {
		error_log("%s: dlsym(\"%s\") failed", __progname, MSPACE_FREE_STAT_FUNC);
		ret = -2;
	}

	return ret;
}
#endif
/*
  END mtk-added.
*/

/* Initializes memory allocation framework once per process. */
static void malloc_init_impl() {
    const char* so_name = NULL;
    MallocDebugInit malloc_debug_initialize = NULL;
    unsigned int qemu_running = 0;
    unsigned int debug_level = 0;
    unsigned int memcheck_enabled = 0;
    char env[PROP_VALUE_MAX];
    char memcheck_tracing[PROP_VALUE_MAX];
    char debug_program[PROP_VALUE_MAX];

/*
  BEGIN mtk-added: 
*/
     /* 
       * since there are NO libc_malloc_debug_leak.so, libc_malloc_debug_qemu.so and 
       * libc_malloc_debug_xxx.so on user load,
       * speed up the malloc initialization.
       */
#ifdef IS_USER_BUILD
     /* Debug level 0 means that we should use dlxxx allocation
     * routines (default). */
    if (debug_level == 0) {
        return;
    }
#endif

    /*
      * debug 15 is enable by default ONLY when:
      * 1. MTK ENG load -> frame pointer, apcs, arm
      * 2. malloc debug feature is on.
      */
#if defined(HAVE_MALLOC_DEBUG_FEATURE) && defined(_MTK_ENG_)
    debug_level = 15;
#else
    debug_level = 0;
#endif
    
    // TODO: temp solution. need optimize.
#if defined(DISABLE_MALLOC_DEBUG)
    debug_level = 0;
#endif

#ifdef HAVE_MALLOC_DEBUG_FEATURE
    /* debug level priority
      * 0 < (15, 16) < (1, 5, 10) < 20
      */
    if (__system_property_get("persist.libc.debug.malloc", env)) {
        debug_level = atoi(env); // overwrite initial value(0, or 15)
    }
#endif
/*
  END mtk-added.
*/

    /* Get custom malloc debug level. Note that emulator started with
     * memory checking option will have priority over debug level set in
     * libc.debug.malloc system property. */
    if (__system_property_get("ro.kernel.qemu", env) && atoi(env)) {
        qemu_running = 1;
        if (__system_property_get("ro.kernel.memcheck", memcheck_tracing)) {
            if (memcheck_tracing[0] != '0') {
                // Emulator has started with memory tracing enabled. Enforce it.
                debug_level = 20;
                memcheck_enabled = 1;
            }
        }
    }


/*
  BEGIN mtk-modified: libc.debug.malloc will overwrite persist.libc.debug.malloc
*/
    /* If debug level has not been set by memcheck option in the emulator,
     * lets grab it from libc.debug.malloc system property. */
    if ((debug_level == 0 || debug_level == 15 || debug_level == 16)
		&& __system_property_get("libc.debug.malloc", env)) {
        debug_level = atoi(env); // overwrite previous value(0, 15 or 16)
    }
/*
  END mtk-modified: 
*/
    /* Debug level 0 means that we should use dlxxx allocation
     * routines (default). */
    if (debug_level == 0) {
        return;
    }

    /* If libc.debug.malloc.program is set and is not a substring of progname,
     * then exit.
     */
    if (__system_property_get("libc.debug.malloc.program", debug_program)) {
        if (!strstr(__progname, debug_program)) {
            return;
        }
    }

    // Lets see which .so must be loaded for the requested debug level
    switch (debug_level) {
        case 1:
        case 5:
        case 10: {
            char debug_backlog[PROP_VALUE_MAX];
            if (__system_property_get("libc.debug.malloc.backlog", debug_backlog)) {
                malloc_double_free_backlog = atoi(debug_backlog);
                info_log("%s: setting backlog length to %d\n",
                         __progname, malloc_double_free_backlog);
            }

            so_name = "/system/lib/libc_malloc_debug_leak.so";
            break;
        }
/*
  BEGIN mtk-added: 
*/
#if defined(HAVE_MALLOC_DEBUG_FEATURE) || defined(HAVE_MSPACE_DEBUG_FEATURE)
        case 15:
        case 16:
            so_name = "/system/lib/libc_malloc_debug_mtk.so";
            break;
#endif
/*
 END mtk-added.
*/
        case 20:
            // Quick check: debug level 20 can only be handled in emulator.
            if (!qemu_running) {
                error_log("%s: Debug level %d can only be set in emulator\n",
                          __progname, debug_level);
                return;
            }
            // Make sure that memory checking has been enabled in emulator.
            if (!memcheck_enabled) {
                error_log("%s: Memory checking is not enabled in the emulator\n",
                          __progname);
                return;
            }
            so_name = "/system/lib/libc_malloc_debug_qemu.so";
            break;
        default:
            error_log("%s: Debug level %d is unknown\n",
                      __progname, debug_level);
            return;
    }

    // Load .so that implements the required malloc debugging functionality.
    libc_malloc_impl_handle = dlopen(so_name, RTLD_LAZY);
    if (libc_malloc_impl_handle == NULL) {
        error_log("%s: Missing module %s required for malloc debug level %d: %s",
                  __progname, so_name, debug_level, dlerror());
        return;
    }

    // Initialize malloc debugging in the loaded module.
    malloc_debug_initialize = reinterpret_cast<MallocDebugInit>(dlsym(libc_malloc_impl_handle,
                                                                      "malloc_debug_initialize"));
    if (malloc_debug_initialize == NULL) {
        error_log("%s: Initialization routine is not found in %s\n",
                  __progname, so_name);
        dlclose(libc_malloc_impl_handle);
        return;
    }
    if (malloc_debug_initialize()) {
        dlclose(libc_malloc_impl_handle);
        return;
    }

    if (debug_level == 20) {
        // For memory checker we need to do extra initialization.
        typedef int (*MemCheckInit)(int, const char*);
        MemCheckInit memcheck_initialize =
            reinterpret_cast<MemCheckInit>(dlsym(libc_malloc_impl_handle,
                                                 "memcheck_initialize"));
        if (memcheck_initialize == NULL) {
            error_log("%s: memcheck_initialize routine is not found in %s\n",
                      __progname, so_name);
            dlclose(libc_malloc_impl_handle);
            return;
        }
        if (memcheck_initialize(MALLOC_ALIGNMENT, memcheck_tracing)) {
            dlclose(libc_malloc_impl_handle);
            return;
        }
    }

/*
  BEGIN mtk-added: 
*/
#if defined(HAVE_MALLOC_DEBUG_FEATURE) || defined(HAVE_MSPACE_DEBUG_FEATURE)
    if (debug_level == 15) {
	int sig = 0;
	int backtrace_method = -1, backtrace_size = -1;

	
        // For debug 15 we need to do extra initialization.
        Debug15ExtraInitialize debug15_extra_initialize =
                reinterpret_cast<Debug15ExtraInitialize>(dlsym(libc_malloc_impl_handle, "debug15_extra_initialize"));
        if (debug15_extra_initialize == NULL) {
            error_log("%s: malloc_debug_extra_initialize routine is not found in %s\n",
                      __progname, so_name);
            dlclose(libc_malloc_impl_handle);
	     return;
        }

		// NOTICE:
		// have to read property in this stage.
		// reading in debug15_extra_initialize does not work, while reson is unclear.
	    if (__system_property_get("persist.debug15.sig", env)) {
	        sig = atoi(env);
	    }

	    if (__system_property_get("persist.debug15.prog", env)) {		
			if (strncmp("ALL", env, sizeof("ALL")) == 0 || strstr(__progname, env)) {
				error_log("gcc 20 bt for: %s\n", __progname);
				backtrace_method = 1; // gcc unwind
				backtrace_size = 20; // back trace depth: 20
	        	}
			
	    } 
				
        if (debug15_extra_initialize(sig, backtrace_method, backtrace_size)) {
            dlclose(libc_malloc_impl_handle);
            return;
        }

		// to indicate that debug 15 is on.
		// NOTICE: This function does not exist on GB.
	    if (__system_property_get("libc.debug15.status", env)) {
	       if (strncmp("off", env, sizeof("off")) == 0)
		   __system_property_set("libc.debug15.status", "on");
	    } else
	      __system_property_set("libc.debug15.status", "on");
    }
#endif
/*
 END mtk-added.
*/

    // Initialize malloc dispatch table with appropriate routines.
    switch (debug_level) {
        case 1:
            InitMalloc(&gMallocUse, debug_level, "leak");
            break;
        case 5:
            InitMalloc(&gMallocUse, debug_level, "fill");
            break;
        case 10:
            InitMalloc(&gMallocUse, debug_level, "chk");
            break;
/*
  BEGIN mtk-added: 
*/
#if defined(HAVE_MALLOC_DEBUG_FEATURE)
        case 15:
            InitMalloc(&gMallocUse, debug_level, "mtk");
            break;
#if defined(HAVE_MSPACE_DEBUG_FEATURE)
        case 16:
            if(mspace_stat_init() == 0)
            	InitMalloc(&gMallocUse, debug_level, "mtk");
            break;
#endif //#if defined(HAVE_MSPACE_DEBUG_FEATURE)
#endif // #if defined(HAVE_MALLOC_DEBUG_FEATURE)
/*
 END mtk-added.
*/
        case 20:
            InitMalloc(&gMallocUse, debug_level, "qemu_instrumented");
            break;
        default:
            break;
    }

    // Make sure dispatch table is initialized
    if ((gMallocUse.malloc == NULL) ||
        (gMallocUse.free == NULL) ||
        (gMallocUse.calloc == NULL) ||
        (gMallocUse.realloc == NULL) ||
        (gMallocUse.memalign == NULL)) {
        error_log("%s: some symbols for libc.debug.malloc level %d were not found (see above)",
                  __progname, debug_level);
        dlclose(libc_malloc_impl_handle);
        libc_malloc_impl_handle = NULL;
    } else {
        __libc_malloc_dispatch = &gMallocUse;
    }
}

static void malloc_fini_impl() {
    if (libc_malloc_impl_handle) {
        MallocDebugFini malloc_debug_finalize =
            reinterpret_cast<MallocDebugFini>(dlsym(libc_malloc_impl_handle,
                                                    "malloc_debug_finalize"));
        if (malloc_debug_finalize) {
            malloc_debug_finalize();
        }
    }
}

static pthread_once_t  malloc_init_once_ctl = PTHREAD_ONCE_INIT;
static pthread_once_t  malloc_fini_once_ctl = PTHREAD_ONCE_INIT;

#endif  // !LIBC_STATIC
#endif  // USE_DL_PREFIX

/* Initializes memory allocation framework.
 * This routine is called from __libc_init routines implemented
 * in libc_init_static.c and libc_init_dynamic.c files.
 */
extern "C" void malloc_debug_init() {
    /* We need to initialize malloc iff we implement here custom
     * malloc routines (i.e. USE_DL_PREFIX is defined) for libc.so */
#if defined(USE_DL_PREFIX) && !defined(LIBC_STATIC)

#if 0
    stack_start = (__get_sp() & ~(4*1024 - 1)) + 4*1024;
    unsigned stacksize = 128 * 1024;
    stack_end = stack_start - stacksize;
	stack_end -= 4 * 1024;
#endif

    if (pthread_once(&malloc_init_once_ctl, malloc_init_impl)) {
        error_log("Unable to initialize malloc_debug component.");
    }
#endif  // USE_DL_PREFIX && !LIBC_STATIC
}

extern "C" void malloc_debug_fini() {
  /* We need to finalize malloc iff we implement here custom
     * malloc routines (i.e. USE_DL_PREFIX is defined) for libc.so */
#if defined(USE_DL_PREFIX) && !defined(LIBC_STATIC)
  if (pthread_once(&malloc_fini_once_ctl, malloc_fini_impl)) {
        error_log("Unable to finalize malloc_debug component.");
    }
#endif  // USE_DL_PREFIX && !LIBC_STATIC
}
/*******************the Below is for  MMAP leakage Debug *************************/
#ifndef LIBC_STATIC//MMAP_DEBUG
#include <sys/system_properties.h>
#include <dlfcn.h>
#include "logd.h"

extern char*  __progname;

static pthread_once_t  mmap_init_once_ctl = PTHREAD_ONCE_INIT;
static pthread_once_t  mmap_fini_once_ctl = PTHREAD_ONCE_INIT;
static void* libc_mmap_impl_handle = NULL;


/* Initializes memory allocation framework once per process. */
static void mmap_init_impl(void)
{
    const char* so_name = NULL;
	MmapDebugInit mmap_debug_initialize = NULL ;
    unsigned int mmap_debug_level = 0;    
    char env[PROP_VALUE_MAX];    
    char debug_program[PROP_VALUE_MAX]; 
	//mmap_bt = NULL ;
	//munmap_bt = NULL ;
	

    /* If debug level has not been set by memcheck option in the emulator,
     * lets grab it from libc.debug.malloc system property. */
   

    if ( __system_property_get("persist.libc.debug.mmap", env)) {
        mmap_debug_level = atoi(env);
    }

    if (__system_property_get("ro.build.type", env)) {
        if(strncmp(env, "eng", 3))
			mmap_debug_level = 0;
    }
    
        /* Debug level 0 means that we should use dlxxx allocation
     * routines (default). */
     
    

    /* If libc.debug.malloc.program is set and is not a substring of progname,
     * then exit.
     */
     #if 0 //meaning ??
    if (__system_property_get("libc.debug.malloc.program", debug_program)) {
        if (!strstr(__progname, debug_program)) {
            return;
        }
    }
    #endif

	/* Process need bypass */
	#if 1
    if (!strstr(__progname, "system/bin/surfaceflinger")) {/*bypass aee and aee_dumpstate*/
        mmap_debug_level = 0;	
    }  
	#endif
	if (!mmap_debug_level) {
        return;
    }
	mmap_error_log("==>Here we run SurfaceFlinger=>%s\n",__progname);
	
		so_name = "/system/lib/libc_mmap_debug_mtk.so";		    
        
     // Load .so that implements the required malloc debugging functionality.
    libc_mmap_impl_handle = dlopen(so_name, RTLD_LAZY);
    if (libc_mmap_impl_handle == NULL) {
        mmap_error_log("%s: Missing module %s required for mmap debug level %d\n",
                 __progname, so_name, mmap_debug_level);
        return;
    }

    // Initialize mmap debugging in the loaded module. 
	mmap_debug_initialize =
                reinterpret_cast<MmapDebugInit>(dlsym(libc_mmap_impl_handle, "mmap_debug_initialize"));

	
    if (mmap_debug_initialize == NULL) {
        mmap_error_log("%s: Initialization routine is not found in %s\n",
                  __progname, so_name);
        dlclose(libc_mmap_impl_handle);
        return;
    }
    if (mmap_debug_initialize()) { //// return 0: success; others: fail
        dlclose(libc_mmap_impl_handle);
        return;
    }

	
    //extern void mmap_bt(void * buffer,size_t bytes);
	mmap_bt =reinterpret_cast<Mmap_Bt>(dlsym(libc_mmap_impl_handle, "mmap_bt_mtk"));
	if(mmap_bt==NULL){
		mmap_error_log("%s: MMAP_BT routine is not found in %s\n",
                  __progname, so_name);
        dlclose(libc_mmap_impl_handle);
        return;
	}
	//extern void  munmap_bt(void* mem);
	munmap_bt=reinterpret_cast<Munmap_Bt>(dlsym(libc_mmap_impl_handle, "munmap_bt_mtk"));
		if(munmap_bt==NULL){
		mmap_error_log("%s: MUNMAP_BT routine is not found in %s\n",
                  __progname, so_name);
        dlclose(libc_mmap_impl_handle);
        return;
	}	
		
        
       // dlclose(libc_mmap_impl_handle);
        //libc_mmap_impl_handle = NULL;
} 

/* Initializes mmap Leakge debugging.
 * This routine is called from __libc_preinit routines
 */

extern "C" void mmap_debug_init(void)
{ 	
    if (pthread_once(&mmap_init_once_ctl, mmap_init_impl)) {
        mmap_error_log("Unable to initialize mmap_debug component.");
    }
}
#endif
