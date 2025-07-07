//
// test_midasio.cxx --- test midasio classes
//
// K.Olchanski
//

#include <stdio.h>
#include <sys/time.h>
#include <iostream>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h> // exit()

#ifdef HAVE_MIDAS
#include "TMidasOnline.h"
#endif
#ifdef HaVE_MVODB
#include "mvodb.h"
#endif

#include "midasio.h"

std::string toString(int i)
{
  char buf[256];
  snprintf(buf, sizeof(buf),"%d", i);
  return buf;
}

void print_ia(const std::vector<int> &ia)
{
   int size = ia.size();
   printf("int[%d] has [", size);
   for (int i=0; i<size; i++) {
      if (i>0)
         printf(", ");
      printf("%d", ia[i]);
   }
   printf("]");
}

void print_da(const std::vector<double> &da)
{
   int size = da.size();
   printf("int[%d] has [", size);
   for (int i=0; i<size; i++) {
      if (i>0)
         printf(", ");
      printf("%f", da[i]);
   }
   printf("]");
}

static int gCountFail = 0;

void report_fail(const char* text)
{
   printf("FAIL: %s\n", text);
   gCountFail++;
}

void usage()
{
   fprintf(stderr, "Usage: test_midasio.exe [-h] [-o output_file] input_file1 [input_file2...]\n");
   exit(1); // DOES NOT RETURN
}

// Main function

int main(int argc, char *argv[])
{
   setbuf(stdout,NULL);
   setbuf(stderr,NULL);
 
   signal(SIGILL,  SIG_DFL);
   signal(SIGBUS,  SIG_DFL);
   signal(SIGSEGV, SIG_DFL);
   signal(SIGPIPE, SIG_DFL);

#ifdef HAVE_MIDAS
   const char* hostname = NULL;
   const char* exptname = NULL;
#endif
   std::vector<std::string> rfilenames;
   std::string wfilename;
   bool online  = false;
   bool test_init = false;

   for (int i=1; i<argc; i++) {
      if (strcmp(argv[i], "-h")==0) {
         usage();
      } else if (strcmp(argv[i], "--help")==0) {
         usage();
      } else if (strcmp(argv[i], "-i")==0) {
         test_init = true;
      } else if (strcmp(argv[i], "-o")==0) {
         i++;
         if (i == argc) usage();
         wfilename = argv[i];
      } else if (argv[i][0] == '-') {
         fprintf(stderr, "invalid switch: \"%s\"\n", argv[i]);
         usage();
      } else {
         rfilenames.push_back(argv[i]);
      }
   }

   TMWriterInterface* writer = NULL;

   if (!wfilename.empty()) {
      writer = TMNewWriter(wfilename.c_str());
   }

   if (test_init) {
      for (int i=0; i<10; i++) {
         TMEvent event;
         event.PrintHeader();
         event.Init(1);
         event.PrintHeader();
         //event.DumpHeader();
         uint32_t buf32[3];
         buf32[0] = 0x11111111;
         buf32[1] = 0x22222222;
         buf32[2] = 0x33333333;
         event.AddBank("AAAA", TID_UINT32, (char*)buf32, sizeof(buf32));
         event.PrintHeader();
         //event.DumpHeader();
         event.PrintBanks();
         uint64_t buf64[3];
         buf64[0] = 0x1111111111111111;
         buf64[1] = 0x2222222222222222;
         buf64[2] = 0x3333333333333333;
         event.AddBank("BBBB", TID_UINT64, (char*)buf64, sizeof(buf64));
         event.PrintHeader();
         //event.DumpHeader();
         event.PrintBanks();
         if (writer) {
            TMWriteEvent(writer, &event);
         }
      }
   }

   if (rfilenames.empty())
     online = true;
   
#if HAVE_MVODB
   MVOdb* odb = NULL;
   MVOdbError odberror;
#endif

   if (online) {
#ifdef HAVE_MIDAS
      printf("Using MidasOdb\n");
      midas = TMidasOnline::instance();
      
      int err = midas->connect(hostname, exptname, "test_mvodb");
      if (err != 0) {
         fprintf(stderr,"Cannot connect to MIDAS, error %d\n", err);
         return -1;
      }
      
      odb = MakeMidasOdb(midas->fDB, &odberror);
      
      if (midas)
         midas->disconnect();
#else
      printf("MIDAS support not available, sorry! Bye.\n");
      exit(1);
#endif
   } else {
      int counter = 0;
      for (size_t ri=0; ri<rfilenames.size(); ri++) {
         TMReaderInterface* reader = TMNewReader(rfilenames[ri].c_str());

         if (reader->fError) {
            printf("Cannot open input file \"%s\"\n", rfilenames[ri].c_str());
            delete reader;
            continue;
         }

         while (1) {
            TMEvent* e = TMReadEvent(reader);
            
            if (!e) {
               // EOF
               delete e;
               break;
            }
            
            if (e->error) {
               // broken event
               printf("event with error, bye!\n");
               delete e;
               break;
            }

            printf("Event: id 0x%04x, mask 0x%04x, serial 0x%08x, time 0x%08x, data size %d\n",
                   e->event_id,
                   e->trigger_mask,
                   e->serial_number,
                   e->time_stamp,
                   e->data_size);
            
            if ((e->event_id & 0xFFFF) == 0x8000) {
               printf("Event: this is a begin of run ODB dump\n");
#ifdef HAVE_MVODB
               odb = MakeFileDumpOdb(event.GetData(),event.GetDataSize(), &odberror);
#endif
               if (writer) {
                  TMWriteEvent(writer, e);
               }
               delete e;
               continue;
            } else if ((e->event_id & 0xFFFF) == 0x8001) {
               printf("Event: this is an end of run ODB dump\n");
#ifdef HAVE_MVODB
               odb = MakeFileDumpOdb(event.GetData(),event.GetDataSize(), &odberror);
#endif
               if (writer) {
                  TMWriteEvent(writer, e);
               }
               delete e;
               continue;
            }

            if (counter == 0) {
               e->PrintBanks(2);
            } else if (counter == 1) {
               e->PrintBanks(1);
            } else {
               e->PrintBanks();
            }

            if (writer) {
               TMWriteEvent(writer, e);
            }

            counter++;

            delete e;
         }
         
         reader->Close();
         delete reader;
      }
   }

   if (writer) {
      writer->Close();
      delete writer;
      writer = NULL;
   }

   return 0;
}

//end
/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */