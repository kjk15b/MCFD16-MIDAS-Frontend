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
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include "midas.h"
#undef calloc
using namespace std;


#define DEFAULT_TIMEOUT 1000     // milliseconds
#define SHORT_TIMEOUT 100


#define TRIGGER_0_OUT 16
#define TRIGGER_1_OUT 17
#define TRIGGER_2_OUT 18
#define SUM_OUT 19


#define DD_MCFD_SETTINGS_STR "\
Read Period ms = FLOAT : 200\n\
Register Mask = INT : 0\n\
Channel Threshold = INT[16] :\n\
[0] 0\n\
[1] 0\n\
[2] 0\n\
[3] 0\n\
[4] 0\n\
[5] 0\n\
[6] 0\n\
[7] 0\n\
[8] 0\n\
[9] 0\n\
[10] 0\n\
[11] 0\n\
[12] 0\n\
[13] 0\n\
[14] 0\n\
[15] 0\n\
Trigger Source = INT[3] :\n\
[0] 1\n\
[1] 1\n\
[2] 1\n\
Set Coincidence = INT : 36\n\
"

string removeChar (std::string str) {
    int i=str.length();
    int k=0;
    while (k<=i) {
      //cout << " { " << str[k];
      if ((str[k] > 57 || ((str[k] < 48) && str[k] != 46)) && str[k] != 0){
	//cout << "Found value: 0x" << std::hex << (int) str[k] << std::dec;
	str[k]=0x20;
      }
      //cout << " } ";
      k++;
    }
    cout << endl;
    return str;
}

string removeSpaces (std::string str) {
  str.erase(std::remove(str.begin(), str.end(),' '), str.end());
  str.erase(std::remove(str.begin(), str.end(),'\n'), str.end());
  str.erase(std::remove(str.begin(), str.end(),'\r'), str.end());
  str.erase(std::remove(str.begin(), str.end(),'\t'), str.end());
  str.erase(std::remove(str.begin(), str.end(),'\f'), str.end());
  str.erase(std::remove(str.begin(), str.end(),'\v'), str.end());
  return str;
}

string shiftDigits (std::string str) {
  //cout << "--------------------\n";
  int i=str.length();
  int k=0;
  while (k<=i-3) { // seems to always have 3 initial blank spaces and 3 final blank spaces
    if (str[k]==0)
      str[k]=str[k+3];
    //cout << " { " << str[k] << " } ";
    k++;
  }
  return str; // may have to work on ending condition
}

float cut_string_frq(std::string str) {
  regex chn_header("rate channel [0-9]*: ");
  regex trg_header("trigger rate[0-9]*: ");
  regex sum_header("sum rate : ");
  regex chn_units(" Hz|kHz|MHz");
  regex khz(" kHz");
  regex hz(" Hz");
  regex mhz(" MHz");
  float frq=0, div=1; // frequency place holder and divider to catch units, report in kHz
  bool found_header=false;
  
  smatch m;
  if (regex_search(str, m, chn_header)) { // check for event header
    //cout << "Found channel... " << endl << "Cutting channel..." << endl;
    str=std::regex_replace(str, chn_header, ""); // cut the header
    //cout << str << endl;
    found_header=true;
  }
  else if (regex_search(str, m, trg_header)) {
    //cout << "Found trigger... " << endl << "Cutting trigger..." << endl;
    str=std::regex_replace(str, trg_header, ""); // cut the header
    //cout << str << endl;
    found_header=true;
  }
  else if (regex_search(str, m, sum_header)) {
    //cout << "Found sum rates... " << endl << "Cutting sum rates..." << endl;
    str=std::regex_replace(str, sum_header, ""); // cut the header
    //cout << str << endl;
    found_header=true;
  }
  else
    found_header=false;
  if (found_header==true){
    if (regex_search(str, m, chn_units)) {
      //cout << "Found units... " << endl << "Cutting units..." << endl;
      if (regex_search(str, m, khz))
	div=1000; // in the units we want
      else if (regex_search(str, m, hz))
	div=1; // divide by 1000 to return kHz
      else if (regex_search(str, m, mhz))
	div=1000000; // multiply by a thousand to get the right units
      str=std::regex_replace(str, chn_units, "");
      //str=removeSpaces(str);
      str=removeChar(str);
      //str=shiftDigits(str);
      //str=removeChar(str);
      //cout << "Frequency:\t\t" << frq << "\nString=\t\t{" << str.c_str() << "}" << endl;
      
      frq=::atof(str.c_str());
      //frq=strtof(str.c_str(), 0);
      //cout << "div:\t\t" << div << endl; 
      
      frq=frq*div; // set to kHz
      //cout << "Frequency:\t\t" << frq << "\nString=\t\t{" << str.c_str() << "}" << endl;
      return frq;
    }
  }
  return -1; // does not find rate correctly...
}

float mcfd_get (std::string str) { // fetch our string and concatenate it to pass to cut_string
  smatch m;
  regex cmd_header("ra [0-9]*");
  regex mcfd_ret("mcfd-16>");
  regex chn_header("rate channel [0-9]*: ");
  regex trg_header("trigger rate[0-9]*: ");
  regex sum_header("sum rate : ");
  bool found_event=false;
  
  cout << "\n-------------------\n";
  for (size_t i=0;i<str.length();++i) { // TODO: check for cases where the cmd_header is not present... cut tail...
    if (str[i]=='\n')
      str[i]=0x20;
    if (str[i]=='\r') // remove all characters that cause us problems
      str[i]=0x20;
    //cout << " { " << cpy_str[i] << " } ";
  }
  //cout << "\n--------------------\nReceived:\n\n" << str << "\n--------------------\n";
  if (regex_search(str, m, cmd_header)) {
    //cout << "Found instruction word" << endl;
    str=std::regex_replace(str, cmd_header, ""); // only cut the cmd because cut_string cares about everything else
    //cout << "str=\t\t" << str << endl;
    found_event=true;
  }
  else if (regex_search(str, m, chn_header))
    found_event=true; // we found a channel ID
  else if (regex_search(str, m, trg_header))
    found_event=true; // we found a trigger
  else if (regex_search(str, m, sum_header))
    found_event=true; // we found the sum of rates
  else
    found_event=false; // we could not find anything meaningful
  
  if (found_event) { // If we found a channel or event command, there should be the trailing edge mcfd-16> return
    if (regex_search(str, m, mcfd_ret)) {
	//cout << "Found trailing return" << endl;
	str=std::regex_replace(str, mcfd_ret, "");
	//cout << "str=\t\t" << str << endl;
	return cut_string_frq(str);
      }
  }
    
  return -1; // something did not go right...
}


typedef struct {
  int readPeriod_ms; // maybe need?
  int register_mask;
  int channel_threhold[16];
  int trigger_source[3];
  int set_coincidence;
//   bool manual_control; // maybe...
} DD_MCFD_SETTINGS;


typedef struct {
  DD_MCFD_SETTINGS settings;
  DD_MCFD_SETTINGS settingsIncoming;

  INT num_channels;
  INT(*bd)(INT cmd, ...);      // bus driver entry function
  void *bd_info;               // private info of bus driver
  HNDLE hkey;                  // ODB key for bus driver info


  float *array;                // Most recent measurement or NaN, one for each channel
  DWORD *update_time;          // seconds

  INT get_label_calls;

} DD_MCFD_INFO;


// Should probably call this every time the fe is started.  This would log PID parameters to the midas.log so they can be recovered later...
//int recall_pid_settings(bool saveToODB=false); // read from Arduino, print to messages/stdout, optionally save to ODB

int mcfd_apply_new_setting(char* cmd, DD_MCFD_INFO * info) { // Faster? try not to write to everything after intializing setup...
  char str[256];
  BD_PUTS(cmd); // only need a few BD_PUTS, do not really care about returns in this case because these registers are really
  // write only
  for (int tries=0; tries<15; ++tries) {
    int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
    //if (len==0 && tries > 4) break;
    //printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
  }
  
  return FE_SUCCESS; // TODO: make sure things applied okay...
}


int mcfd_apply_settings(DD_MCFD_INFO* info) {
  /* Format of most commands to R / W from the MCFD:
   * (char) + (int) + (opt. modifier) 
   * Where char is the identifying register, int is the data write and optional modifier is for special cases...
   */
  char cmd[256]; // only can set the pulser right now...
  char str[256];
  memset(cmd, 0, sizeof(cmd));
  // want to write for loops to automate the r / w process...
  
  snprintf(cmd, sizeof(cmd)-1, "sc %d\r\n", info->settings.set_coincidence); // set coincidence timing
  BD_PUTS(cmd); // only need a few BD_PUTS, do not really care about returns in this case because these registers are really
  // write only
  for (int tries=0; tries<15; ++tries) {
    int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
    //if (len==0 && tries > 4) break;
    //printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
  }
  
  snprintf(cmd, sizeof(cmd)-1, "sk %d\r\n", info->settings.register_mask); // set masking
  BD_PUTS(cmd);
  for (int tries=0; tries<15; ++tries) {
    int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
    //if (len==0 && tries > 4) break;
    //printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
  }
  
  for (int i=0;i<16;++i) {
    if (i < 3) {
      snprintf(cmd, sizeof(cmd)-1, "tr %d %d\r\n", i, info->settings.trigger_source[i] ); // set trigger config
      BD_PUTS(cmd);
      for (int tries=0; tries<15; ++tries) {
	int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
	//if (len==0 && tries > 4) break;
	//printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
      }
    }
    snprintf(cmd, sizeof(cmd)-1, "st %d %d\r\n", i, info->settings.channel_threhold[i]); // set thresholds
    BD_PUTS(cmd);
    for (int tries=0; tries<15; ++tries) {
      int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
      //if (len==0 && tries > 4) break;
      //printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
    }
  }
  
  return FE_SUCCESS; // TODO: make sure things applied okay...
}


