// midasio.cxx

#include "midasio.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h> // malloc()
#include <string.h> // memcpy()
#include <errno.h>  // errno
#include <signal.h> // signal()
#include <ctime>

#include <string>

#undef NDEBUG // working assert() is required by this program. K.O.

bool TMWriterInterface::fgTrace = false;
bool TMReaderInterface::fgTrace = false;

TMReaderInterface::TMReaderInterface() // ctor
{
   if (TMReaderInterface::fgTrace)
      printf("TMReaderInterface::ctor!\n");
   fError = false;
   fErrorString = "";
}

static std::string to_string(int v)
{
   char buf[256];
   snprintf(buf, sizeof(buf), "%d", v);
   return buf;
}

static std::string Errno(const char* s)
{
   std::string r;
   r += s;
   r += " failed: errno: ";
   r += to_string(errno);
   r += " (";
   r += strerror(errno);
   r += ")";
   return r;
}

static int ReadPipe(FILE *fp, char* buf, int length)
{
  int count = 0;
  while (length > 0) {
     int rd = fread(buf, 1, length, fp);
     if (rd < 0)
        return rd;
     if (rd == 0)
        return count;

     buf += rd;
     length -= rd;
     count += rd;
  }
  return count;
}


class ErrorReader: public TMReaderInterface
{
 public:
   ErrorReader(const char* filename)
   {
      if (TMReaderInterface::fgTrace)
         printf("ErrorReader::ctor!\n");
      fError = true;
      fErrorString = "The ErrorReader always returns an error";
   }

   ~ErrorReader() // dtor
   {
      if (TMReaderInterface::fgTrace)
         printf("ErrorReader::dtor!\n");
   }

   int Read(void* buf, int count)
   {
      return -1;
   }

   int Close()
   {
      // an exception, no error on close.
      return 0;
   }
};

class FileReader: public TMReaderInterface
{
 public:
   FileReader(const char* filename)
   {
      if (TMReaderInterface::fgTrace)
         printf("FileReader::ctor!\n");
      fFilename = filename;
      fFp = fopen(filename, "r");
      if (!fFp) {
         fError = true;
         fErrorString = Errno((std::string("fopen(\"")+filename+"\")").c_str());
      }
   }

   ~FileReader() // dtor
   {
      if (TMReaderInterface::fgTrace)
         printf("FileReader::dtor!\n");
      if (fFp)
         Close();
   }

   int Read(void* buf, int count)
   {
      if (fError)
         return -1;
      assert(fFp != NULL);
      int rd = ReadPipe(fFp, (char*)buf, count);
      if (rd < 0) {
         fError = true;
         fErrorString = Errno((std::string("fread(\"")+fFilename+"\")").c_str());
      }
      return rd;
   }

   int Close()
   {
      if (TMReaderInterface::fgTrace)
         printf("FileReader::Close!\n");
      if (fFp) {
         fclose(fFp);
         fFp = NULL;
      }
      return 0;
   }

   std::string fFilename;
   FILE* fFp;
};

class PipeReader: public TMReaderInterface
{
 public:
   PipeReader(const char* pipename)
   {
      if (TMReaderInterface::fgTrace)
         printf("PipeReader::ctor!\n");
      fPipename = pipename;
      fPipe = popen(pipename, "r");
      if (!fPipe) {
         fError = true;
         fErrorString = Errno((std::string("popen(\"")+pipename+"\")").c_str());
      }
   }

   ~PipeReader() // dtor
   {
      if (TMReaderInterface::fgTrace)
         printf("PipeReader::dtor!\n");
      if (fPipe)
         Close();
   }

   int Read(void* buf, int count)
   {
      if (fError)
         return -1;
      assert(fPipe != NULL);
      int rd = ReadPipe(fPipe, (char*)buf, count);
      if (rd < 0) {
         fError = true;
         fErrorString = Errno((std::string("fread(\"")+fPipename+"\")").c_str());
      }
      return rd;
   }

   int Close()
   {
      if (TMReaderInterface::fgTrace)
         printf("PipeReader::Close!\n");
      if (fPipe) {
         pclose(fPipe); // FIXME: check error // only need to check error if writing to pipe
         fPipe = NULL;
      }
      return 0;
   }

   std::string fPipename;
   FILE* fPipe;
};

#include <zlib.h>

class ZlibReader: public TMReaderInterface
{
 public:
   ZlibReader(const char* filename)
   {
      if (TMReaderInterface::fgTrace)
         printf("ZlibReader::ctor!\n");
      fFilename = filename;
      fGzFile = gzopen(filename, "rb");
      if (!fGzFile) {
         fError = true;
         fErrorString = Errno((std::string("gzopen(\"")+fFilename+"\")").c_str());
      }
   }

   ~ZlibReader() // dtor
   {
      if (TMReaderInterface::fgTrace)
         printf("PipeReader::dtor!\n");
      if (fGzFile)
         Close();
   }

   int Read(void* buf, int count)
   {
      if (fError)
         return -1;
      assert(fGzFile != NULL);
      int rd = gzread(fGzFile, buf, count);
      if (rd < 0) {
         fError = true;
         fErrorString = Errno((std::string("gzread(\"")+fFilename+"\")").c_str());
      }
      return rd;
   }

   int Close()
   {
      if (TMReaderInterface::fgTrace)
         printf("ZlibReader::Close!\n");
      if (fGzFile) {
         gzclose(fGzFile); // FIXME: must check error on write, ok no check on read
         fGzFile = NULL;
      }
      return 0;
   }

   std::string fFilename;
   gzFile fGzFile;
};

#include "mlz4frame.h"

static std::string Lz4Error(int errorCode)
{
   std::string s;
   s += to_string(errorCode);
   s += " (";
   s += MLZ4F_getErrorName(errorCode);
   s += ")";
   return s;
}

class Lz4Reader: public TMReaderInterface
{
public:
   Lz4Reader(TMReaderInterface* reader)
   {
      assert(reader);
      // If there is an error opening the file, the decompression will
      // not do anything (not even report an error)
      if (reader->fError)
      {
         fError = reader->fError;
         fErrorString = reader->fErrorString;
      }
      fReader = reader;

      if (TMReaderInterface::fgTrace)
         printf("Lz4Reader::ctor!\n");

      MLZ4F_errorCode_t errorCode = MLZ4F_createDecompressionContext(&fContext, MLZ4F_VERSION);
      if (MLZ4F_isError(errorCode)) {
         fError = true;
         fErrorString = "MLZ4F_createDecompressionContext() error ";
         fErrorString += Lz4Error(errorCode);
      }

      fSrcBuf = NULL;
      fSrcBufSize  = 0;
      fSrcBufStart = 0;
      fSrcBufHave  = 0;

      AllocSrcBuf(1024*1024);
   }

   ~Lz4Reader() {
      if (TMReaderInterface::fgTrace)
         printf("Lz4Reader::dtor!\n");

      if (fSrcBuf) {
         free(fSrcBuf);
         fSrcBuf = NULL;
      }

      MLZ4F_errorCode_t errorCode = MLZ4F_freeDecompressionContext(fContext);
      if (MLZ4F_isError(errorCode)) {
         fError = true;
         fErrorString = "MLZ4F_freeDecompressionContext() error ";
         fErrorString += Lz4Error(errorCode);
         // NB: this error cannot be reported: we are in the destructor, fErrorString
         // is immediately destroyed...
      }

      if (fReader) {
         delete fReader;
         fReader = NULL;
      }
   }


   int Read(void* buf, int count)
   {
      if (fError)
         return -1;

      //printf("Lz4Reader::Read %d bytes!\n", count);

      char* cptr = (char*)buf;
      int clen = 0;

      while (clen < count) {
         int more = count - clen;

         if (fSrcBufHave == 0) {
            assert(fSrcBufStart == 0);

            int rd = fReader->Read(fSrcBuf, fSrcBufSize);
               
            //printf("read asked %d, got %d\n", to_read, rd);
            
            if (rd < 0) {
               if (clen > 0)
                  return clen;
               else
                  return rd;
            } else if (rd == 0) {
               return clen;
            }
               
            fSrcBufHave += rd;
            
            //printf("Lz4Reader::ReadMore: rd %d, srcbuf start %d, have %d\n", rd, fSrcBufStart, fSrcBufHave);
         }
         
         MLZ4F_decompressOptions_t* dOptPtr = NULL;

         char*  dst = cptr;
         size_t dst_size = more;
         size_t src_size = fSrcBufHave;

         size_t status = MLZ4F_decompress(fContext, dst, &dst_size, fSrcBuf + fSrcBufStart, &src_size, dOptPtr);

         if (MLZ4F_isError(status)) {
            fError = true;
            fErrorString = "MLZ4F_decompress() error ";
            fErrorString += Lz4Error(status);
            return -1;
         }

         //printf("LZ4Reader::Decompress: status %d, dst_size %d -> %d, src_size %d -> %d\n", (int)status, more, (int)dst_size, fSrcBufHave, (int)src_size);

         assert(dst_size!=0 || src_size!=0); // make sure we make progress
         
         clen += dst_size;
         cptr += dst_size;
         
         fSrcBufStart += src_size;
         fSrcBufHave  -= src_size;

         if (fSrcBufHave == 0)
            fSrcBufStart = 0;
      }
      
      //printf("Lz4Reader::Read %d bytes, returning %d bytes!\n", count, clen);
      return clen;
   }
   
   int Close()
   {
      if (TMReaderInterface::fgTrace)
         printf("Lz4Reader::Close!\n");
      return fReader->Close();
   }

   void AllocSrcBuf(int size)
   {
      //printf("Lz4Reader::AllocSrcBuffer %d -> %d bytes!\n", fSrcBufSize, size);
      fSrcBuf = (char*) realloc(fSrcBuf, size);
      assert(fSrcBuf != NULL);
      fSrcBufSize = size;
   }
   
   TMReaderInterface *fReader;
   MLZ4F_decompressionContext_t fContext;

   int fSrcBufSize;
   int fSrcBufStart;
   int fSrcBufHave;
   char* fSrcBuf;
};

class FileWriter: public TMWriterInterface
{
 public:
   FileWriter(const char* filename)
   {
      fFilename = filename;
      fFp = fopen(filename, "w");
      assert(fFp != NULL); // FIXME: check for error
   }

   int Write(const void* buf, int count)
   {
      assert(fFp != NULL);
      return fwrite(buf, 1, count, fFp);
   }

   int Close()
   {
      assert(fFp != NULL);
      fclose(fFp);
      fFp = NULL;
      return 0;
   }

   std::string fFilename;
   FILE* fFp;
};

static int hasSuffix(const char*name,const char*suffix)
{
   const char* s = strstr(name,suffix);
   if (s == NULL)
      return 0;
   
   return (s-name)+strlen(suffix) == strlen(name);
}

