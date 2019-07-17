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

#include "midas.h"

#undef calloc


//#define SP_8 1 // COMMON -> sets polarity to negative
//#define SG_8 1 // COMMON -> sets gain commonly to 1

#define DEFAULT_TIMEOUT 1000     // milliseconds
#define CHANNELS 16
#define PAIRINGS 8
#define THREE_CHANNELS 3
#define TWO_CHANNELS 2
#define FOUR_CHANNELS 4
#define NINETEEN_CHANNELS 19



#define DD_MCFD_SETTINGS_STR "\
BWL = INT : 1\n\
CFD = INT : 1\n\
Sk = INT : 0\n\
Sc = INT : 36\n\
Sv = INT : 0\n\
Gs = INT : 0\n\
Ga1 = INT : 255\n\
Ps = INT : 0\n\
Read Period ms = FLOAT : 200\n\
"

typedef struct {
  int* set_polarity; // pair
  int* set_gain; // pair
  int* set_threshold; // individual
  int* set_width; // pair 
  int* set_dead_time; // pair
  int* set_delay_line; // pair
  int* set_fraction; // pair
  int* trigger_source; // 3 values
  int* trigger_monitor; // 2 values
  int* trigger_pattern; // 4 values
  int* set_multiplicity; // Upper & lower
  int* paired_coincidence; // 1->15
} MCFD_CHANNEL_SETTINGS;

typedef struct {
  int BWL; // Bandwidth limit
  int CFD; // turn on CFD
  int Sk; // set mask for registers pairings
  int Sc; // global coincidence time
  int Sv; // fast veto input
  int Gs; // gate selector
  int Ga1; // Gate allowed timing, hard coding to falling edge right now b/c  of our equipment
  int Ps; // test pulser status
  int readPeriod_ms; // maybe need?
  
//   bool manual_control; // maybe...
} DD_MCFD_SETTINGS;


typedef struct {
  DD_MCFD_SETTINGS settings;
  MCFD_CHANNEL_SETTINGS chn_settings;
  DD_MCFD_SETTINGS settingsIncoming;

  INT num_channels;
  INT(*bd)(INT cmd, ...);      // bus driver entry function
  void *bd_info;               // private info of bus driver
  HNDLE hkey;                  // ODB key for bus driver info


  float* channel_frequency;
  
  float *array;                // Most recent measurement or NaN, one for each channel
  DWORD *update_time;          // seconds

  INT get_label_calls;

} DD_MCFD_INFO;


// Should probably call this every time the fe is started.  This would log PID parameters to the midas.log so they can be recovered later...
//int recall_pid_settings(bool saveToODB=false); // read from Arduino, print to messages/stdout, optionally save to ODB


int mcfd_apply_settings(DD_MCFD_INFO * info) {
  
  
  
  char cmd[256];
  char str[256];
  memset(cmd, 0, sizeof(cmd));
  //printf("Set channel %d to %.2f\n", channel, value);
  
  snprintf(cmd, sizeof(cmd)-1, "sp 0 %d\r\n", info->settings.Sp0); // set polarity
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "sg 0 %d\r\n", info->settings.Sg0); // set gain
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "bwl %d\r\n", info->settings.BWL); // set bandwidth limit
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "cfd %d\r\n", info->settings.CFD); // set CFD mode
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "st 0 %d\r\n", info->settings.St0); // set threshold
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "sw 0 %d\r\n", info->settings.Sw0); 
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  return FE_SUCCESS; // TODO: make sure things applied okay...
}


void mcfd_settings_updated(INT hDB, INT hkey, void* vinfo)
{
  printf("Settings updated\n");

  DD_MCFD_INFO* info = (DD_MCFD_INFO*) vinfo;

  bool changed=false;

  
  if (info->settingsIncoming.kP != info->settings.kP) {
    std::cout << "   kP changed from ``" << info->settings.kP << "'' to ``" << info->settingsIncoming.kP << "''" << std::endl;
    info->settings.kP = info->settingsIncoming.kP;
    //cm_msg(MINFO, "pid_settings_updated", "This frontend must be restarted for changes to     ``kP'' to take effect.");
    changed=true;
  }
  
  if (info->settingsIncoming.kI != info->settings.kI) {
    std::cout << "   kI changed from ``" << info->settings.kI << "'' to ``" << info->settingsIncoming.kI << "''" << std::endl;
    info->settings.kI = info->settingsIncoming.kI;
    changed=true;
  }

  if (info->settingsIncoming.kD != info->settings.kD) {
    std::cout << "   kD changed from ``" << info->settings.kD << "'' to ``" << info->settingsIncoming.kD << "''" << std::endl;
    info->settings.kD = info->settingsIncoming.kD;
    changed=true;
  }

  if (info->settingsIncoming.out_Min != info->settings.out_Min) {
    std::cout << "   out_Min changed from ``" << info->settings.out_Min << "'' to ``" << info->settingsIncoming.out_Min << "''" << std::endl;
    info->settings.out_Min = info->settingsIncoming.out_Min;
    changed=true;
  }

  if (info->settingsIncoming.out_Max != info->settings.out_Max) {
    std::cout << "   out_Max changed from ``" << info->settings.out_Max << "'' to ``" << info->settingsIncoming.out_Max << "''" << std::endl;
    info->settings.out_Max = info->settingsIncoming.out_Max;
    changed=true;
  }

  if (info->settingsIncoming.readPeriod_ms != info->settings.readPeriod_ms) {
    std::cout << "   readPeriod_ms changed from ``" << info->settings.readPeriod_ms << "'' to ``" << info->settingsIncoming.readPeriod_ms << "''" << std::endl;
    info->settings.readPeriod_ms = info->settingsIncoming.readPeriod_ms;
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
  
  info->chn_settings.set_polarity = (int*) calloc(PAIRINGS, sizeof(int)); // TODO: these values need to be 2, 3, 4, 8, or 16 in width, not by channels
  info->chn_settings.set_gain = (int*) calloc(PAIRINGS, sizeof(int));
  info->chn_settings.set_threshold = (int*) calloc(CHANNELS, sizeof(int));
  info->chn_settings.set_width = (int*) calloc(PAIRINGS, sizeof(int));
  info->chn_settings.set_dead_time = (int*) calloc(PAIRINGS, sizeof(int));
  info->chn_settings.set_delay_line = (int*) calloc(PAIRINGS, sizeof(int));
  info->chn_settings.set_fraction = (int*) calloc(PAIRINGS, sizeof(int));
  info->chn_settings.trigger_source = (int*) calloc(THREE_CHANNELS, sizeof(int));
  info->chn_settings.trigger_monitor = (int*) calloc(TWO_CHANNELS, sizeof(int));
  info->chn_settings.trigger_pattern = (int*) calloc(FOUR_CHANNELS, sizeof(int));
  info->chn_settings.set_multiplicity = (int*) calloc(TWO_CHANNELS, sizeof(int));
  info->chn_settings.paired_coincidence = (int*) calloc(CHANNELS, sizeof(int));
  
  info->num_channels = channels;  // TODO: make sure it is 19 channel readout
  info->bd = bd;
  info->hkey = hkey;
  
  for (int i=; i<NINETEEN_CHANNELS; i++) { // TODO: obviously change this to be back to channels, just making notes for the meantime...
    info->channel_frequency[i] = ss_nan(); // initialize the channel readouts
    info->array[i] = ss_nan();
    info->update_time[i] = 0;
  }
  
  info->get_label_calls=0;
  
  
  

  // DD Settings
  status = db_create_record(hDB, hkey, "DD", DD_MCFD_SETTINGS_STR);
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
  
  printf("Sending initialization commands to Arduino PID controller\n");
  

  char str[256];
  memset(str, 0, sizeof(str));
  int len=0;
  // TODO: Check to see if Arduino is outputing data
  // TODO: may need to read a lot of old buffered data...
  BD_PUTS("g\n"); // "get" measurement, also disables automatic printout as of PIDVer2_1
  int tries=0;
  for (tries=0; tries<500; ++tries) {
    int len = BD_GETS(str, sizeof(str)-1, "\r\n", 2);
    if (len==0 && tries > 4) break;
  }
  //printf("read tries=%d, len=%d, str=``%s''\n", tries, len, str);
  if (len == 0) {
  }


  BD_PUTS("w\n");
  len = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);  // EXTRA dummy read to readback "echo?"
  len = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);
  printf("w response = ``%s''\n",str);
