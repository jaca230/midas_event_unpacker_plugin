/*
   LZ4 HC - High Compression Mode of LZ4
   Header File
   Copyright (C) 2011-2015, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 source repository : https://github.com/Cyan4973/lz4
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#pragma once


//#if defined (__cplusplus)
//extern "C" {
//#endif

/*****************************
*  Includes
*****************************/
#include <stddef.h>   /* size_t */


/**************************************
*  Block Compression
**************************************/
int MLZ4_compress_HC (const char* src, char* dst, int srcSize, int maxDstSize, int compressionLevel);
/*
MLZ4_compress_HC :
    Destination buffer 'dst' must be already allocated.
    Compression completion is guaranteed if 'dst' buffer is sized to handle worst circumstances (data not compressible)
    Worst size evaluation is provided by function MLZ4_compressBound() (see "lz4.h")
      srcSize  : Max supported value is MLZ4_MAX_INPUT_SIZE (see "lz4.h")
      compressionLevel : Recommended values are between 4 and 9, although any value between 0 and 16 will work.
                         0 means "use default value" (see lz4hc.c).
                         Values >16 behave the same as 16.
      return : the number of bytes written into buffer 'dst'
            or 0 if compression fails.
*/


/* Note :
   Decompression functions are provided within LZ4 source code (see "lz4.h") (BSD license)
*/


int MLZ4_sizeofStateHC(void);
int MLZ4_compress_HC_extStateHC(void* state, const char* src, char* dst, int srcSize, int maxDstSize, int compressionLevel);
/*
MLZ4_compress_HC_extStateHC() :
   Use this function if you prefer to manually allocate memory for compression tables.
   To know how much memory must be allocated for the compression tables, use :
      int MLZ4_sizeofStateHC();

   Allocated memory must be aligned on 8-bytes boundaries (which a normal malloc() will do properly).

   The allocated memory can then be provided to the compression functions using 'void* state' parameter.
   MLZ4_compress_HC_extStateHC() is equivalent to previously described function.
   It just uses externally allocated memory for stateHC.
*/


/**************************************
*  Streaming Compression
**************************************/
#define MLZ4_STREAMHCSIZE        262192
#define MLZ4_STREAMHCSIZE_SIZET (MLZ4_STREAMHCSIZE / sizeof(size_t))
typedef struct { size_t table[MLZ4_STREAMHCSIZE_SIZET]; } MLZ4_streamHC_t;
/*
  MLZ4_streamHC_t
  This structure allows static allocation of LZ4 HC streaming state.
  State must then be initialized using MLZ4_resetStreamHC() before first use.

  Static allocation should only be used in combination with static linking.
  If you want to use LZ4 as a DLL, please use construction functions below, which are future-proof.
*/


MLZ4_streamHC_t* MLZ4_createStreamHC(void);
int             MLZ4_freeStreamHC (MLZ4_streamHC_t* streamHCPtr);
/*
  These functions create and release memory for LZ4 HC streaming state.
  Newly created states are already initialized.
  Existing state space can be re-used anytime using MLZ4_resetStreamHC().
  If you use LZ4 as a DLL, use these functions instead of static structure allocation,
  to avoid size mismatch between different versions.
*/

void MLZ4_resetStreamHC (MLZ4_streamHC_t* streamHCPtr, int compressionLevel);
int  MLZ4_loadDictHC (MLZ4_streamHC_t* streamHCPtr, const char* dictionary, int dictSize);

int MLZ4_compress_HC_continue (MLZ4_streamHC_t* streamHCPtr, const char* src, char* dst, int srcSize, int maxDstSize);

int MLZ4_saveDictHC (MLZ4_streamHC_t* streamHCPtr, char* safeBuffer, int maxDictSize);

/*
  These functions compress data in successive blocks of any size, using previous blocks as dictionary.
  One key assumption is that previous blocks (up to 64 KB) remain read-accessible while compressing next blocks.
  There is an exception for ring buffers, which can be smaller 64 KB.
  Such case is automatically detected and correctly handled by MLZ4_compress_HC_continue().

  Before starting compression, state must be properly initialized, using MLZ4_resetStreamHC().
  A first "fictional block" can then be designated as initial dictionary, using MLZ4_loadDictHC() (Optional).

  Then, use MLZ4_compress_HC_continue() to compress each successive block.
  It works like MLZ4_compress_HC(), but use previous memory blocks as dictionary to improve compression.
  Previous memory blocks (including initial dictionary when present) must remain accessible and unmodified during compression.
  As a reminder, size 'dst' buffer to handle worst cases, using MLZ4_compressBound(), to ensure success of compression operation.

  If, for any reason, previous data blocks can't be preserved unmodified in memory during next compression block,
  you must save it to a safer memory space, using MLZ4_saveDictHC().
  Return value of MLZ4_saveDictHC() is the size of dictionary effectively saved into 'safeBuffer'.
*/



