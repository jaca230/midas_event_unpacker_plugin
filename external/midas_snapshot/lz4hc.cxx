/*
    LZ4 HC - High Compression Mode of LZ4
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



/**************************************
*  Tuning Parameter
**************************************/
static const int LZ4HC_compressionLevel_default = 9;


/**************************************
*  Includes
**************************************/
#include "mlz4hc.h"


/**************************************
*  Local Compiler Options
**************************************/
#if defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if defined (__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#endif


/**************************************
*  Common LZ4 definition
**************************************/
#define MLZ4_COMMONDEFS_ONLY
#include "lz4.cxx"


/**************************************
*  Local Constants
**************************************/
#define DICTIONARY_LOGSIZE 16
#define MAXD (1<<DICTIONARY_LOGSIZE)
#define MAXD_MASK (MAXD - 1)

#define HASH_LOG (DICTIONARY_LOGSIZE-1)
#define HASHTABLESIZE (1 << HASH_LOG)
#define HASH_MASK (HASHTABLESIZE - 1)

#define OPTIMAL_ML (int)((ML_MASK-1)+MINMATCH)

static const int g_maxCompressionLevel = 16;


/**************************************
*  Local Types
**************************************/
typedef struct
{
    U32   hashTable[HASHTABLESIZE];
    U16   chainTable[MAXD];
    const BYTE* end;        /* next block here to continue on current prefix */
    const BYTE* base;       /* All index relative to this position */
    const BYTE* dictBase;   /* alternate base for extDict */
    BYTE* inputBuffer;      /* deprecated */
    U32   dictLimit;        /* below that point, need extDict */
    U32   lowLimit;         /* below that point, no more dict */
    U32   nextToUpdate;     /* index from which to continue dictionary update */
    U32   compressionLevel;
} LZ4HC_Data_Structure;


/**************************************
*  Local Macros
**************************************/
#define HASH_FUNCTION(i)       (((i) * 2654435761U) >> ((MINMATCH*8)-HASH_LOG))
//#define DELTANEXTU16(p)        chainTable[(p) & MAXD_MASK]   /* flexible, MAXD dependent */
#define DELTANEXTU16(p)        chainTable[(U16)(p)]   /* faster */

static U32 LZ4HC_hashPtr(const void* ptr) { return HASH_FUNCTION(MLZ4_read32(ptr)); }



/**************************************
*  HC Compression
**************************************/
static void LZ4HC_init (LZ4HC_Data_Structure* hc4, const BYTE* start)
{
    MEM_INIT((void*)hc4->hashTable, 0, sizeof(hc4->hashTable));
    MEM_INIT(hc4->chainTable, 0xFF, sizeof(hc4->chainTable));
    hc4->nextToUpdate = 64 KB;
    hc4->base = start - 64 KB;
    hc4->end = start;
    hc4->dictBase = start - 64 KB;
    hc4->dictLimit = 64 KB;
    hc4->lowLimit = 64 KB;
}


/* Update chains up to ip (excluded) */
FORCE_INLINE void LZ4HC_Insert (LZ4HC_Data_Structure* hc4, const BYTE* ip)
{
    U16* chainTable = hc4->chainTable;
    U32* HashTable  = hc4->hashTable;
    const BYTE* const base = hc4->base;
    const U32 target = (U32)(ip - base);
    U32 idx = hc4->nextToUpdate;

    while(idx < target)
    {
        U32 h = LZ4HC_hashPtr(base+idx);
        size_t delta = idx - HashTable[h];
        if (delta>MAX_DISTANCE) delta = MAX_DISTANCE;
        DELTANEXTU16(idx) = (U16)delta;
        HashTable[h] = idx;
        idx++;
    }

    hc4->nextToUpdate = target;
}