//   const char testStr[] = "Kp\tKi\tKd\tmin\tmax\tset\treadPeriod";
//   printf ("%s\n%s\n",str, testStr);
//   printf ("%d\n",strncmp(str, testStr, 31));
//   printf ("%d\n",strcmp(str, testStr));
  if (strncmp(str, "Kp\tKi\tKd\tmin\tmax\tset\treadPeriod", 31) == 0) { // TODO: should be able to compare more than 31 chars to 37 or 38...
      len = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);
      printf("w response = ``%s''\n",str);
      float kp, ki, kd, omin, omax, setp, readPeriod;
      sscanf(str, "%f\t%f\t%f\t%f\t%f\t%f\t%f", &kp, &ki, &kd, &omin, &omax, &setp, &readPeriod);
      printf("%f\t%f\t%f\t%f\t%f\t%f\t%f\n", kp, ki, kd, omin, omax, setp, readPeriod);
  }
  
  for (tries=0; tries<500; ++tries) {
    int len = BD_GETS(str, sizeof(str)-1, "\r\n", 2);
    if (len==0 && tries > 2) break;
  }


  pid_apply_settings(info);

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_arduino_fan_pid_exit(DD_FAN_PID_INFO * info)
{
  printf("Running dd_arduino_fan_pid_exit\n");

  // Close serial
  info->bd(CMD_EXIT, info->bd_info);

//   if (info->array) free(info->array);
//   if (info->update_time) free(info->update_time);
//   delete info->recent;
  delete info;

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_arduino_fan_pid_set(DD_FAN_PID_INFO * info, INT channel, float value)
{
  if (channel < 0 || channel >= info->num_channels)
    return FE_ERR_DRIVER;

  channel+=info->num_channels;

  char cmd[256];
  char str[256];
  memset(cmd, 0, sizeof(cmd));

  printf("Set channel %d to %.2f\n", channel, value);
  
  switch (channel) {
    case CHAN_OUT_SP: // Setpoint
      // TODO: make sure "value" is reasonable
      info->set_point = value;
      snprintf(cmd, sizeof(cmd)-1, "s%.2f\n", info->set_point);
      BD_PUTS(cmd);
      BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
      printf("Setpoint to %.2f\n", info->set_point);
      //cmd << "s" << value << "\n";
      //BD_PUTS(cmd.str().c_str());
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

INT dd_arduino_fan_pid_get(DD_FAN_PID_INFO * info, INT channel, float *pvalue)
{
  // Get: PID Output and PV

  INT status = 0;
  char str[256];
  *pvalue = ss_nan();
  memset(str, 0,sizeof(str)-1);


  //INT PID_VERSION=1;  //## FIXME
  //if (PID_VERSION > 1) {
  status = BD_PUTS("g\n");
  if (status < 0) {
    std::cerr << "BD_PUTS error." << std::endl;
    al_trigger_alarm("FanPID", "Fan PID communication failure with Arduino.", 0, "BD_PUTS returns < 0", AT_INTERNAL);
    return FE_ERR_HW;
  }
  //}

  status = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);  // EXTRA dummy read to readback "echo?"
  ss_sleep(50);
  status = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); 
  if (status <= 0) {
    std::cerr << "BD_GETS error." << std::endl;
    //al_trigger_alarm("FanPID", "Fan PID communication failure with Arduino.", 0, "BD_GETS returns < 0", AT_INTERNAL);
  }

//   printf("``%s''\n", str);

//   std::string s(str);
//   size_t pos = s.find("\r\n");
//   if (pos != std::string::npos) {
//     s.replace(pos,2,"\\r\\n");
//   }
//   printf("``%s''\n", s.c_str());

//   float pv = atof(s.c_str());
//   pos = s.find("\t");
//   float co = atof(s.c_str() + pos + 1);
  
  float pv=0,co=0;
  if ( sscanf(str,"%f\t%f",&pv,&co) != 2) {
    std::cerr << "Error: Failed to parse data from Arduino." << std::endl;
    std::cerr << "received: ``" << str << "''" << std::endl;
//    al_trigger_alarm("FanPID", "Fan PID communication failure with Arduino.", 0, "Failed to parse received data", AT_INTERNAL); // TODO: re-enable this...
    return FE_ERR_HW;
  }

   printf("PV=%.2f  CO=%.2f\n", pv, co);

  switch (channel) {
    case CHAN_INP_PV:
      *pvalue = pv;
      info->process_value = pv;
      break;
    case CHAN_INP_CO:
      *pvalue = co;
      info->controlled_output = co;
      break;
    default:
      *pvalue = ss_nan();
      break;
  }

  //ss_sleep(1);
  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_arduino_fan_pid_get_label(DD_FAN_PID_INFO * info, INT channel, char *name)
{

  // Keep track of calls to this function to get right channel labels with multi.c class driver.
  if (info->num_channels==2 && info->get_label_calls > info->num_channels/2) {
    channel+=info->num_channels;
  }

  switch (channel) {
    case CHAN_INP_PV:
      strncpy(name, "Process Value degC", NAME_LENGTH-1);
      break;
    case CHAN_INP_CO:
      strncpy(name, "Controlled Output", NAME_LENGTH-1);
      break;
    case CHAN_OUT_SP:
      strncpy(name, "Setpoint degC", NAME_LENGTH-1);
      break;
    case CHAN_OUT_RE:
      strncpy(name, "reserved", NAME_LENGTH-1);
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

INT dd_arduino_fan_pid(INT cmd, ...)
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
      status = dd_arduino_fan_pid_init(hKey, (void**)info, channel, (INT (*)(INT, ...)) bd);
      break;

    case CMD_EXIT:
      info = va_arg(argptr, void *);
      status = dd_arduino_fan_pid_exit((DD_FAN_PID_INFO*) info);
      break;

    case CMD_SET:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      value = (float) va_arg(argptr, double);
      status = dd_arduino_fan_pid_set((DD_FAN_PID_INFO*) info, channel, value);
      break;

    case CMD_GET:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      pvalue = va_arg(argptr, float *);
      status = dd_arduino_fan_pid_get((DD_FAN_PID_INFO*) info, channel, pvalue);
      break;

    case CMD_GET_LABEL:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      name = va_arg(argptr, char *);
      status = dd_arduino_fan_pid_get_label((DD_FAN_PID_INFO*) info, channel, name);
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