/**************************************
*  Deprecated Functions
**************************************/
/* Deprecate Warnings */
/* Should these warnings messages be a problem,
   it is generally possible to disable them,
   with -Wno-deprecated-declarations for gcc
   or _CRT_SECURE_NO_WARNINGS in Visual for example.
   You can also define MLZ4_DEPRECATE_WARNING_DEFBLOCK. */
#ifndef MLZ4_DEPRECATE_WARNING_DEFBLOCK
#  define MLZ4_DEPRECATE_WARNING_DEFBLOCK
#  define MLZ4_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#  if (MLZ4_GCC_VERSION >= 405) || defined(__clang__)
#    define MLZ4_DEPRECATED(message) __attribute__((deprecated(message)))
#  elif (MLZ4_GCC_VERSION >= 301)
#    define MLZ4_DEPRECATED(message) __attribute__((deprecated))
#  elif defined(_MSC_VER)
#    define MLZ4_DEPRECATED(message) __declspec(deprecated(message))
#  else
#    pragma message("WARNING: You need to implement MLZ4_DEPRECATED for this compiler")
#    define MLZ4_DEPRECATED(message)
#  endif
#endif // MLZ4_DEPRECATE_WARNING_DEFBLOCK

/* compression functions */
/* these functions are planned to trigger warning messages by r131 approximately */
int MLZ4_compressHC                (const char* source, char* dest, int inputSize);
int MLZ4_compressHC_limitedOutput  (const char* source, char* dest, int inputSize, int maxOutputSize);
int MLZ4_compressHC2               (const char* source, char* dest, int inputSize, int compressionLevel);
int MLZ4_compressHC2_limitedOutput (const char* source, char* dest, int inputSize, int maxOutputSize, int compressionLevel);
int MLZ4_compressHC_withStateHC               (void* state, const char* source, char* dest, int inputSize);
int MLZ4_compressHC_limitedOutput_withStateHC (void* state, const char* source, char* dest, int inputSize, int maxOutputSize);
int MLZ4_compressHC2_withStateHC              (void* state, const char* source, char* dest, int inputSize, int compressionLevel);
int MLZ4_compressHC2_limitedOutput_withStateHC(void* state, const char* source, char* dest, int inputSize, int maxOutputSize, int compressionLevel);
int MLZ4_compressHC_continue               (MLZ4_streamHC_t* MLZ4_streamHCPtr, const char* source, char* dest, int inputSize);
int MLZ4_compressHC_limitedOutput_continue (MLZ4_streamHC_t* MLZ4_streamHCPtr, const char* source, char* dest, int inputSize, int maxOutputSize);

/* Streaming functions following the older model; should no longer be used */
MLZ4_DEPRECATED("use MLZ4_createStreamHC() instead") void* MLZ4_createHC (char* inputBuffer);
MLZ4_DEPRECATED("use MLZ4_saveDictHC() instead")     char* MLZ4_slideInputBufferHC (void* LZ4HC_Data);
MLZ4_DEPRECATED("use MLZ4_freeStreamHC() instead")   int   MLZ4_freeHC (void* LZ4HC_Data);
MLZ4_DEPRECATED("use MLZ4_compress_HC_continue() instead") int   MLZ4_compressHC2_continue (void* LZ4HC_Data, const char* source, char* dest, int inputSize, int compressionLevel);
MLZ4_DEPRECATED("use MLZ4_compress_HC_continue() instead") int   MLZ4_compressHC2_limitedOutput_continue (void* LZ4HC_Data, const char* source, char* dest, int inputSize, int maxOutputSize, int compressionLevel);
MLZ4_DEPRECATED("use MLZ4_createStreamHC() instead") int   MLZ4_sizeofStreamStateHC(void);
MLZ4_DEPRECATED("use MLZ4_resetStreamHC() instead")  int   MLZ4_resetStreamStateHC(void* state, char* inputBuffer);


//#if defined (__cplusplus)
//}
//#endif