FORCE_INLINE int LZ4HC_InsertAndFindBestMatch (LZ4HC_Data_Structure* hc4,   /* Index table will be updated */
                                               const BYTE* ip, const BYTE* const iLimit,
                                               const BYTE** matchpos,
                                               const int maxNbAttempts)
{
    U16* const chainTable = hc4->chainTable;
    U32* const HashTable = hc4->hashTable;
    const BYTE* const base = hc4->base;
    const BYTE* const dictBase = hc4->dictBase;
    const U32 dictLimit = hc4->dictLimit;
    const U32 lowLimit = (hc4->lowLimit + 64 KB > (U32)(ip-base)) ? hc4->lowLimit : (U32)(ip - base) - (64 KB - 1);
    U32 matchIndex;
    const BYTE* match;
    int nbAttempts=maxNbAttempts;
    size_t ml=0;

    /* HC4 match finder */
    LZ4HC_Insert(hc4, ip);
    matchIndex = HashTable[LZ4HC_hashPtr(ip)];

    while ((matchIndex>=lowLimit) && (nbAttempts))
    {
        nbAttempts--;
        if (matchIndex >= dictLimit)
        {
            match = base + matchIndex;
            if (*(match+ml) == *(ip+ml)
                && (MLZ4_read32(match) == MLZ4_read32(ip)))
            {
                size_t mlt = MLZ4_count(ip+MINMATCH, match+MINMATCH, iLimit) + MINMATCH;
                if (mlt > ml) { ml = mlt; *matchpos = match; }
            }
        }
        else
        {
            match = dictBase + matchIndex;
            if (MLZ4_read32(match) == MLZ4_read32(ip))
            {
                size_t mlt;
                const BYTE* vLimit = ip + (dictLimit - matchIndex);
                if (vLimit > iLimit) vLimit = iLimit;
                mlt = MLZ4_count(ip+MINMATCH, match+MINMATCH, vLimit) + MINMATCH;
                if ((ip+mlt == vLimit) && (vLimit < iLimit))
                    mlt += MLZ4_count(ip+mlt, base+dictLimit, iLimit);
                if (mlt > ml) { ml = mlt; *matchpos = base + matchIndex; }   /* virtual matchpos */
            }
        }
        matchIndex -= DELTANEXTU16(matchIndex);
    }

    return (int)ml;
}


FORCE_INLINE int LZ4HC_InsertAndGetWiderMatch (
    LZ4HC_Data_Structure* hc4,
    const BYTE* const ip,
    const BYTE* const iLowLimit,
    const BYTE* const iHighLimit,
    int longest,
    const BYTE** matchpos,
    const BYTE** startpos,
    const int maxNbAttempts)
{
    U16* const chainTable = hc4->chainTable;
    U32* const HashTable = hc4->hashTable;
    const BYTE* const base = hc4->base;
    const U32 dictLimit = hc4->dictLimit;
    const BYTE* const lowPrefixPtr = base + dictLimit;
    const U32 lowLimit = (hc4->lowLimit + 64 KB > (U32)(ip-base)) ? hc4->lowLimit : (U32)(ip - base) - (64 KB - 1);
    const BYTE* const dictBase = hc4->dictBase;
    U32   matchIndex;
    int nbAttempts = maxNbAttempts;
    int delta = (int)(ip-iLowLimit);


    /* First Match */
    LZ4HC_Insert(hc4, ip);
    matchIndex = HashTable[LZ4HC_hashPtr(ip)];

    while ((matchIndex>=lowLimit) && (nbAttempts))
    {
        nbAttempts--;
        if (matchIndex >= dictLimit)
        {
            const BYTE* matchPtr = base + matchIndex;
            if (*(iLowLimit + longest) == *(matchPtr - delta + longest))
                if (MLZ4_read32(matchPtr) == MLZ4_read32(ip))
                {
                    int mlt = MINMATCH + MLZ4_count(ip+MINMATCH, matchPtr+MINMATCH, iHighLimit);
                    int back = 0;

                    while ((ip+back>iLowLimit)
                           && (matchPtr+back > lowPrefixPtr)
                           && (ip[back-1] == matchPtr[back-1]))
                            back--;

                    mlt -= back;

                    if (mlt > longest)
                    {
                        longest = (int)mlt;
                        *matchpos = matchPtr+back;
                        *startpos = ip+back;
                    }
                }
        }
        else
        {
            const BYTE* matchPtr = dictBase + matchIndex;
            if (MLZ4_read32(matchPtr) == MLZ4_read32(ip))
            {
                size_t mlt;
                int back=0;
                const BYTE* vLimit = ip + (dictLimit - matchIndex);
                if (vLimit > iHighLimit) vLimit = iHighLimit;
                mlt = MLZ4_count(ip+MINMATCH, matchPtr+MINMATCH, vLimit) + MINMATCH;
                if ((ip+mlt == vLimit) && (vLimit < iHighLimit))
                    mlt += MLZ4_count(ip+mlt, base+dictLimit, iHighLimit);
                while ((ip+back > iLowLimit) && (matchIndex+back > lowLimit) && (ip[back-1] == matchPtr[back-1])) back--;
                mlt -= back;
                if ((int)mlt > longest) { longest = (int)mlt; *matchpos = base + matchIndex + back; *startpos = ip+back; }
            }
        }
        matchIndex -= DELTANEXTU16(matchIndex);
    }

    return longest;
}


typedef enum { noLimit = 0, limitedOutput = 1 } limitedOutput_directive;

#define LZ4HC_DEBUG 0
#if LZ4HC_DEBUG
static unsigned debug = 0;
#endif

FORCE_INLINE int LZ4HC_encodeSequence (
    const BYTE** ip,
    BYTE** op,
    const BYTE** anchor,
    int matchLength,
    const BYTE* const match,
    limitedOutput_directive limitedOutputBuffer,
    BYTE* oend)
{
    int length;
    BYTE* token;

#if LZ4HC_DEBUG
    if (debug) printf("literal : %u  --  match : %u  --  offset : %u\n", (U32)(*ip - *anchor), (U32)matchLength, (U32)(*ip-match));
#endif

    /* Encode Literal length */
    length = (int)(*ip - *anchor);
    token = (*op)++;
    if ((limitedOutputBuffer) && ((*op + (length>>8) + length + (2 + 1 + LASTLITERALS)) > oend)) return 1;   /* Check output limit */
    if (length>=(int)RUN_MASK) { int len; *token=(RUN_MASK<<ML_BITS); len = length-RUN_MASK; for(; len > 254 ; len-=255) *(*op)++ = 255;  *(*op)++ = (BYTE)len; }
    else *token = (BYTE)(length<<ML_BITS);

    /* Copy Literals */
    MLZ4_wildCopy(*op, *anchor, (*op) + length);
    *op += length;

    /* Encode Offset */
    MLZ4_writeLE16(*op, (U16)(*ip-match)); *op += 2;

    /* Encode MatchLength */
    length = (int)(matchLength-MINMATCH);
    if ((limitedOutputBuffer) && (*op + (length>>8) + (1 + LASTLITERALS) > oend)) return 1;   /* Check output limit */
    if (length>=(int)ML_MASK) { *token+=ML_MASK; length-=ML_MASK; for(; length > 509 ; length-=510) { *(*op)++ = 255; *(*op)++ = 255; } if (length > 254) { length-=255; *(*op)++ = 255; } *(*op)++ = (BYTE)length; }
    else *token += (BYTE)(length);

    /* Prepare next loop */
    *ip += matchLength;
    *anchor = *ip;

    return 0;
}


static int LZ4HC_compress_generic (
    void* ctxvoid,
    const char* source,
    char* dest,
    int inputSize,
    int maxOutputSize,
    int compressionLevel,
    limitedOutput_directive limit
    )
{
    LZ4HC_Data_Structure* ctx = (LZ4HC_Data_Structure*) ctxvoid;
    const BYTE* ip = (const BYTE*) source;
    const BYTE* anchor = ip;
    const BYTE* const iend = ip + inputSize;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = (iend - LASTLITERALS);

    BYTE* op = (BYTE*) dest;
    BYTE* const oend = op + maxOutputSize;

    unsigned maxNbAttempts;
    int   ml, ml2, ml3, ml0;
    const BYTE* ref=NULL;
    const BYTE* start2=NULL;
    const BYTE* ref2=NULL;
    const BYTE* start3=NULL;
    const BYTE* ref3=NULL;
    const BYTE* start0;
    const BYTE* ref0;


    /* init */
    if (compressionLevel > g_maxCompressionLevel) compressionLevel = g_maxCompressionLevel;
    if (compressionLevel < 1) compressionLevel = LZ4HC_compressionLevel_default;
    maxNbAttempts = 1 << (compressionLevel-1);
    ctx->end += inputSize;

    ip++;

    /* Main Loop */
    while (ip < mflimit)
    {
        ml = LZ4HC_InsertAndFindBestMatch (ctx, ip, matchlimit, (&ref), maxNbAttempts);
        if (!ml) { ip++; continue; }

        /* saved, in case we would skip too much */
        start0 = ip;
        ref0 = ref;
        ml0 = ml;

_Search2:
        if (ip+ml < mflimit)
            ml2 = LZ4HC_InsertAndGetWiderMatch(ctx, ip + ml - 2, ip + 1, matchlimit, ml, &ref2, &start2, maxNbAttempts);
        else ml2 = ml;

        if (ml2 == ml)  /* No better match */
        {
            if (LZ4HC_encodeSequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;
            continue;
        }

        if (start0 < ip)
        {
            if (start2 < ip + ml0)   /* empirical */
            {
                ip = start0;
                ref = ref0;
                ml = ml0;
            }
        }

        /* Here, start0==ip */
        if ((start2 - ip) < 3)   /* First Match too small : removed */
        {
            ml = ml2;
            ip = start2;
            ref =ref2;
            goto _Search2;
        }

_Search3:
        /*
        * Currently we have :
        * ml2 > ml1, and
        * ip1+3 <= ip2 (usually < ip1+ml1)
        */
        if ((start2 - ip) < OPTIMAL_ML)
        {
            int correction;
            int new_ml = ml;
            if (new_ml > OPTIMAL_ML) new_ml = OPTIMAL_ML;
            if (ip+new_ml > start2 + ml2 - MINMATCH) new_ml = (int)(start2 - ip) + ml2 - MINMATCH;
            correction = new_ml - (int)(start2 - ip);
            if (correction > 0)
            {
                start2 += correction;
                ref2 += correction;
                ml2 -= correction;
            }
        }
        /* Now, we have start2 = ip+new_ml, with new_ml = min(ml, OPTIMAL_ML=18) */

        if (start2 + ml2 < mflimit)
            ml3 = LZ4HC_InsertAndGetWiderMatch(ctx, start2 + ml2 - 3, start2, matchlimit, ml2, &ref3, &start3, maxNbAttempts);
        else ml3 = ml2;

        if (ml3 == ml2) /* No better match : 2 sequences to encode */
        {
            /* ip & ref are known; Now for ml */
            if (start2 < ip+ml)  ml = (int)(start2 - ip);
            /* Now, encode 2 sequences */
            if (LZ4HC_encodeSequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;
            ip = start2;
            if (LZ4HC_encodeSequence(&ip, &op, &anchor, ml2, ref2, limit, oend)) return 0;
            continue;
        }

        if (start3 < ip+ml+3) /* Not enough space for match 2 : remove it */
        {
            if (start3 >= (ip+ml)) /* can write Seq1 immediately ==> Seq2 is removed, so Seq3 becomes Seq1 */
            {
                if (start2 < ip+ml)
                {
                    int correction = (int)(ip+ml - start2);
                    start2 += correction;
                    ref2 += correction;
                    ml2 -= correction;
                    if (ml2 < MINMATCH)
                    {
                        start2 = start3;
                        ref2 = ref3;
                        ml2 = ml3;
                    }
                }

                if (LZ4HC_encodeSequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;
                ip  = start3;
                ref = ref3;
                ml  = ml3;

                start0 = start2;
                ref0 = ref2;
                ml0 = ml2;
                goto _Search2;
            }

            start2 = start3;
            ref2 = ref3;
            ml2 = ml3;
            goto _Search3;
        }

        /*
        * OK, now we have 3 ascending matches; let's write at least the first one
        * ip & ref are known; Now for ml
        */
        if (start2 < ip+ml)
        {
            if ((start2 - ip) < (int)ML_MASK)
            {
                int correction;
                if (ml > OPTIMAL_ML) ml = OPTIMAL_ML;
                if (ip + ml > start2 + ml2 - MINMATCH) ml = (int)(start2 - ip) + ml2 - MINMATCH;
                correction = ml - (int)(start2 - ip);
                if (correction > 0)
                {
                    start2 += correction;
                    ref2 += correction;
                    ml2 -= correction;
                }
            }
            else
            {
                ml = (int)(start2 - ip);
            }
        }
        if (LZ4HC_encodeSequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;

        ip = start2;
        ref = ref2;
        ml = ml2;

        start2 = start3;
        ref2 = ref3;
        ml2 = ml3;

        goto _Search3;
    }

    /* Encode Last Literals */
    {
        int lastRun = (int)(iend - anchor);
        if ((limit) && (((char*)op - dest) + lastRun + 1 + ((lastRun+255-RUN_MASK)/255) > (U32)maxOutputSize)) return 0;  /* Check output limit */
        if (lastRun>=(int)RUN_MASK) { *op++=(RUN_MASK<<ML_BITS); lastRun-=RUN_MASK; for(; lastRun > 254 ; lastRun-=255) *op++ = 255; *op++ = (BYTE) lastRun; }
        else *op++ = (BYTE)(lastRun<<ML_BITS);
        memcpy(op, anchor, iend - anchor);
        op += iend-anchor;
    }

    /* End */
    return (int) (((char*)op)-dest);
}


int MLZ4_sizeofStateHC(void) { return sizeof(LZ4HC_Data_Structure); }

int MLZ4_compress_HC_extStateHC (void* state, const char* src, char* dst, int srcSize, int maxDstSize, int compressionLevel)
{
    if (((size_t)(state)&(sizeof(void*)-1)) != 0) return 0;   /* Error : state is not aligned for pointers (32 or 64 bits) */
    LZ4HC_init ((LZ4HC_Data_Structure*)state, (const BYTE*)src);
    if (maxDstSize < MLZ4_compressBound(srcSize))
        return LZ4HC_compress_generic (state, src, dst, srcSize, maxDstSize, compressionLevel, limitedOutput);
    else
        return LZ4HC_compress_generic (state, src, dst, srcSize, maxDstSize, compressionLevel, noLimit);
}

int MLZ4_compress_HC(const char* src, char* dst, int srcSize, int maxDstSize, int compressionLevel)
{
    LZ4HC_Data_Structure state;
    return MLZ4_compress_HC_extStateHC(&state, src, dst, srcSize, maxDstSize, compressionLevel);
}



/**************************************
*  Streaming Functions
**************************************/
/* allocation */
MLZ4_streamHC_t* MLZ4_createStreamHC(void) { return (MLZ4_streamHC_t*)malloc(sizeof(MLZ4_streamHC_t)); }
int             MLZ4_freeStreamHC (MLZ4_streamHC_t* MLZ4_streamHCPtr) { free(MLZ4_streamHCPtr); return 0; }


/* initialization */
void MLZ4_resetStreamHC (MLZ4_streamHC_t* MLZ4_streamHCPtr, int compressionLevel)
{
    MLZ4_STATIC_ASSERT(sizeof(LZ4HC_Data_Structure) <= sizeof(MLZ4_streamHC_t));   /* if compilation fails here, MLZ4_STREAMHCSIZE must be increased */
    ((LZ4HC_Data_Structure*)MLZ4_streamHCPtr)->base = NULL;
    ((LZ4HC_Data_Structure*)MLZ4_streamHCPtr)->compressionLevel = (unsigned)compressionLevel;
}

int MLZ4_loadDictHC (MLZ4_streamHC_t* MLZ4_streamHCPtr, const char* dictionary, int dictSize)
{
    LZ4HC_Data_Structure* ctxPtr = (LZ4HC_Data_Structure*) MLZ4_streamHCPtr;
    if (dictSize > 64 KB)
    {
        dictionary += dictSize - 64 KB;
        dictSize = 64 KB;
    }
    LZ4HC_init (ctxPtr, (const BYTE*)dictionary);
    if (dictSize >= 4) LZ4HC_Insert (ctxPtr, (const BYTE*)dictionary +(dictSize-3));
    ctxPtr->end = (const BYTE*)dictionary + dictSize;
    return dictSize;
}


/* compression */

static void LZ4HC_setExternalDict(LZ4HC_Data_Structure* ctxPtr, const BYTE* newBlock)
{
    if (ctxPtr->end >= ctxPtr->base + 4)
        LZ4HC_Insert (ctxPtr, ctxPtr->end-3);   /* Referencing remaining dictionary content */
    /* Only one memory segment for extDict, so any previous extDict is lost at this stage */
    ctxPtr->lowLimit  = ctxPtr->dictLimit;
    ctxPtr->dictLimit = (U32)(ctxPtr->end - ctxPtr->base);
    ctxPtr->dictBase  = ctxPtr->base;
    ctxPtr->base = newBlock - ctxPtr->dictLimit;
    ctxPtr->end  = newBlock;
    ctxPtr->nextToUpdate = ctxPtr->dictLimit;   /* match referencing will resume from there */
}

static int MLZ4_compressHC_continue_generic (LZ4HC_Data_Structure* ctxPtr,
                                            const char* source, char* dest,
                                            int inputSize, int maxOutputSize, limitedOutput_directive limit)
{
    /* auto-init if forgotten */
    if (ctxPtr->base == NULL)
        LZ4HC_init (ctxPtr, (const BYTE*) source);

    /* Check overflow */
    if ((size_t)(ctxPtr->end - ctxPtr->base) > 2 GB)
    {
        size_t dictSize = (size_t)(ctxPtr->end - ctxPtr->base) - ctxPtr->dictLimit;
        if (dictSize > 64 KB) dictSize = 64 KB;

        MLZ4_loadDictHC((MLZ4_streamHC_t*)ctxPtr, (const char*)(ctxPtr->end) - dictSize, (int)dictSize);
    }

    /* Check if blocks follow each other */
    if ((const BYTE*)source != ctxPtr->end)
        LZ4HC_setExternalDict(ctxPtr, (const BYTE*)source);

    /* Check overlapping input/dictionary space */
    {
        const BYTE* sourceEnd = (const BYTE*) source + inputSize;
        const BYTE* dictBegin = ctxPtr->dictBase + ctxPtr->lowLimit;
        const BYTE* dictEnd   = ctxPtr->dictBase + ctxPtr->dictLimit;
        if ((sourceEnd > dictBegin) && ((const BYTE*)source < dictEnd))
        {
            if (sourceEnd > dictEnd) sourceEnd = dictEnd;
            ctxPtr->lowLimit = (U32)(sourceEnd - ctxPtr->dictBase);
            if (ctxPtr->dictLimit - ctxPtr->lowLimit < 4) ctxPtr->lowLimit = ctxPtr->dictLimit;
        }
    }

    return LZ4HC_compress_generic (ctxPtr, source, dest, inputSize, maxOutputSize, ctxPtr->compressionLevel, limit);
}

int MLZ4_compress_HC_continue (MLZ4_streamHC_t* MLZ4_streamHCPtr, const char* source, char* dest, int inputSize, int maxOutputSize)
{
    if (maxOutputSize < MLZ4_compressBound(inputSize))
        return MLZ4_compressHC_continue_generic ((LZ4HC_Data_Structure*)MLZ4_streamHCPtr, source, dest, inputSize, maxOutputSize, limitedOutput);
    else
        return MLZ4_compressHC_continue_generic ((LZ4HC_Data_Structure*)MLZ4_streamHCPtr, source, dest, inputSize, maxOutputSize, noLimit);
}


/* dictionary saving */

int MLZ4_saveDictHC (MLZ4_streamHC_t* MLZ4_streamHCPtr, char* safeBuffer, int dictSize)
{
    LZ4HC_Data_Structure* streamPtr = (LZ4HC_Data_Structure*)MLZ4_streamHCPtr;
    int prefixSize = (int)(streamPtr->end - (streamPtr->base + streamPtr->dictLimit));
    if (dictSize > 64 KB) dictSize = 64 KB;
    if (dictSize < 4) dictSize = 0;
    if (dictSize > prefixSize) dictSize = prefixSize;
    memmove(safeBuffer, streamPtr->end - dictSize, dictSize);
    {
        U32 endIndex = (U32)(streamPtr->end - streamPtr->base);
        streamPtr->end = (const BYTE*)safeBuffer + dictSize;
        streamPtr->base = streamPtr->end - endIndex;
        streamPtr->dictLimit = endIndex - dictSize;
        streamPtr->lowLimit = endIndex - dictSize;
        if (streamPtr->nextToUpdate < streamPtr->dictLimit) streamPtr->nextToUpdate = streamPtr->dictLimit;
    }
    return dictSize;
}


/***********************************
*  Deprecated Functions
***********************************/
/* Deprecated compression functions */
/* These functions are planned to start generate warnings by r131 approximately */
int MLZ4_compressHC(const char* src, char* dst, int srcSize) { return MLZ4_compress_HC (src, dst, srcSize, MLZ4_compressBound(srcSize), 0); }
int MLZ4_compressHC_limitedOutput(const char* src, char* dst, int srcSize, int maxDstSize) { return MLZ4_compress_HC(src, dst, srcSize, maxDstSize, 0); }
int MLZ4_compressHC2(const char* src, char* dst, int srcSize, int cLevel) { return MLZ4_compress_HC (src, dst, srcSize, MLZ4_compressBound(srcSize), cLevel); }
int MLZ4_compressHC2_limitedOutput(const char* src, char* dst, int srcSize, int maxDstSize, int cLevel) { return MLZ4_compress_HC(src, dst, srcSize, maxDstSize, cLevel); }
int MLZ4_compressHC_withStateHC (void* state, const char* src, char* dst, int srcSize) { return MLZ4_compress_HC_extStateHC (state, src, dst, srcSize, MLZ4_compressBound(srcSize), 0); }
int MLZ4_compressHC_limitedOutput_withStateHC (void* state, const char* src, char* dst, int srcSize, int maxDstSize) { return MLZ4_compress_HC_extStateHC (state, src, dst, srcSize, maxDstSize, 0); }
int MLZ4_compressHC2_withStateHC (void* state, const char* src, char* dst, int srcSize, int cLevel) { return MLZ4_compress_HC_extStateHC(state, src, dst, srcSize, MLZ4_compressBound(srcSize), cLevel); }
int MLZ4_compressHC2_limitedOutput_withStateHC (void* state, const char* src, char* dst, int srcSize, int maxDstSize, int cLevel) { return MLZ4_compress_HC_extStateHC(state, src, dst, srcSize, maxDstSize, cLevel); }
int MLZ4_compressHC_continue (MLZ4_streamHC_t* ctx, const char* src, char* dst, int srcSize) { return MLZ4_compress_HC_continue (ctx, src, dst, srcSize, MLZ4_compressBound(srcSize)); }
int MLZ4_compressHC_limitedOutput_continue (MLZ4_streamHC_t* ctx, const char* src, char* dst, int srcSize, int maxDstSize) { return MLZ4_compress_HC_continue (ctx, src, dst, srcSize, maxDstSize); }


/* Deprecated streaming functions */
/* These functions currently generate deprecation warnings */
int MLZ4_sizeofStreamStateHC(void) { return MLZ4_STREAMHCSIZE; }

int MLZ4_resetStreamStateHC(void* state, char* inputBuffer)
{
    if ((((size_t)state) & (sizeof(void*)-1)) != 0) return 1;   /* Error : pointer is not aligned for pointer (32 or 64 bits) */
    LZ4HC_init((LZ4HC_Data_Structure*)state, (const BYTE*)inputBuffer);
    ((LZ4HC_Data_Structure*)state)->inputBuffer = (BYTE*)inputBuffer;
    return 0;
}

void* MLZ4_createHC (char* inputBuffer)
{
    void* hc4 = ALLOCATOR(1, sizeof(LZ4HC_Data_Structure));
    if (hc4 == NULL) return NULL;   /* not enough memory */
    LZ4HC_init ((LZ4HC_Data_Structure*)hc4, (const BYTE*)inputBuffer);
    ((LZ4HC_Data_Structure*)hc4)->inputBuffer = (BYTE*)inputBuffer;
    return hc4;
}

int MLZ4_freeHC (void* LZ4HC_Data)
{
    FREEMEM(LZ4HC_Data);
    return (0);
}

int MLZ4_compressHC2_continue (void* LZ4HC_Data, const char* source, char* dest, int inputSize, int compressionLevel)
{
    return LZ4HC_compress_generic (LZ4HC_Data, source, dest, inputSize, 0, compressionLevel, noLimit);
}

int MLZ4_compressHC2_limitedOutput_continue (void* LZ4HC_Data, const char* source, char* dest, int inputSize, int maxOutputSize, int compressionLevel)
{
    return LZ4HC_compress_generic (LZ4HC_Data, source, dest, inputSize, maxOutputSize, compressionLevel, limitedOutput);
}

char* MLZ4_slideInputBufferHC(void* LZ4HC_Data)
{
    LZ4HC_Data_Structure* hc4 = (LZ4HC_Data_Structure*)LZ4HC_Data;
    int dictSize = MLZ4_saveDictHC((MLZ4_streamHC_t*)LZ4HC_Data, (char*)(hc4->inputBuffer), 64 KB);
    return (char*)(hc4->inputBuffer + dictSize);
}