TMReaderInterface* TMNewReader(const char* source)
{
#ifdef SIGPIPE
   signal(SIGPIPE, SIG_IGN); // crash if reading from closed pipe
#endif
#ifdef SIGXFSZ
   signal(SIGXFSZ, SIG_IGN); // crash if reading from file >2GB without O_LARGEFILE
#endif

   if (0)
      {
      }
   else if (strncmp(source, "ssh://", 6) == 0)
      {
         const char* name = source + 6;
         const char* s = strstr(name, "/");
         
         if (s == NULL) {
            ErrorReader *e = new ErrorReader(source);
            e->fError = true;
            e->fErrorString = "TMidasFile::Open: Invalid ssh:// URI. Should be: ssh://user@host/file/path/...";
            return e;
         }
         
         const char* remoteFile = s + 1;
         
         std::string remoteHost;
         for (s=name; *s != '/'; s++)
            remoteHost += *s;

         std::string pipe;

         pipe = "ssh -e none -T -x -n ";
         pipe += remoteHost;
         pipe += " dd if=";
         pipe += remoteFile;
         pipe += " bs=1024k";
         
         if (hasSuffix(remoteFile,".gz"))
            pipe += " | gzip -dc";
         else if (hasSuffix(remoteFile,".bz2"))
            pipe += " | bzip2 -dc";
         else if (hasSuffix(remoteFile,".lz4"))
            pipe += " | lz4 -d";

         return new PipeReader(pipe.c_str());
      }
   else if (strncmp(source, "dccp://", 7) == 0)
      {
         const char* name = source + 7;

         std::string pipe;

         pipe = "dccp ";
         pipe += name;
         pipe += " /dev/fd/1";
         
         if (hasSuffix(source,".gz"))
            pipe += " | gzip -dc";
         else if (hasSuffix(source,".bz2"))
            pipe += " | bzip2 -dc";
         else if (hasSuffix(source,".lz4"))
            pipe += " | lz4 -d";

         return new PipeReader(pipe.c_str());
      }
   else if (strncmp(source, "pipein://", 9) == 0)
      {
         std::string pipe = source + 9;
         return new PipeReader(pipe.c_str());
      }
#if 0 // read compressed files using the zlib library
   else if (hasSuffix(source, ".gz"))
      {
         pipe = "gzip -dc ";
         pipe += source;
      }
#endif
   else if (hasSuffix(source, ".gz"))
      {
         return new ZlibReader(source);
      }
   else if (hasSuffix(source, ".bz2"))
      {
         return new PipeReader((std::string("bzip2 -dc ") + source).c_str());
      }
   else if (hasSuffix(source, ".lz4"))
      {
         return new Lz4Reader(new FileReader(source));
      }
   else
      {
         return new FileReader(source);
      }
}

TMWriterInterface* TMNewWriter(const char* destination)
{
   if (0) {
#if 0
   } else if (hasSuffix(source, ".gz")) {
      return new ZlibReader(source);
   } else if (hasSuffix(source, ".bz2")) {
      return new PipeReader((std::string("bzip2 -dc ") + source).c_str());
   } else if (hasSuffix(source, ".lz4")) {
      return new Lz4Reader(new FileReader(source));
#endif
   } else {
      return new FileWriter(destination);
   }
}

static uint16_t GetU16(const void*ptr)
{
   return *(uint16_t*)ptr;
}

static uint32_t GetU32(const void*ptr)
{
   return *(uint32_t*)ptr;
}

static void PutU16(char* ptr, uint16_t v)
{
   *(uint16_t*)ptr = v;
}

static void PutU32(char* ptr, uint32_t v)
{
   *(uint32_t*)ptr = v;
}

static size_t Align8(size_t size)
{
   // align to 8 bytes (uint64_t)
   return (size + 7) & ~7;
}

TMEvent* TMReadEvent(TMReaderInterface* reader)
{
   bool gOnce = true;
   if (gOnce) {
      gOnce = false;
      assert(sizeof(char)==1);
      assert(sizeof(uint16_t)==2);
      assert(sizeof(uint32_t)==4);
   }
   
   TMEvent* e = new TMEvent;

   e->Reset();

   const int event_header_size = 4*4;
   char event_header[event_header_size];

   int rd = reader->Read(event_header, event_header_size);

   if (rd < 0) { // read error
      delete e;
      return NULL;
   } else if (rd == 0) { // end of file
      delete e;
      return NULL;
   } else if (rd != event_header_size) { // truncated data in file
      fprintf(stderr, "TMReadEvent: error: read %d shorter than event header size %d\n", (int)rd, (int)event_header_size);
      e->error = true;
      return e;
   }

   e->event_id      = GetU16(event_header+0);
   e->trigger_mask  = GetU16(event_header+2);
   e->serial_number = GetU32(event_header+4);
   e->time_stamp    = GetU32(event_header+8);
   e->data_size     = GetU32(event_header+12);

   e->event_header_size = event_header_size;

   e->bank_header_flags = 0;

   //if (!e->old_event.IsGoodSize()) { // invalid event size
   //   e->error = true;
   //   return e;
   //}

   size_t to_read = e->data_size;

   e->data.resize(event_header_size + to_read);

   memcpy(&e->data[0], event_header, event_header_size);

   rd = reader->Read(&e->data[event_header_size], to_read);

   if (rd < 0) { // read error
      delete e;
      return NULL;
   } else if (rd != (int)to_read) { // truncated data in file
      fprintf(stderr, "TMReadEvent: error: short read %d instead of %d\n", (int)rd, (int)to_read);
      e->error = true;
      return e;
   }

   return e;
}

void TMWriteEvent(TMWriterInterface* writer, const TMEvent* event)
{
   writer->Write(&(event->data[0]), event->data.size());
}

std::string TMEvent::HeaderToString() const
{
   char buf[1024];
   snprintf(buf, sizeof(buf), "event: id %d, mask 0x%04x, serial %d, time %d, size %d, error %d, banks %d",
            event_id, trigger_mask, serial_number, time_stamp, data_size, error, (int)banks.size());
   return buf;
}

std::string TMEvent::BankListToString() const
{
   std::string s;
   for (unsigned i=0; i<banks.size(); i++) {
      if (i>0)
         s += ",";
      s += banks[i].name;
   }
   return s;
}

std::string TMEvent::BankToString(const TMBank*b) const
{
   char buf[1024];
   snprintf(buf, sizeof(buf), "name \"%s\", type %d, size %d, offset %d\n",
            b->name.c_str(), b->type, b->data_size, (int)b->data_offset);
   return buf;
}

TMEvent::TMEvent() // ctor
{
   Reset();
}

void TMEvent::Reset()
{
   error = false;

   event_id      = 0;
   trigger_mask  = 0;
   serial_number = 0;
   time_stamp    = 0;
   data_size     = 0;

   event_header_size = 0;
   bank_header_flags  = 0;

   banks.clear();
   data.clear();

   found_all_banks = false;
   bank_scan_position = 0;
}

void TMEvent::ParseEvent()
{
   ParseHeader(data.data(), data.size());

   if (data.size() != event_header_size + data_size) {
      fprintf(stderr, "TMEvent::ParseEvent: error: vector size %d mismatch against event size in event header: data_size %d, event_size %d\n", (int)data.size(), (int)data_size, (int)(event_header_size + data_size));
      error = true;
   }
}

void TMEvent::ParseHeader(const void* buf, size_t buf_size)
{
   event_header_size = 4*4;

   const char* event_header = (const char*)buf;

   if (buf_size < event_header_size) {
      fprintf(stderr, "TMEvent::ctor: error: buffer size %d is smaller than event header size %d\n", (int)buf_size, (int)event_header_size);
      error = true;
      return;
   }

   event_id      = GetU16(event_header+0);
   trigger_mask  = GetU16(event_header+2);
   serial_number = GetU32(event_header+4);
   time_stamp    = GetU32(event_header+8);
   data_size     = GetU32(event_header+12);

   bank_header_flags = 0;

   //if (!e->old_event.IsGoodSize()) { // invalid event size
   //   e->error = true;
   //   return e;
   //}
}

TMEvent::TMEvent(const void* buf, size_t buf_size)
{
   bool gOnce = true;
   if (gOnce) {
      gOnce = false;
      assert(sizeof(char)==1);
      assert(sizeof(uint16_t)==2);
      assert(sizeof(uint32_t)==4);
   }

   Reset();
   ParseHeader(buf, buf_size);

   if (error) {
      return;
   }

   size_t zdata_size  = data_size;
   size_t zevent_size = zdata_size + event_header_size;

   if (zevent_size != buf_size) {
      fprintf(stderr, "TMEvent::ctor: error: buffer size %d mismatch against event size in event header: data_size %d, event_size %d\n", (int)buf_size, (int)zdata_size, (int)zevent_size);
      error = true;
      return;
   }

   //data.resize(event_header_size + to_read);
   //memcpy(&data[0], xdata, event_header_size + to_read);

   const char* cptr = (const char*)buf;

   data.assign(cptr, cptr + zevent_size);

   assert(data.size() == zevent_size);
}

void TMEvent::Init(uint16_t xevent_id, uint16_t xtrigger_mask, uint32_t xserial_number, uint32_t xtime_stamp, size_t capacity)
{
   Reset();

   event_header_size = 4*4;

   event_id = xevent_id;
   trigger_mask = xtrigger_mask;
   serial_number = xserial_number;
   if (xtime_stamp) {
      time_stamp = xtime_stamp;
   } else {
      time_stamp = time(NULL);
   }

   data_size   = 8; // data size: 2 words of bank header, 0 words of data banks
   bank_header_flags = 0x00000031; // bank header flags, bk_init32a() format

   // bank list is empty

   bank_scan_position = 0;
   found_all_banks = false;

   // allocate event buffer

   if (capacity < 1024)
      capacity = 1024;

   data.reserve(capacity);

   // create event data

   data.resize(event_header_size + data_size);

   char* event_header = data.data();

   PutU16(event_header+0,  event_id);
   PutU16(event_header+2,  trigger_mask);
   PutU32(event_header+4,  serial_number);
   PutU32(event_header+8,  time_stamp);
   PutU32(event_header+12, data_size);
   PutU32(event_header+16, 0); // bank header data size
   PutU32(event_header+20, bank_header_flags); // bank header flags
}

void TMEvent::AddBank(const char* bank_name, int tid, const char* buf, size_t size)
{
   assert(data.size() > 0); // must call Init() before calling AddBank()

   size_t bank_size = event_header_size + Align8(size);

   size_t end_of_event_offset = data.size();
   
   data.resize(end_of_event_offset + bank_size);

   char* pbank = data.data() + end_of_event_offset;

   //printf("AddBank: buf %p size %d, bank size %d, data.size %d, data.data %p, pbank %p, memcpy %p\n", buf, (int)size, (int)bank_size, (int)data.size(), data.data(), pbank, pbank+4*4);

   pbank[0] = bank_name[0]; 
   pbank[1] = bank_name[1]; 
   pbank[2] = bank_name[2]; 
   pbank[3] = bank_name[3]; 
   PutU32(pbank+1*4, tid);
   PutU32(pbank+2*4, size);
   PutU32(pbank+3*4, 0); // bk_init32a() extra padding/reserved word
   
   memcpy(pbank+4*4, buf, size);

   // adjust event header

   data_size += bank_size;

   char* event_header = data.data();
   PutU32(event_header + 12, data_size); // event header
   PutU32(event_header + 16, data_size - 8); // bank header

   // add a bank record

   if (found_all_banks) {
      TMBank b;
      b.name = bank_name;
      b.type = tid;
      b.data_size = size;
      b.data_offset = end_of_event_offset + 4*4;
      banks.push_back(b);
   }
}

static size_t FindFirstBank(TMEvent* e)
{
   if (e->error)
      return 0;
   
   size_t off = e->event_header_size;
   
   if (e->data.size() < off + 8) {
      fprintf(stderr, "TMEvent::FindFirstBank: error: data size %d is too small\n", (int)e->data.size());
      e->error = true;
      return 0;
   }

   uint32_t bank_header_data_size = GetU32(&e->data[off]);
   uint32_t bank_header_flags     = GetU32(&e->data[off+4]);

   //printf("bank header: data size %d, flags 0x%08x\n", bank_header_data_size, bank_header_flags);

   if (bank_header_data_size + 8 != e->data_size) {
      fprintf(stderr, "TMEvent::FindFirstBank: error: bank header size %d mismatch against data size %d\n", (int)bank_header_data_size, (int)e->data_size);
      e->error = true;
      return 0;
   }

   e->bank_header_flags = bank_header_flags;

   return off+8;
}

#if 0
static char xchar(char c)
{
   if (c>='0' && c<='9')
      return c;
   if (c>='a' && c<='z')
      return c;
   if (c>='A' && c<='Z')
      return c;
   return '$';
}
#endif

static size_t FindNextBank(TMEvent* e, size_t pos, TMBank** pb)
{
   if (e->error)
      return 0;

   size_t remaining = e->data.size() - pos;

   //printf("pos %d, event data_size %d, size %d, remaining %d\n", pos, e->data_size, (int)e->data.size(), remaining);

   if (remaining == 0) {
      // end of data, no more banks to find
      e->found_all_banks = true;
      return 0;
   }

   if (remaining < 8) {
      fprintf(stderr, "TMEvent::FindNextBank: error: too few bytes %d remaining at the end of event\n", (int)remaining);
      e->error = true;
      e->found_all_banks = true; // no more banks will be found after this error
      return 0;
   }

   size_t ibank = e->banks.size();
   e->banks.resize(ibank+1);

   TMBank* b = &e->banks[ibank];

   b->name.resize(4);
   b->name[0] = e->data[pos+0];
   b->name[1] = e->data[pos+1];
   b->name[2] = e->data[pos+2];
   b->name[3] = e->data[pos+3];
   b->name[4] = 0;

   size_t data_offset = 0;

   //printf("bank header flags: 0x%08x\n", e->bank_header_flags);

   if (e->bank_header_flags & (1<<5)) {
      // bk_init32a format
      b->type      = GetU32(&e->data[pos+4+0]);
      b->data_size = GetU32(&e->data[pos+4+4]);
      data_offset = pos+4+4+4+4;
   } else if (e->bank_header_flags & (1<<4)) {
      // bk_init32 format
      b->type      = GetU32(&e->data[pos+4+0]);
      b->data_size = GetU32(&e->data[pos+4+4]);
      data_offset = pos+4+4+4;
   } else {
      // bk_init format
      b->type      = GetU16(&e->data[pos+4+0]);
      b->data_size = GetU16(&e->data[pos+4+2]);
      data_offset = pos+4+2+2;
   }

   b->data_offset = data_offset;

   //printf("found bank at pos %d: %s\n", pos, e->BankToString(b).c_str());
   
   if (b->type < 1 || b->type >= TID_LAST) {
      fprintf(stderr, "TMEvent::FindNextBank: error: invalid tid %d\n", b->type);
      e->error = true;
      return 0;
   }

   size_t aligned_data_size = Align8(b->data_size);

   //printf("data_size %d, alignemnt: %d %d, aligned %d\n", b->data_size, align, b->data_size%align, aligned_data_size);

   size_t npos = data_offset + aligned_data_size;

   //printf("pos %d, next bank at %d: [%c%c%c%c]\n", pos, npos, xchar(e->data[npos+0]), xchar(e->data[npos+1]), xchar(e->data[npos+2]), xchar(e->data[npos+3]));

   if (npos > e->data.size()) {
      fprintf(stderr, "TMEvent::FindNextBank: error: invalid bank data size %d: aligned %d, npos %d, end of event %d\n", b->data_size, (int)aligned_data_size, (int)npos, (int)e->data.size());
      e->error = true;
      return 0;
   }

   if (pb)
      *pb = b;

   return npos;
}

char* TMEvent::GetEventData()
{
   if (error)
      return NULL;
   if (event_header_size == 0)
      return NULL;
   if (event_header_size > data.size())
      return NULL;
   if (event_header_size + data_size > data.size())
      return NULL;
   return &data[event_header_size];
}

const char* TMEvent::GetEventData() const
{
   if (error)
      return NULL;
   if (event_header_size == 0)
      return NULL;
   if (event_header_size > data.size())
      return NULL;
   if (event_header_size + data_size > data.size())
      return NULL;
   return &data[event_header_size];
}

char* TMEvent::GetBankData(const TMBank* b)
{
   if (error)
      return NULL;
   if (!b)
      return NULL;
   if (b->data_offset >= data.size())
      return NULL;
   if (b->data_offset + b->data_size > data.size())
      return NULL;
   return &data[b->data_offset];
}

const char* TMEvent::GetBankData(const TMBank* b) const
{
   if (error)
      return NULL;
   if (!b)
      return NULL;
   if (b->data_offset >= data.size())
      return NULL;
   if (b->data_offset + b->data_size > data.size())
      return NULL;
   return &data[b->data_offset];
}

TMBank* TMEvent::FindBank(const char* bank_name)
{
   if (error)
      return NULL;

   // if we already found this bank, return it

   if (bank_name)
      for (unsigned i=0; i<banks.size(); i++) {
         if (banks[i].name == bank_name)
            return &banks[i];
      }

   //printf("found_all_banks %d\n", found_all_banks);

   if (found_all_banks)
      return NULL;
   
   size_t pos = bank_scan_position;

   if (pos == 0)
      pos = FindFirstBank(this);

   //printf("pos %d\n", pos);

   while (pos > 0) {
      TMBank* b = NULL;
      pos = FindNextBank(this, pos, &b);
      bank_scan_position = pos;
      //printf("pos %d, b %p\n", pos, b);
      if (pos>0 && b && bank_name) {
         if (b->name == bank_name)
            return b;
      }
   }

   found_all_banks = true;

   return NULL;
}

void TMEvent::FindAllBanks()
{
   if (found_all_banks)
      return;

   FindBank(NULL);

   assert(found_all_banks);
}

void TMEvent::PrintHeader() const
{
   printf("Event: id 0x%04x, mask 0x%04x, serial 0x%08x, time 0x%08x, data size %d, vector size %d, capacity %d\n",
          event_id,
          trigger_mask,
          serial_number,
          time_stamp,
          data_size,
          (int)data.size(),
          (int)data.capacity()
          );
}

void TMEvent::PrintBanks(int level)
{
   FindAllBanks();
   
   printf("%d banks:", (int)banks.size());
   for (size_t i=0; i<banks.size(); i++) {
      printf(" %s", banks[i].name.c_str());
   }
   printf("\n");

   if (level > 0) {
      for (size_t i=0; i<banks.size(); i++) {
         printf("bank %3d: name \"%s\", tid %2d, data_size %8d\n",
                (int)i,
                banks[i].name.c_str(),
                banks[i].type,
                banks[i].data_size);
         if (level > 1) {
            const char* p = GetBankData(&banks[i]);
            if (p) {
               for (size_t j=0; j<banks[i].data_size; j+=4) {
                  printf("%11d: 0x%08x\n", (int)j, *(uint32_t*)(p+j));
               }
            }
         }
      }
   }
}

void TMEvent::DumpHeader() const
{
   const char* p = data.data();

   // event header
   printf(" 0: 0x%08x\n", GetU32(p+0*4));
   printf(" 1: 0x%08x\n", GetU32(p+1*4));
   printf(" 2: 0x%08x\n", GetU32(p+2*4));
   printf(" 3: 0x%08x (0x%08x and 0x%08x-0x10)\n", GetU32(p+3*4), data_size, (int)data.size());

   // bank header
   printf(" 4: 0x%08x\n", GetU32(p+4*4));
   printf(" 5: 0x%08x (0x%08x)\n", GetU32(p+5*4), bank_header_flags);
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */