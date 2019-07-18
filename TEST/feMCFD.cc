//********************************************************************
//
//  Name:         feMCFD.cc
//  Created by:   Kolby Kiesling
//
//  Contents:     Slow control readout of MCFD16
//
//  $Id: $
//
//********************************************************************

#include <cstdio>
#include "midas.h"
#include "mfe.h"

#include "dd_mcfd16.h"

//#ifdef __cplusplus
//extern "C" {
//#endif
#include "rs232.h"  // $MIDASSYS/drivers/bus
#include "multi.h"  // $MIDASSYS/drivers/class
//#ifdef __cplusplus
//}
//#endif


//-- Globals ---------------------------------------------------------
//#ifdef __cplusplus
//extern "C" {
//#endif

const char *frontend_name = "feMCFD";  // client name visible to other MIDAS clients
const char *frontend_file_name = __FILE__;     // This frontend's source file name.  Do not change it.

BOOL frontend_call_loop = TRUE;  // frontend_loop will be called periodically if TRUE
INT display_period = 0;          // milliseconds, frontend status page will be displayed/updated if > zero

INT max_event_size = 2048;                    // maximum size of produced events
INT max_event_size_frag = 5 * max_event_size; // maximum size for fragmented events (EQ_FRAGMENTED)
INT event_buffer_size = 10 * max_event_size;  // buffer size to hold events

//-- Equipment list --------------------------------------------------

// device driver list

DEVICE_DRIVER mcfd_driver[] = {
   {"MCFD16", dd_mcfd16, 2, rs232, DF_INPUT | DF_OUTPUT},
   {""}
};

EQUIPMENT equipment[] = {

   {"Mesytec MCFD16",                     // equipment name
      {15, 0,                     // event ID, trigger mask
         "SYSTEM",                // event buffer
         EQ_SLOW,                 // equipment type
         0,                       // event source
         "FIXED",                 // format
         TRUE,                    // enabled
         RO_ALWAYS,               // read always
         1000,                    // read every second 
         0,                       // stop run after this event limit
         0,                       // number of sub events
         60,                      // log history (minimum interval in seconds, 0==disabled)
         "", "", ""} ,
      cd_multi_read,              // readout routine
      cd_multi,                   // class driver main routine
      mcfd_driver,             // device driver list
      NULL,                       // init string
   },

   {""}
};


//-- Dummy routines --------------------------------------------------

//INT poll_event(INT source[], INT count, BOOL test)
INT poll_event(INT source, INT count, BOOL test)
{
   return 1;
}

//INT interrupt_configure(INT cmd, INT source[], POINTER_T adr)
INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
{
   return 1;
}

//-- Frontend Init ---------------------------------------------------

INT frontend_init()
{
  // TODO: override the names of the variable...
   return CM_SUCCESS;
}

//-- Frontend Exit ---------------------------------------------------

INT frontend_exit()
{
   int idx=0;
   set_equipment_status(equipment[idx].name, "(frontend stopped)", "#FF0000");
   return CM_SUCCESS;
}

//-- Frontend Loop ---------------------------------------------------

INT frontend_loop()
{
   ss_sleep(50); // limit CPU usage
   return CM_SUCCESS;
}

//-- Begin of Run ----------------------------------------------------

INT begin_of_run(INT run_number, char *error)
{
   return CM_SUCCESS;
}

//-- End of Run ------------------------------------------------------

INT end_of_run(INT run_number, char *error)
{
   return CM_SUCCESS;
}

//-- Pause Run -------------------------------------------------------

INT pause_run(INT run_number, char *error)
{
   return CM_SUCCESS;
}

//-- Resume Run ------------------------------------------------------

INT resume_run(INT run_number, char *error)
{
   return CM_SUCCESS;
}

//--------------------------------------------------------------------
//#ifdef __cplusplus
//}
//#endif
