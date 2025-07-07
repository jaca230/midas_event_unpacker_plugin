// midasio.h

#ifndef MIDASIO_H
#define MIDASIO_H

#include <string>
#include <vector>
#include <stdint.h>

#ifndef TID_LAST
/**
MIDAS Data Type Definitions                             min      max    */
#define TID_BYTE      1       /**< DEPRECATED, use TID_UINT8 instead    */
#define TID_UINT8     1       /**< unsigned byte         0       255    */
#define TID_SBYTE     2       /**< DEPRECATED, use TID_INT8 instead     */
#define TID_INT8      2       /**< signed byte         -128      127    */
#define TID_CHAR      3       /**< single character      0       255    */
#define TID_WORD      4       /**< DEPRECATED, use TID_UINT16 instead   */
#define TID_UINT16    4       /**< two bytes             0      65535   */
#define TID_SHORT     5       /**< DEPRECATED, use TID_INT16 instead    */
#define TID_INT16     5       /**< signed word        -32768    32767   */
#define TID_DWORD     6       /**< DEPRECATED, use TID_UINT32 instead   */
#define TID_UINT32    6       /**< four bytes            0      2^32-1  */
#define TID_INT       7       /**< DEPRECATED, use TID_INT32 instead    */
#define TID_INT32     7       /**< signed dword        -2^31    2^31-1  */
#define TID_BOOL      8       /**< four bytes bool       0        1     */
#define TID_FLOAT     9       /**< 4 Byte float format                  */
#define TID_FLOAT32   9       /**< 4 Byte float format                  */
#define TID_DOUBLE   10       /**< 8 Byte float format                  */
#define TID_FLOAT64  10       /**< 8 Byte float format                  */
#define TID_BITFIELD 11       /**< 32 Bits Bitfield      0  111... (32) */
#define TID_STRING   12       /**< zero terminated string               */
#define TID_ARRAY    13       /**< array with unknown contents          */
#define TID_STRUCT   14       /**< structure with fixed length          */
#define TID_KEY      15       /**< key in online database               */
#define TID_LINK     16       /**< link in online database              */
#define TID_INT64    17       /**< 8 bytes int          -2^63   2^63-1  */
#define TID_UINT64   18       /**< 8 bytes unsigned int  0      2^64-1  */
#define TID_QWORD    18       /**< 8 bytes unsigned int  0      2^64-1  */
#define TID_LAST     19       /**< end of TID list indicator            */
#endif

class TMBank
{
 public:
   std::string name;            ///< bank name, 4 characters max
   uint32_t    type = 0;        ///< type of bank data, enum of TID_xxx
   uint32_t    data_size   = 0; ///< size of bank data in bytes
   size_t      data_offset = 0; ///< offset of data for this bank in the event data[] container
};

class TMEvent
{
public: // event data
   bool error;                  ///< event has an error - incomplete, truncated, inconsistent or corrupted
   
   uint16_t event_id;           ///< MIDAS event ID
   uint16_t trigger_mask;       ///< MIDAS trigger mask
   uint32_t serial_number;      ///< MIDAS event serial number
   uint32_t time_stamp;         ///< MIDAS event time stamp (unix time in sec)
   uint32_t data_size;          ///< MIDAS event data size

   size_t   event_header_size;  ///< size of MIDAS event header
   uint32_t bank_header_flags;  ///< flags from the MIDAS event bank header

   std::vector<TMBank> banks;   ///< list of MIDAS banks, fill using FindAllBanks()
   std::vector<char> data;      ///< MIDAS event bytes

public: // internal data

   bool found_all_banks;        ///< all the banks in the event data have been discovered
   size_t bank_scan_position;   ///< location where scan for MIDAS banks was last stopped

public: // constructors
   TMEvent(); // ctor
   TMEvent(const void* buf, size_t buf_size); // ctor
   void Reset();       ///< reset everything
   void ParseEvent();  ///< parse event data
   void ParseHeader(const void* buf, size_t buf_size); ///< parse event header
   void Init(uint16_t event_id, uint16_t trigger_mask = 0, uint32_t serial_number = 0, uint32_t time_stamp = 0, size_t capacity = 0);

public: // read data
   void FindAllBanks();                      ///< scan the MIDAS event, find all data banks
   TMBank* FindBank(const char* bank_name);  ///< scan the MIDAS event
   char* GetEventData();                     ///< get pointer to MIDAS event data
   const char* GetEventData() const;         ///< get pointer to MIDAS event data
   char* GetBankData(const TMBank*);         ///< get pointer to MIDAS data bank
   const char* GetBankData(const TMBank*) const; ///< get pointer to MIDAS data bank

public: // add data
   void AddBank(const char* bank_name, int tid, const char* buf, size_t size); ///< add new MIDAS bank

public: // bank manipulation
   //void DeleteBank(const TMBank*);           ///< delete MIDAS bank

public: // information methods

   std::string HeaderToString() const;            ///< print the MIDAS event header
   std::string BankListToString() const;          ///< print the list of MIDAS banks
   std::string BankToString(const TMBank*) const; ///< print definition of one MIDAS bank

   void PrintHeader() const;
   void PrintBanks(int level = 0);
   void DumpHeader() const;
};

class TMReaderInterface
{
 public:
   TMReaderInterface(); // ctor
   virtual int Read(void* buf, int count) = 0;
   virtual int Close() = 0;
   virtual ~TMReaderInterface() {};
 public:
   bool fError;
   std::string fErrorString;
   static bool fgTrace;
};

class TMWriterInterface
{
 public:
   virtual int Write(const void* buf, int count) = 0;
   virtual int Close() = 0;
   virtual ~TMWriterInterface() {};
 public:
   static bool fgTrace;
};

TMReaderInterface* TMNewReader(const char* source);
TMWriterInterface* TMNewWriter(const char* destination);

TMEvent* TMReadEvent(TMReaderInterface* reader);
void TMWriteEvent(TMWriterInterface* writer, const TMEvent* event);

extern bool TMTraceCtorDtor;

#endif

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */