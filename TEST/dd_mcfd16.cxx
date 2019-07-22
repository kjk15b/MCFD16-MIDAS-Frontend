//********************************************************************
//
//  Name:         dd_mcfd16.cxx
//  Created by:   Kolby Kiesling
//
//  Contents:     Device driver for Mesytec MCFD16
//
//  $Id: $
//
//********************************************************************

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <regex>

#include "midas.h"

#undef calloc



#define DEFAULT_TIMEOUT 1000     // milliseconds


#define CHAN_INP_PULSER 0
#define CHAN_OUT_FREQUENCY 1
#define CHAN_OUT_RE 2 // not known if this is necessary...
#define NAN 3


#define DD_MCFD_SETTINGS_STR "\
pulser = INT : 1\n\
Read Period ms = FLOAT : 200\n\
"


typedef struct {
  int pulser; // test pulser status
  int readPeriod_ms; // maybe need?
//   bool manual_control; // maybe...
} DD_MCFD_SETTINGS;


typedef struct {
  DD_MCFD_SETTINGS settings;
  DD_MCFD_SETTINGS settingsIncoming;

  INT num_channels;
  INT(*bd)(INT cmd, ...);      // bus driver entry function
  void *bd_info;               // private info of bus driver
  HNDLE hkey;                  // ODB key for bus driver info


  float channel_0_frequency; // 0-15 standard channels 16-18 are trig0,1,2 and 19 is total 
  
  float *array;                // Most recent measurement or NaN, one for each channel
  DWORD *update_time;          // seconds

  INT get_label_calls;

} DD_MCFD_INFO;


// Should probably call this every time the fe is started.  This would log PID parameters to the midas.log so they can be recovered later...
//int recall_pid_settings(bool saveToODB=false); // read from Arduino, print to messages/stdout, optionally save to ODB


int mcfd_apply_settings(DD_MCFD_INFO* info) {
  /* Format of most commands to R / W from the MCFD:
   * (char) + (int) + (opt. modifier) 
   * Where char is the identifying register, int is the data write and optional modifier is for special cases...
   */
  char cmd[256]; // only can set the pulser right now...
  char str[256];
  memset(cmd, 0, sizeof(cmd));
  // want to write for loops to automate the r / w process...
  
  snprintf(cmd, sizeof(cmd)-1, "p%d\r\n", info->settings.pulser); // set pulser
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  return FE_SUCCESS; // TODO: make sure things applied okay...
}


void mcfd_settings_updated(INT hDB, INT hkey, void* vinfo)
{ // simple routines to iterate through to check for changes in the ODB...
  printf("Settings updated\n");

  DD_MCFD_INFO* info = (DD_MCFD_INFO*) vinfo;

  bool changed=false;

  if (info->settingsIncoming.readPeriod_ms != info->settings.readPeriod_ms) {
    std::cout << "   readPeriod_ms changed from ``" << info->settings.readPeriod_ms << "'' to ``" << info->settingsIncoming.readPeriod_ms << "''" << std::endl;
    info->settings.readPeriod_ms = info->settingsIncoming.readPeriod_ms;
    changed=true;
  }

  if (info->settingsIncoming.pulser != info->settings.pulser) {
    std::cout << "   pulser changed from ``" << info->settings.pulser << "'' to ``" << info->settingsIncoming.pulser << "''" << std::endl;
    info->settings.pulser = info->settingsIncoming.pulser;
    changed=true;
  }
  
  if (changed) mcfd_apply_settings(info); // TODO: only apply the changed ones...
}


//---- standard device driver routines -------------------------------

INT dd_mcfd16_init(HNDLE hkey, void **pinfo, INT channels, INT(*bd)(INT cmd, ...))
{
  int status;
  HNDLE hDB, hkeydd;
  DD_MCFD_INFO *info;
  printf("dd_mcfd16_init: channels = %d\n", channels);

  info = new DD_MCFD_INFO;
  *pinfo = info;

  cm_get_experiment_database(&hDB, NULL);

  info->array = (float*) calloc(channels, sizeof(float));
  info->update_time = (DWORD*) calloc(channels, sizeof(DWORD));
  
  for (int i=0; i<channels; ++i) { // TODO: obviously change this to be back to channels, just making notes for the meantime...
    info->array[i] = ss_nan();
    info->update_time[i] = 0;
  }
  
  info->channel_0_frequency = ss_nan();
  info->get_label_calls=0;  
  
  info->num_channels = channels;  // TODO: make sure it is 19 channel readout
  info->bd = bd;
  info->hkey = hkey;

  // DD Settings
  status = db_create_record(hDB, hkey, "DD", DD_MCFD_SETTINGS_STR); // should make the database correctly now...
  if (status != DB_SUCCESS)
     return FE_ERR_ODB;

  status = db_find_key(hDB, hkey, "DD", &hkeydd);
  if (status != DB_SUCCESS) {
    return FE_ERR_ODB;
  }
  int size = sizeof(info->settingsIncoming);
  status = db_get_record(hDB, hkeydd, &info->settingsIncoming, &size, 0);
  if (status != DB_SUCCESS) return FE_ERR_ODB;

  status = db_open_record(hDB, hkeydd, &info->settingsIncoming,
                          size, MODE_READ, mcfd_settings_updated, info);
  if (status != DB_SUCCESS) {
    return FE_ERR_ODB;
  }
  memcpy(&info->settings, &info->settingsIncoming, sizeof(info->settingsIncoming));

  // Initialize bus driver
  status = info->bd(CMD_INIT, info->hkey, &info->bd_info);
  if (status != SUCCESS) return status;
  
  printf("Sending initialization commands to MCFD16\n");
  

  char str[256];
  memset(str, 0, sizeof(str));
  int len=0;
  // TODO: Check to see if MCFD16 is outputing data
  BD_PUTS("p1\r\n"); // "get" measurement
  int tries=0;
  for (tries=0; tries<10; ++tries) {
    int len = BD_GETS(str, sizeof(str), "\r\n", 1000); // will have to format this for MCFD, will need heavy testing
    //if (len==0 && tries > 4) break;
    printf("str=%s\t\ttry=%d\t\tlen=%d\n", str, tries, len);
  }
  printf("read tries=%d, len=%d, str=``%s''\n", tries, len, str); // debugging
  if (len == 0) {
  }


  BD_PUTS("ra 0\r\n");
  len = BD_GETS(str, sizeof(str)-1, "\r\n", 1000);  // EXTRA dummy read to readback "echo?"
  len = BD_GETS(str, sizeof(str)-1, "\r\n", 1000);
  printf("ra 0 response = ``%s''\n",str);

  for (tries=0; tries<500; ++tries) {
    int len = BD_GETS(str, sizeof(str)-1, "\r\n", 5);
    if (len==0 && tries > 2) break;
  }


  mcfd_apply_settings(info); // settings are probably not functional, but they appear to be getting there...

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_exit(DD_MCFD_INFO * info)
{
  printf("Running dd_mcfd_exit\n");

  // Close serial
  info->bd(CMD_EXIT, info->bd_info);

//   if (info->array) free(info->array);
//   if (info->update_time) free(info->update_time);
//   delete info->recent;
  delete info;

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_set(DD_MCFD_INFO * info, INT channel, int value) // TODO: make sure value is int everywhere...
{
  if (channel < 0 || channel >= info->num_channels)
    return FE_ERR_DRIVER;

  channel+=info->num_channels;

  char cmd[256];
  char str[256];
  memset(cmd, 0, sizeof(cmd));

  printf("Set channel %d to %d\n", channel, value);
  
  switch (channel) {
    case CHAN_INP_PULSER: // Pulser status
      // TODO: make sure "value" is reasonable
      info->settings.pulser = value;
      snprintf(cmd, sizeof(cmd)-1, "p%d\r\n", info->settings.pulser);
      BD_PUTS(cmd);
      BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
      printf("Pulser set to %d\n", info->settings.pulser);
      // TODO: make sure it was applied correctly...
      break;
    case CHAN_OUT_RE:
      // FIXME: not implemented
      //int ivalue = (int) value;
      //if (ivalue < 0 || ivalue > 255) {
        //std::cerr << "[dd_arduino_fan_pid_set] Error: Value = " << value << " is out of range for Channel["<< channel <<"]" << std::endl;
        //return FE_ERR_DRIVER;
      //}
      break;
  }

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_get(DD_MCFD_INFO * info, INT channel, float *pvalue)
{
  // Get: PID Output and PV

  INT status = 0;
  char str[256];
  *pvalue = ss_nan();
  memset(str, 0,sizeof(str)-1);
  
  //const char cstr[];
  std::cmatch cm;
  std::regex match ("(rate)(.*)");

  status = BD_PUTS("ra 0\r\n");
  if (status < 0) {
    std::cerr << "BD_PUTS error." << std::endl;
    al_trigger_alarm("MCFD16", "MCFD16 communication failure with Mesytec MCFD16.", 0, "BD_PUTS returns < 0", AT_INTERNAL);
    return FE_ERR_HW;
  }
  //}
  //int attempts=100;
  //while (attempts>=0) {
    status = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);  // EXTRA dummy read to readback "echo?"
    ss_sleep(50);
    printf("String:\t%s\n", str);
    status = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); 
    printf("String2:\t%s\n", str);
    if (status <= 0) {
      std::cerr << "BD_GETS error." << std::endl;
    }
    std::cout<<std::regex_match (str, cm, match)<<std::endl;
    if (cm.size() > 0) {
      printf("String:\t%s\n", str);
    }
    
    //attempts--;
  //}

  float frq=0;
  if ( sscanf(str,"%f",&frq) != 1) { // need to figure out how to read this properly
    std::cerr << "Error: Failed to parse data from MCFD16." << std::endl;
    std::cerr << "received: ``" << str << "''" << std::endl;
//    al_trigger_alarm("FanPID", "Fan PID communication failure with Arduino.", 0, "Failed to parse received data", AT_INTERNAL); // TODO: re-enable this...
    return FE_ERR_HW;
  }

   printf("FRQ=%.2f\n", frq);

  switch (channel) {
    case CHAN_OUT_FREQUENCY:
      *pvalue = frq;
      info->channel_0_frequency = frq;
      break;
    default:
      *pvalue = ss_nan();
      break;
  }

  //ss_sleep(1);
  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_get_label(DD_MCFD_INFO * info, INT channel, char *name)
{

  // Keep track of calls to this function to get right channel labels with multi.c class driver.
  if (info->num_channels==2 && info->get_label_calls > info->num_channels/2) {
    channel+=info->num_channels;
  }

  switch (channel) {
    case CHAN_INP_PULSER:
      strncpy(name, "Pulser", NAME_LENGTH-1);
      break;
    case CHAN_OUT_RE:
      strncpy(name, "reserved", NAME_LENGTH-1);
      break;
    case CHAN_OUT_FREQUENCY:
      strncpy(name, "Channel Frequency", NAME_LENGTH-1);
      break;
    case NAN:
      strncpy(name, "NAN", NAME_LENGTH-1);
      break;
    default:
      //snprintf(name,"");
      memset(name, 0, NAME_LENGTH);
      //return FE_ERR_DRIVER;
  }

  //printf("dd_arduino_fan_pid_get_label: chan=%d, Name=``%s''\n",channel,name);
  info->get_label_calls++;
  return FE_SUCCESS;
}

//---- device driver entry point -------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

INT dd_mcfd16(INT cmd, ...)
{
  va_list argptr;
  HNDLE hKey;
  INT channel, status;
  DWORD flags=0;
  float value, *pvalue;
  void *info, *bd;
  char* name;

  va_start(argptr, cmd);
  status = FE_SUCCESS;

  switch (cmd) {
    case CMD_INIT:
      hKey = va_arg(argptr, HNDLE);
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      flags = va_arg(argptr, DWORD);
      if (flags==0) {} // prevent set-but-unused compile warning
      bd = va_arg(argptr, void *);
      status = dd_mcfd16_init(hKey, (void**)info, channel, (INT (*)(INT, ...)) bd);
      break;

    case CMD_EXIT:
      info = va_arg(argptr, void *);
      status = dd_mcfd_exit((DD_MCFD_INFO*) info);
      break;

    case CMD_SET:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      value = (int) va_arg(argptr, double); // probably will break...
      status = dd_mcfd_set((DD_MCFD_INFO*) info, channel, value);
      break;

    case CMD_GET:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      pvalue = va_arg(argptr, float *);
      status = dd_mcfd_get((DD_MCFD_INFO*) info, channel, pvalue);
      break;

    case CMD_GET_LABEL:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      name = va_arg(argptr, char *);
      status = dd_mcfd_get_label((DD_MCFD_INFO*) info, channel, name);
      break;

    default:
      break;
  }

  va_end(argptr);

  return status;
}

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------


