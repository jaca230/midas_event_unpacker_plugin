/*
   LZ4 auto-framing library
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

/* LZ4F is a stand-alone API to create LZ4-compressed frames
 * fully conformant to specification v1.5.1.
 * All related operations, including memory management, are handled by the library.
 * You don't need lz4.h when using lz4frame.h.
 * */

#pragma once

//#if defined (__cplusplus)
//extern "C" {
//#endif

/**************************************
*  Includes
**************************************/
#include <stddef.h>   /* size_t */


/**************************************
 * Error management
 * ************************************/
typedef size_t MLZ4F_errorCode_t;

unsigned    MLZ4F_isError(MLZ4F_errorCode_t code);
const char* MLZ4F_getErrorName(MLZ4F_errorCode_t code);   /* return error code string; useful for debugging */


/**************************************
 * Frame compression types
 * ************************************/
//#define MLZ4F_DISABLE_OBSOLETE_ENUMS
#ifndef MLZ4F_DISABLE_OBSOLETE_ENUMS
#  define MLZ4F_OBSOLETE_ENUM(x) ,x
#else
#  define MLZ4F_OBSOLETE_ENUM(x)
#endif

typedef enum {
    MLZ4F_default=0,
    MLZ4F_max64KB=4,
    MLZ4F_max256KB=5,
    MLZ4F_max1MB=6,
    MLZ4F_max4MB=7
    MLZ4F_OBSOLETE_ENUM(max64KB = MLZ4F_max64KB)
    MLZ4F_OBSOLETE_ENUM(max256KB = MLZ4F_max256KB)
    MLZ4F_OBSOLETE_ENUM(max1MB = MLZ4F_max1MB)
    MLZ4F_OBSOLETE_ENUM(max4MB = MLZ4F_max4MB)
} MLZ4F_blockSizeID_t;

typedef enum {
    MLZ4F_blockLinked=0,
    MLZ4F_blockIndependent
    MLZ4F_OBSOLETE_ENUM(blockLinked = MLZ4F_blockLinked)
    MLZ4F_OBSOLETE_ENUM(blockIndependent = MLZ4F_blockIndependent)
} MLZ4F_blockMode_t;

typedef enum {
    MLZ4F_noContentChecksum=0,
    MLZ4F_contentChecksumEnabled
    MLZ4F_OBSOLETE_ENUM(noContentChecksum = MLZ4F_noContentChecksum)
    MLZ4F_OBSOLETE_ENUM(contentChecksumEnabled = MLZ4F_contentChecksumEnabled)
} MLZ4F_contentChecksum_t;

typedef enum {
    MLZ4F_frame=0,
    MLZ4F_skippableFrame
    MLZ4F_OBSOLETE_ENUM(skippableFrame = MLZ4F_skippableFrame)
} MLZ4F_frameType_t;

#ifndef MLZ4F_DISABLE_OBSOLETE_ENUMS
typedef MLZ4F_blockSizeID_t blockSizeID_t;
typedef MLZ4F_blockMode_t blockMode_t;
typedef MLZ4F_frameType_t frameType_t;
typedef MLZ4F_contentChecksum_t contentChecksum_t;
#endif

typedef struct {
  MLZ4F_blockSizeID_t     blockSizeID;           /* max64KB, max256KB, max1MB, max4MB ; 0 == default */
  MLZ4F_blockMode_t       blockMode;             /* blockLinked, blockIndependent ; 0 == default */
  MLZ4F_contentChecksum_t contentChecksumFlag;   /* noContentChecksum, contentChecksumEnabled ; 0 == default  */
  MLZ4F_frameType_t       frameType;             /* MLZ4F_frame, skippableFrame ; 0 == default */
  unsigned long long     contentSize;           /* Size of uncompressed (original) content ; 0 == unknown */
  unsigned               reserved[2];           /* must be zero for forward compatibility */
} MLZ4F_frameInfo_t;

typedef struct {
  MLZ4F_frameInfo_t frameInfo;
  int      compressionLevel;       /* 0 == default (fast mode); values above 16 count as 16; values below 0 count as 0 */
  unsigned autoFlush;              /* 1 == always flush (reduce need for tmp buffer) */
  unsigned reserved[4];            /* must be zero for forward compatibility */
} MLZ4F_preferences_t;


/***********************************
 * Simple compression function
 * *********************************/
size_t MLZ4F_compressFrameBound(size_t srcSize, const MLZ4F_preferences_t* preferencesPtr);

size_t MLZ4F_compressFrame(void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const MLZ4F_preferences_t* preferencesPtr);
/* MLZ4F_compressFrame()
 * Compress an entire srcBuffer into a valid LZ4 frame, as defined by specification v1.5.1
 * The most important rule is that dstBuffer MUST be large enough (dstMaxSize) to ensure compression completion even in worst case.
 * You can get the minimum value of dstMaxSize by using MLZ4F_compressFrameBound()
 * If this condition is not respected, MLZ4F_compressFrame() will fail (result is an errorCode)
 * The MLZ4F_preferences_t structure is optional : you can provide NULL as argument. All preferences will be set to default.
 * The result of the function is the number of bytes written into dstBuffer.
 * The function outputs an error code if it fails (can be tested using MLZ4F_isError())
 */



/**********************************
*  Advanced compression functions
**********************************/
typedef struct MLZ4F_cctx_s* MLZ4F_compressionContext_t;   /* must be aligned on 8-bytes */

typedef struct {
  unsigned stableSrc;    /* 1 == src content will remain available on future calls to MLZ4F_compress(); avoid saving src content within tmp buffer as future dictionary */
  unsigned reserved[3];
} MLZ4F_compressOptions_t;

/* Resource Management */

#define MLZ4F_VERSION 100
MLZ4F_errorCode_t MLZ4F_createCompressionContext(MLZ4F_compressionContext_t* cctxPtr, unsigned version);
MLZ4F_errorCode_t MLZ4F_freeCompressionContext(MLZ4F_compressionContext_t cctx);
/* MLZ4F_createCompressionContext() :
 * The first thing to do is to create a compressionContext object, which will be used in all compression operations.
 * This is achieved using MLZ4F_createCompressionContext(), which takes as argument a version and an MLZ4F_preferences_t structure.
 * The version provided MUST be MLZ4F_VERSION. It is intended to track potential version differences between different binaries.
 * The function will provide a pointer to a fully allocated MLZ4F_compressionContext_t object.
 * If the result MLZ4F_errorCode_t is not zero, there was an error during context creation.
 * Object can release its memory using MLZ4F_freeCompressionContext();
 */


/* Compression */

size_t MLZ4F_compressBegin(MLZ4F_compressionContext_t cctx, void* dstBuffer, size_t dstMaxSize, const MLZ4F_preferences_t* prefsPtr);
/* MLZ4F_compressBegin() :
 * will write the frame header into dstBuffer.
 * dstBuffer must be large enough to accommodate a header (dstMaxSize). Maximum header size is 15 bytes.
 * The MLZ4F_preferences_t structure is optional : you can provide NULL as argument, all preferences will then be set to default.
 * The result of the function is the number of bytes written into dstBuffer for the header
 * or an error code (can be tested using MLZ4F_isError())
 */

size_t MLZ4F_compressBound(size_t srcSize, const MLZ4F_preferences_t* prefsPtr);
/* MLZ4F_compressBound() :
 * Provides the minimum size of Dst buffer given srcSize to handle worst case situations.
 * Different preferences can produce different results.
 * prefsPtr is optional : you can provide NULL as argument, all preferences will then be set to cover worst case.
 * This function includes frame termination cost (4 bytes, or 8 if frame checksum is enabled)
 */

size_t MLZ4F_compressUpdate(MLZ4F_compressionContext_t cctx, void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const MLZ4F_compressOptions_t* cOptPtr);
/* MLZ4F_compressUpdate()
 * MLZ4F_compressUpdate() can be called repetitively to compress as much data as necessary.
 * The most important rule is that dstBuffer MUST be large enough (dstMaxSize) to ensure compression completion even in worst case.
 * You can get the minimum value of dstMaxSize by using MLZ4F_compressBound().
 * If this condition is not respected, MLZ4F_compress() will fail (result is an errorCode).
 * MLZ4F_compressUpdate() doesn't guarantee error recovery, so you have to reset compression context when an error occurs.
 * The MLZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 * The result of the function is the number of bytes written into dstBuffer : it can be zero, meaning input data was just buffered.
 * The function outputs an error code if it fails (can be tested using MLZ4F_isError())
 */

size_t MLZ4F_flush(MLZ4F_compressionContext_t cctx, void* dstBuffer, size_t dstMaxSize, const MLZ4F_compressOptions_t* cOptPtr);
/* MLZ4F_flush()
 * Should you need to generate compressed data immediately, without waiting for the current block to be filled,
 * you can call MLZ4_flush(), which will immediately compress any remaining data buffered within cctx.
 * Note that dstMaxSize must be large enough to ensure the operation will be successful.
 * MLZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 * The result of the function is the number of bytes written into dstBuffer
 * (it can be zero, this means there was no data left within cctx)
 * The function outputs an error code if it fails (can be tested using MLZ4F_isError())
 */

size_t MLZ4F_compressEnd(MLZ4F_compressionContext_t cctx, void* dstBuffer, size_t dstMaxSize, const MLZ4F_compressOptions_t* cOptPtr);
/* MLZ4F_compressEnd()
 * When you want to properly finish the compressed frame, just call MLZ4F_compressEnd().
 * It will flush whatever data remained within compressionContext (like MLZ4_flush())
 * but also properly finalize the frame, with an endMark and a checksum.
 * The result of the function is the number of bytes written into dstBuffer (necessarily >= 4 (endMark), or 8 if optional frame checksum is enabled)
 * The function outputs an error code if it fails (can be tested using MLZ4F_isError())
 * The MLZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 * A successful call to MLZ4F_compressEnd() makes cctx available again for next compression task.
 */


/***********************************
*  Decompression functions
***********************************/

typedef struct MLZ4F_dctx_s* MLZ4F_decompressionContext_t;   /* must be aligned on 8-bytes */

typedef struct {
  unsigned stableDst;       /* guarantee that decompressed data will still be there on next function calls (avoid storage into tmp buffers) */
  unsigned reserved[3];
} MLZ4F_decompressOptions_t;


/* Resource management */

MLZ4F_errorCode_t MLZ4F_createDecompressionContext(MLZ4F_decompressionContext_t* dctxPtr, unsigned version);
MLZ4F_errorCode_t MLZ4F_freeDecompressionContext(MLZ4F_decompressionContext_t dctx);
/* MLZ4F_createDecompressionContext() :
 * The first thing to do is to create an MLZ4F_decompressionContext_t object, which will be used in all decompression operations.
 * This is achieved using MLZ4F_createDecompressionContext().
 * The version provided MUST be MLZ4F_VERSION. It is intended to track potential breaking differences between different versions.
 * The function will provide a pointer to a fully allocated and initialized MLZ4F_decompressionContext_t object.
 * The result is an errorCode, which can be tested using MLZ4F_isError().
 * dctx memory can be released using MLZ4F_freeDecompressionContext();
 * The result of MLZ4F_freeDecompressionContext() is indicative of the current state of decompressionContext when being released.
 * That is, it should be == 0 if decompression has been completed fully and correctly.
 */


/* Decompression */

size_t MLZ4F_getFrameInfo(MLZ4F_decompressionContext_t dctx,
                         MLZ4F_frameInfo_t* frameInfoPtr,
                         const void* srcBuffer, size_t* srcSizePtr);
/* MLZ4F_getFrameInfo()
 * This function decodes frame header information (such as max blockSize, frame checksum, etc.).
 * Its usage is optional : you can start by calling directly MLZ4F_decompress() instead.
 * The objective is to extract frame header information, typically for allocation purposes.
 * MLZ4F_getFrameInfo() can also be used anytime *after* starting decompression, on any valid MLZ4F_decompressionContext_t.
 * The result is *copied* into an existing MLZ4F_frameInfo_t structure which must be already allocated.
 * The number of bytes read from srcBuffer will be provided within *srcSizePtr (necessarily <= original value).
 * The function result is an hint of how many srcSize bytes MLZ4F_decompress() expects for next call,
 *                        or an error code which can be tested using MLZ4F_isError()
 *                        (typically, when there is not enough src bytes to fully decode the frame header)
 * You are expected to resume decompression from where it stopped (srcBuffer + *srcSizePtr)
 */

size_t MLZ4F_decompress(MLZ4F_decompressionContext_t dctx,
                       void* dstBuffer, size_t* dstSizePtr,
                       const void* srcBuffer, size_t* srcSizePtr,
                       const MLZ4F_decompressOptions_t* dOptPtr);
/* MLZ4F_decompress()
 * Call this function repetitively to regenerate data compressed within srcBuffer.
 * The function will attempt to decode *srcSizePtr bytes from srcBuffer, into dstBuffer of maximum size *dstSizePtr.
 *
 * The number of bytes regenerated into dstBuffer will be provided within *dstSizePtr (necessarily <= original value).
 *
 * The number of bytes read from srcBuffer will be provided within *srcSizePtr (necessarily <= original value).
 * If number of bytes read is < number of bytes provided, then decompression operation is not completed.
 * It typically happens when dstBuffer is not large enough to contain all decoded data.
 * MLZ4F_decompress() must be called again, starting from where it stopped (srcBuffer + *srcSizePtr)
 * The function will check this condition, and refuse to continue if it is not respected.
 *
 * dstBuffer is supposed to be flushed between each call to the function, since its content will be overwritten.
 * dst arguments can be changed at will with each consecutive call to the function.
 *
 * The function result is an hint of how many srcSize bytes MLZ4F_decompress() expects for next call.
 * Schematically, it's the size of the current (or remaining) compressed block + header of next block.
 * Respecting the hint provides some boost to performance, since it does skip intermediate buffers.
 * This is just a hint, you can always provide any srcSize you want.
 * When a frame is fully decoded, the function result will be 0 (no more data expected).
 * If decompression failed, function result is an error code, which can be tested using MLZ4F_isError().
 *
 * After a frame is fully decoded, dctx can be used again to decompress another frame.
 */


//#if defined (__cplusplus)
//}
//#endif