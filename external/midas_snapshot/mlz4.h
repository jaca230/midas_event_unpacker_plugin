/*
   LZ4 - Fast LZ compression algorithm
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

/*
 * lz4.h provides block compression functions, and gives full buffer control to programmer.
 * If you need to generate inter-operable compressed data (respecting LZ4 frame specification),
 * and can let the library handle its own memory, please use lz4frame.h instead.
*/

/**************************************
*  Version
**************************************/
#define MLZ4_VERSION_MAJOR    1    /* for breaking interface changes  */
#define MLZ4_VERSION_MINOR    7    /* for new (non-breaking) interface capabilities */
#define MLZ4_VERSION_RELEASE  1    /* for tweaks, bug-fixes, or development */
#define MLZ4_VERSION_NUMBER (MLZ4_VERSION_MAJOR *100*100 + MLZ4_VERSION_MINOR *100 + MLZ4_VERSION_RELEASE)
int MLZ4_versionNumber (void);

/**************************************
*  Tuning parameter
**************************************/
/*
 * MLZ4_MEMORY_USAGE :
 * Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
 * Increasing memory usage improves compression ratio
 * Reduced memory usage can improve speed, due to cache effect
 * Default value is 14, for 16KB, which nicely fits into Intel x86 L1 cache
 */
#define MLZ4_MEMORY_USAGE 14


/**************************************
*  Simple Functions
**************************************/

int MLZ4_compress_default(const char* source, char* dest, int sourceSize, int maxDestSize);
int MLZ4_decompress_safe (const char* source, char* dest, int compressedSize, int maxDecompressedSize);

/*
MLZ4_compress_default() :
    Compresses 'sourceSize' bytes from buffer 'source'
    into already allocated 'dest' buffer of size 'maxDestSize'.
    Compression is guaranteed to succeed if 'maxDestSize' >= MLZ4_compressBound(sourceSize).
    It also runs faster, so it's a recommended setting.
    If the function cannot compress 'source' into a more limited 'dest' budget,
    compression stops *immediately*, and the function result is zero.
    As a consequence, 'dest' content is not valid.
    This function never writes outside 'dest' buffer, nor read outside 'source' buffer.
        sourceSize  : Max supported value is MLZ4_MAX_INPUT_VALUE
        maxDestSize : full or partial size of buffer 'dest' (which must be already allocated)
        return : the number of bytes written into buffer 'dest' (necessarily <= maxOutputSize)
              or 0 if compression fails

MLZ4_decompress_safe() :
    compressedSize : is the precise full size of the compressed block.
    maxDecompressedSize : is the size of destination buffer, which must be already allocated.
    return : the number of bytes decompressed into destination buffer (necessarily <= maxDecompressedSize)
             If destination buffer is not large enough, decoding will stop and output an error code (<0).
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             This function is protected against buffer overflow exploits, including malicious data packets.
             It never writes outside output buffer, nor reads outside input buffer.
*/


/**************************************
*  Advanced Functions
**************************************/
#define MLZ4_MAX_INPUT_SIZE        0x7E000000   /* 2 113 929 216 bytes */
#define MLZ4_COMPRESSBOUND(isize)  ((unsigned)(isize) > (unsigned)MLZ4_MAX_INPUT_SIZE ? 0 : (isize) + ((isize)/255) + 16)

/*
MLZ4_compressBound() :
    Provides the maximum size that LZ4 compression may output in a "worst case" scenario (input data not compressible)
    This function is primarily useful for memory allocation purposes (destination buffer size).
    Macro MLZ4_COMPRESSBOUND() is also provided for compilation-time evaluation (stack memory allocation for example).
    Note that MLZ4_compress_default() compress faster when dest buffer size is >= MLZ4_compressBound(srcSize)
        inputSize  : max supported value is MLZ4_MAX_INPUT_SIZE
        return : maximum output size in a "worst case" scenario
              or 0, if input size is too large ( > MLZ4_MAX_INPUT_SIZE)
*/
int MLZ4_compressBound(int inputSize);

/*
MLZ4_compress_fast() :
    Same as MLZ4_compress_default(), but allows to select an "acceleration" factor.
    The larger the acceleration value, the faster the algorithm, but also the lesser the compression.
    It's a trade-off. It can be fine tuned, with each successive value providing roughly +~3% to speed.
    An acceleration value of "1" is the same as regular MLZ4_compress_default()
    Values <= 0 will be replaced by ACCELERATION_DEFAULT (see lz4.c), which is 1.
*/
int MLZ4_compress_fast (const char* source, char* dest, int sourceSize, int maxDestSize, int acceleration);


/*
MLZ4_compress_fast_extState() :
    Same compression function, just using an externally allocated memory space to store compression state.
    Use MLZ4_sizeofState() to know how much memory must be allocated,
    and allocate it on 8-bytes boundaries (using malloc() typically).
    Then, provide it as 'void* state' to compression function.
*/
int MLZ4_sizeofState(void);
int MLZ4_compress_fast_extState (void* state, const char* source, char* dest, int inputSize, int maxDestSize, int acceleration);


/*
MLZ4_compress_destSize() :
    Reverse the logic, by compressing as much data as possible from 'source' buffer
    into already allocated buffer 'dest' of size 'targetDestSize'.
    This function either compresses the entire 'source' content into 'dest' if it's large enough,
    or fill 'dest' buffer completely with as much data as possible from 'source'.
        *sourceSizePtr : will be modified to indicate how many bytes where read from 'source' to fill 'dest'.
                         New value is necessarily <= old value.
        return : Nb bytes written into 'dest' (necessarily <= targetDestSize)
              or 0 if compression fails
*/
int MLZ4_compress_destSize (const char* source, char* dest, int* sourceSizePtr, int targetDestSize);


/*
MLZ4_decompress_fast() :
    originalSize : is the original and therefore uncompressed size
    return : the number of bytes read from the source buffer (in other words, the compressed size)
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             Destination buffer must be already allocated. Its size must be a minimum of 'originalSize' bytes.
    note : This function fully respect memory boundaries for properly formed compressed data.
           It is a bit faster than MLZ4_decompress_safe().
           However, it does not provide any protection against intentionally modified data stream (malicious input).
           Use this function in trusted environment only (data to decode comes from a trusted source).
*/
int MLZ4_decompress_fast (const char* source, char* dest, int originalSize);

/*
MLZ4_decompress_safe_partial() :
    This function decompress a compressed block of size 'compressedSize' at position 'source'
    into destination buffer 'dest' of size 'maxDecompressedSize'.
    The function tries to stop decompressing operation as soon as 'targetOutputSize' has been reached,
    reducing decompression time.
    return : the number of bytes decoded in the destination buffer (necessarily <= maxDecompressedSize)
       Note : this number can be < 'targetOutputSize' should the compressed block to decode be smaller.
             Always control how many bytes were decoded.
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             This function never writes outside of output buffer, and never reads outside of input buffer. It is therefore protected against malicious data packets
*/
int MLZ4_decompress_safe_partial (const char* source, char* dest, int compressedSize, int targetOutputSize, int maxDecompressedSize);


/***********************************************
*  Streaming Compression Functions
***********************************************/
#define MLZ4_STREAMSIZE_U64 ((1 << (MLZ4_MEMORY_USAGE-3)) + 4)
#define MLZ4_STREAMSIZE     (MLZ4_STREAMSIZE_U64 * sizeof(long long))
/*
 * MLZ4_stream_t
 * information structure to track an LZ4 stream.
 * important : init this structure content before first use !
 * note : only allocated directly the structure if you are statically linking LZ4
 *        If you are using liblz4 as a DLL, please use below construction methods instead.
 */
typedef struct { long long table[MLZ4_STREAMSIZE_U64]; } MLZ4_stream_t;

/*
 * MLZ4_resetStream
 * Use this function to init an allocated MLZ4_stream_t structure
 */
void MLZ4_resetStream (MLZ4_stream_t* streamPtr);

/*
 * MLZ4_createStream will allocate and initialize an MLZ4_stream_t structure
 * MLZ4_freeStream releases its memory.
 * In the context of a DLL (liblz4), please use these methods rather than the static struct.
 * They are more future proof, in case of a change of MLZ4_stream_t size.
 */
MLZ4_stream_t* MLZ4_createStream(void);
int           MLZ4_freeStream (MLZ4_stream_t* streamPtr);

/*
 * MLZ4_loadDict
 * Use this function to load a static dictionary into MLZ4_stream.
 * Any previous data will be forgotten, only 'dictionary' will remain in memory.
 * Loading a size of 0 is allowed.
 * Return : dictionary size, in bytes (necessarily <= 64 KB)
 */
int MLZ4_loadDict (MLZ4_stream_t* streamPtr, const char* dictionary, int dictSize);

/*
 * MLZ4_compress_fast_continue
 * Compress buffer content 'src', using data from previously compressed blocks as dictionary to improve compression ratio.
 * Important : Previous data blocks are assumed to still be present and unmodified !
 * 'dst' buffer must be already allocated.
 * If maxDstSize >= MLZ4_compressBound(srcSize), compression is guaranteed to succeed, and runs faster.
 * If not, and if compressed data cannot fit into 'dst' buffer size, compression stops, and function returns a zero.
 */
int MLZ4_compress_fast_continue (MLZ4_stream_t* streamPtr, const char* src, char* dst, int srcSize, int maxDstSize, int acceleration);

/*
 * MLZ4_saveDict
 * If previously compressed data block is not guaranteed to remain available at its memory location
 * save it into a safer place (char* safeBuffer)
 * Note : you don't need to call MLZ4_loadDict() afterwards,
 *        dictionary is immediately usable, you can therefore call MLZ4_compress_fast_continue()
 * Return : saved dictionary size in bytes (necessarily <= dictSize), or 0 if error
 */
int MLZ4_saveDict (MLZ4_stream_t* streamPtr, char* safeBuffer, int dictSize);


/************************************************
*  Streaming Decompression Functions
************************************************/

#define MLZ4_STREAMDECODESIZE_U64  4
#define MLZ4_STREAMDECODESIZE     (MLZ4_STREAMDECODESIZE_U64 * sizeof(unsigned long long))
typedef struct { unsigned long long table[MLZ4_STREAMDECODESIZE_U64]; } MLZ4_streamDecode_t;
/*
 * MLZ4_streamDecode_t
 * information structure to track an LZ4 stream.
 * init this structure content using MLZ4_setStreamDecode or memset() before first use !
 *
 * In the context of a DLL (liblz4) please prefer usage of construction methods below.
 * They are more future proof, in case of a change of MLZ4_streamDecode_t size in the future.
 * MLZ4_createStreamDecode will allocate and initialize an MLZ4_streamDecode_t structure
 * MLZ4_freeStreamDecode releases its memory.
 */
MLZ4_streamDecode_t* MLZ4_createStreamDecode(void);
int                 MLZ4_freeStreamDecode (MLZ4_streamDecode_t* MLZ4_stream);

/*
 * MLZ4_setStreamDecode
 * Use this function to instruct where to find the dictionary.
 * Setting a size of 0 is allowed (same effect as reset).
 * Return : 1 if OK, 0 if error
 */
int MLZ4_setStreamDecode (MLZ4_streamDecode_t* MLZ4_streamDecode, const char* dictionary, int dictSize);

