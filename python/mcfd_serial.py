'''
This program serves as a functional controller to the MCFD-16

Kolby Kiesling
07 / 22 / 2019
'''

# Modules used
import serial # NO LONGER MAINTAINED, find alternative?
import re
import datetime
import time

class MCFD():
  def __init__ (self, dev):
    self.dev = serial.Serial(dev, 9600, timeout=1) # string to hold user device
    return
  
  def readout(self): # debugging to see if we made the program right...
    for i in range(5):
      print self.dev.readline()

  def clear(self):
    #time.sleep(0.25)
    #self.dev.write("\r\n")
    self.readout()
    #time.sleep(0.25)
  
  def set_Polarity(self, channel_pair, val): # pair
     cmd = "sp " + str(channel_pair) + " " + str(val) + "\r\n" # construct the command
     self.dev.write(cmd) 
     self.clear()
  def set_Gain(self, channel_pair, val): # pair
    cmd = "sg " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Bandwidth_Limit(self, val): # single use
    cmd = "bwl " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_CFD(self, val): # single use
    cmd = "cfd " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Threshold(self, channel, val): # all channels
   cmd = "st " + str(channel) + " " + str(val) + "\r\n"
   self.dev.write(cmd)
   self.clear()
  
  def set_Width(self, channel_pair, val): # pairs
    cmd = "sw " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Dead_Time(self, channel_pair, val): # pairs
    cmd = "sd " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Delay_Line(self, channel_pair, val): # pairs
    cmd = "sy " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Fraction(self, channel_pair, val): # pairs
    cmd = "sf " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Coincidence(self, val): # single use
    cmd = "sc " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Trigger_Source(self, trigger, val): # three values
    cmd = "tr " + str(trigger) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Trigger_Monitor(self, monitor, channel):
    cmd = "tm " + str(monitor) + " " + str(channel) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Trigger_Pattern(self, byte_pattern, val):
    cmd = "tp " + str(byte_pattern) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Veto(self, val):
    cmd = "sv " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Gate_Source(self, val):
    cmd = "gs " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Gate_Timing(self, edge, val):
    cmd = "ga " + str(edge) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Multiplicity(self, lower_channel, upper_channel):
    cmd = "sm " + str(lower_channel) + " " + str(upper_channel) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Pairings(self, chnID, val):
    cmd = "pa " + str(chnID) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Mask(self, val): # single use
    cmd = "sk " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Pulser(self, val):
    cmd = "p" + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def clear_pulser(self):
    self.dev.write("p0\r\n")
    self.clear()
  
  def initialize(self): # initialize the MCFD 16
    self.set_Bandwidth_Limit(0) # leave bandwidth limit off...
    self.set_CFD(1)
    self.set_Mask(0) # unmask all registers
    self.set_Coincidence(36) # set the coincidence timeout
    self.set_Veto(0) # turn veto off
    self.set_Gate_Source(0) # set the source as zero
    self.set_Gate_Timing(1, 5) # negative edge (FALLING)
    self.set_Pulser(0) # turn pulser off
    
    trgPat=[1, 4, 18] # OR -> PAIRED COINCIDENCE -> PATTERN & MULTIPLICITY
    trgMntr=[0, 3] # monitor channesl 0 and 3
    mltChn=[0, 3] # multiplicity channels
    
    for i in range(1):
      self.set_Multiplicity(mltChn[i], mltChn[i + 1])
      
    for i in range(2):
      self.set_Trigger_Monitor(i, trgMntr[i])
    
    for i in range(3):
      self.set_Trigger_Source(i, trgPat[i]) # setup the triggers
    
    for i in range(4):
      self.set_Trigger_Pattern(i, 255) # setup the patter for triggering
    
    for i in range(8):
      self.set_Polarity(i, 1) # initialize all polarities to negative
      self.set_Gain(i, 1) # set common gain to 1
      self.set_Width(i, 16) # start out with shortest width
      self.set_Dead_Time(i, 27) # shortest amount of deadtime
      self.set_Delay_Line(i, 1) # shortest delay lines
      self.set_Fraction(i, 40) # use 40% of signal
      
    for i in range(16):
      self.set_Threshold(i, 0) # initialize all thresholds to zero
      self.set_Pairings(i, 255) # global pairing
    return 1
  
  def exit(self):
    self.dev.close()
    return -1