void mcfd_settings_updated(INT hDB, INT hkey, void* vinfo)
{ // simple routines to iterate through to check for changes in the ODB...
  printf("Settings updated\n");
  string str;
  string crrg="\r\n";

  DD_MCFD_INFO* info = (DD_MCFD_INFO*) vinfo;

  bool changed=false;

  if (info->settingsIncoming.readPeriod_ms != info->settings.readPeriod_ms) {
    std::cout << "   readPeriod_ms changed from ``" << info->settings.readPeriod_ms << "'' to ``" << info->settingsIncoming.readPeriod_ms << "''" << std::endl;
    info->settings.readPeriod_ms = info->settingsIncoming.readPeriod_ms;
    changed=true;
  }

  if (info->settingsIncoming.register_mask != info->settings.register_mask) {
    std::cout << "   Register Mask changed from ``" << info->settings.register_mask << "'' to ``" << info->settingsIncoming.register_mask << "''" << std::endl;
    info->settings.register_mask = info->settingsIncoming.register_mask;
    changed=true;
    str="sk " + std::to_string(info->settings.register_mask) + crrg;
  }  
  
  if (info->settingsIncoming.set_coincidence != info->settings.set_coincidence) {
    std::cout << "   Global Coincidence changed from ``" << info->settings.set_coincidence << "'' to ``" << info->settingsIncoming.set_coincidence << "''" << std::endl;
    info->settings.set_coincidence = info->settingsIncoming.set_coincidence;
    str="sc " +std::to_string(info->settings.set_coincidence) + crrg;
    changed=true;
  }
  
  for (int i=0; i<16; ++i) {
    if (i < 3) {
      if (info->settingsIncoming.trigger_source[i] != info->settings.trigger_source[i]) {
      std::cout << "   Trigger Source " << i << " changed from ``" << info->settings.trigger_source[i] << "'' to ``" << info->settingsIncoming.trigger_source[i] << "''" << std::endl;
      info->settings.trigger_source[i] = info->settingsIncoming.trigger_source[i];
      changed=true;
      str="tr " + std::to_string(i);
      str=str + " " + std::to_string(info->settings.trigger_source[i]); // compiler seems to get angry when I try to do
      str=str + crrg; // everything on the same line
      }
    }
    if (info->settingsIncoming.channel_threhold[i] != info->settings.channel_threhold[i]) {
      std::cout << "   Channel " << i << " Threshold changed from ``" << info->settings.channel_threhold[i] << "'' to ``" << info->settingsIncoming.channel_threhold[i] << "''" << std::endl;
      info->settings.channel_threhold[i] = info->settingsIncoming.channel_threhold[i];
      changed=true;
      str="st " + std::to_string(i);
      str=str + " " + std::to_string(info->settings.channel_threhold[i]); // compiler seems to get angry when I try to do
      str=str + crrg; // everything on the same line
    }
  }
  
  char * cmd = new char[str.size() + 1]; // char to pass to our function later on...
  std::copy(str.begin(), str.end(), cmd);
  cmd[str.size()] = '\0';
  
  if (changed) mcfd_apply_new_setting(cmd, info); // TODO: only apply the changed one...
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
  stringstream input;
  string out_string;
  float read=-1;// default to bad read
  
  
  // TODO: Check to see if MCFD16 is outputing data
  status = BD_PUTS("ra 19\r\n"); // read firmware ID...
  printf("BD_PUTS status = %d\n", status);
  ss_sleep(50);
  int tries=0;
  for (tries=0; tries<15; ++tries) {
    int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
    //if (len==0 && tries > 4) break;
    input << str;
    //printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
  }
  out_string=input.str(); // collect in string format...
  read=mcfd_get(out_string);
  cout << "BD_GETS Return:\n\n" << read << " Hz" << endl;
  //if (len == 0) {
  //}

  /*
  BD_PUTS("ra 0\r\n");
  len = BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT);  // EXTRA dummy read to readback "echo?"
  //len = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);
  printf("ra 0 response = ``%s''\n",str);

  for (tries=0; tries<500; ++tries) {
    int len = BD_GETS(str, sizeof(str)-1, "\n", 5);
    if (len==0 && tries > 2) break;
  }
  */

  mcfd_apply_settings(info); // settings are probably not functional, but they appear to be getting there...
  //status = info->bd(CMD_EXIT, info->bd_info);
  //printf("...\n%d", status);
  
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
  if (channel < 0 || channel >= info->num_channels) // This function may not be necessary since everything is in
    return FE_ERR_DRIVER; // the settings now...

  channel+=info->num_channels;

  char cmd[256];
  char str[256];
  memset(cmd, 0, sizeof(cmd));

  printf("Set channel %d to %d\n", channel, value);
  
  switch (channel) {
    case TRIGGER_0_OUT: // Pulser status
      // TODO: make sure "value" is reasonable
      //info->settings.pulser = value;
      //snprintf(cmd, sizeof(cmd)-1, "p%d\r\n", info->settings.pulser);
      //BD_PUTS(cmd);
      //BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
      //printf("Pulser set to %d\n", info->settings.pulser);
      // TODO: make sure it was applied correctly...
      break;
    case TRIGGER_1_OUT:
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
  //INT chn = channel + 2;
  printf("\n--------------------\nChecking Channel:\t%d\n--------------------\n", channel);
  INT status = 0;
  char str[256], cmd[256];
  string line=""; // have to do everything in terms of strings otherwise regex breaks
  *pvalue = ss_nan();
  memset(str, 0,sizeof(str)-1);
  stringstream input;
  string out_string;
  
  snprintf(cmd, sizeof(cmd)-1, "ra %d\r\n", channel);
  status = BD_PUTS(cmd);
  printf("BD_PUTS status = %d\n", status);
  ss_sleep(50);
  
  float frq=0; // frequency returned from cutting
  int len;
  len = BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT);
  printf("Length:\t%d\n", len);
  
  int tries=0;
  for (tries=0; tries<15; ++tries) {
    int len = BD_GETS(str, sizeof(str), "\n", SHORT_TIMEOUT); // will have to format this for MCFD, will need heavy testing
    //if (len==0 && tries > 4) break;
    //printf("str=%s\t\t\ttry=%d\t\t\tlen=\t\t\t%d\n", str, tries, len);
    input << str;
  }
  
  out_string=input.str(); // collect in string format...
  //cout << " { " << out_string << " } " << "\n { " << out_string.c_str() << " } " << endl;
  frq = mcfd_get(out_string); // try to get the data...
  cout << "Frequency: " << frq << endl;

  switch (channel) {
    case TRIGGER_0_OUT:
      *pvalue = frq;
      //info->chn_0_frq = frq;
      printf("Trigger:\t%d\tFrequency:\t%f\n", channel, frq);
      break;
    case TRIGGER_1_OUT:
      *pvalue = frq;
      //info->chn_1_frq = frq;
      printf("Trigger:\t%d\tFrequency:\t%f\n", channel, frq);
      break;
    case TRIGGER_2_OUT:
      *pvalue = frq;
      printf("Trigger:\t%d\tFrequency:\t%f\n", channel, frq);
      break;
    case SUM_OUT:
      *pvalue = frq;
      //info->chn_1_frq = frq;
      printf("Sum Rates\tFrequency:\t%f\n", frq);
      break;
    default:
      *pvalue = frq;
      break;
  }

  //ss_sleep(1);
  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_get_label(DD_MCFD_INFO * info, INT channel, char *name)
{

  // Keep track of calls to this function to get right channel labels with multi.c class driver.
  //if (info->num_channels==2 && info->get_label_calls > info->num_channels/2) {
    //channel+=info->num_channels;
  //}

  switch (channel) {
    case TRIGGER_0_OUT:
      strncpy(name, "Trigger 0 (Hz)", NAME_LENGTH-1);
      break;
    case TRIGGER_1_OUT:
      strncpy(name, "Trigger 1 (Hz)", NAME_LENGTH-1);
      break;
    case TRIGGER_2_OUT:
      strncpy(name, "Trigger 2 (Hz)", NAME_LENGTH-1);
      break;
    case SUM_OUT:
      strncpy(name, "Sum (Hz)", NAME_LENGTH-1);
      break;
    default:
      memset(name, 0, NAME_LENGTH);
      snprintf(name, NAME_LENGTH-1, "Channel %d Hz", channel);
      //return FE_ERR_DRIVER;
  }

  //printf("dd_arduino_fan_pid_get_label: chan=%d, Name=``%s''\n",channel,name);
  //info->get_label_calls++;
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