/*
*_continue() :
    These decoding functions allow decompression of multiple blocks in "streaming" mode.
    Previously decoded blocks *must* remain available at the memory position where they were decoded (up to 64 KB)
    In the case of a ring buffers, decoding buffer must be either :
    - Exactly same size as encoding buffer, with same update rule (block boundaries at same positions)
      In which case, the decoding & encoding ring buffer can have any size, including very small ones ( < 64 KB).
    - Larger than encoding buffer, by a minimum of maxBlockSize more bytes.
      maxBlockSize is implementation dependent. It's the maximum size you intend to compress into a single block.
      In which case, encoding and decoding buffers do not need to be synchronized,
      and encoding ring buffer can have any size, including small ones ( < 64 KB).
    - _At least_ 64 KB + 8 bytes + maxBlockSize.
      In which case, encoding and decoding buffers do not need to be synchronized,
      and encoding ring buffer can have any size, including larger than decoding buffer.
    Whenever these conditions are not possible, save the last 64KB of decoded data into a safe buffer,
    and indicate where it is saved using MLZ4_setStreamDecode()
*/
int MLZ4_decompress_safe_continue (MLZ4_streamDecode_t* MLZ4_streamDecode, const char* source, char* dest, int compressedSize, int maxDecompressedSize);
int MLZ4_decompress_fast_continue (MLZ4_streamDecode_t* MLZ4_streamDecode, const char* source, char* dest, int originalSize);


/*
Advanced decoding functions :
*_usingDict() :
    These decoding functions work the same as
    a combination of MLZ4_setStreamDecode() followed by MLZ4_decompress_x_continue()
    They are stand-alone. They don't need nor update an MLZ4_streamDecode_t structure.
*/
int MLZ4_decompress_safe_usingDict (const char* source, char* dest, int compressedSize, int maxDecompressedSize, const char* dictStart, int dictSize);
int MLZ4_decompress_fast_usingDict (const char* source, char* dest, int originalSize, const char* dictStart, int dictSize);



/**************************************
*  Obsolete Functions
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
#endif /* MLZ4_DEPRECATE_WARNING_DEFBLOCK */

/* Obsolete compression functions */
/* These functions are planned to start generate warnings by r131 approximately */
int MLZ4_compress               (const char* source, char* dest, int sourceSize);
int MLZ4_compress_limitedOutput (const char* source, char* dest, int sourceSize, int maxOutputSize);
int MLZ4_compress_withState               (void* state, const char* source, char* dest, int inputSize);
int MLZ4_compress_limitedOutput_withState (void* state, const char* source, char* dest, int inputSize, int maxOutputSize);
int MLZ4_compress_continue                (MLZ4_stream_t* MLZ4_streamPtr, const char* source, char* dest, int inputSize);
int MLZ4_compress_limitedOutput_continue  (MLZ4_stream_t* MLZ4_streamPtr, const char* source, char* dest, int inputSize, int maxOutputSize);

/* Obsolete decompression functions */
/* These function names are completely deprecated and must no longer be used.
   They are only provided here for compatibility with older programs.
    - MLZ4_uncompress is the same as MLZ4_decompress_fast
    - MLZ4_uncompress_unknownOutputSize is the same as MLZ4_decompress_safe
   These function prototypes are now disabled; uncomment them only if you really need them.
   It is highly recommended to stop using these prototypes and migrate to maintained ones */
/* int MLZ4_uncompress (const char* source, char* dest, int outputSize); */
/* int MLZ4_uncompress_unknownOutputSize (const char* source, char* dest, int isize, int maxOutputSize); */

/* Obsolete streaming functions; use new streaming interface whenever possible */
MLZ4_DEPRECATED("use MLZ4_createStream() instead") void* MLZ4_create (char* inputBuffer);
MLZ4_DEPRECATED("use MLZ4_createStream() instead") int   MLZ4_sizeofStreamState(void);
MLZ4_DEPRECATED("use MLZ4_resetStream() instead")  int   MLZ4_resetStreamState(void* state, char* inputBuffer);
MLZ4_DEPRECATED("use MLZ4_saveDict() instead")     char* MLZ4_slideInputBuffer (void* state);

/* Obsolete streaming decoding functions */
MLZ4_DEPRECATED("use MLZ4_decompress_safe_usingDict() instead") int MLZ4_decompress_safe_withPrefix64k (const char* src, char* dst, int compressedSize, int maxDstSize);
MLZ4_DEPRECATED("use MLZ4_decompress_fast_usingDict() instead") int MLZ4_decompress_fast_withPrefix64k (const char* src, char* dst, int originalSize);


//#if defined (__cplusplus)
//}
//#endif