###################################################################################################
#                           Android Server Cockpit Utility:                                       #
#             This utilite will dump the EEPROM and protocol data into NFCC                       #
###################################################################################################
  Folder Name: Android_Cockpit_server_pkg
  Contents:
   o    pnscr_nfc                                      -> Binary
   o    restore_NFC_HAL.bat                            -> Script
   o    PN722x_UcBalAndroid_LogDisabled.bat            -> Script (Debug print disable )
   o    PN722x_UcBalAndroid_LogEnabled.bat             -> Script (Debug print enabled )
   o    README.txt                                     -> How to run the android server

  Pre-requisite :
   o    adb driver must be installed.
   o    verify adb devices
   o    NFC should up and running

  Step to run the android server :
  1. run the PN722x_UcBalAndroid_LogDisabled.bat. it will start the server and keep listening
     for client (windows Cockpit tool).
  2. Once client will be trigger it will connect with client and ready for cmd exchange.

  Note : If you want server's debug print then please run PN722x_UcBalAndroid_LogEnabled.bat
         script and follow the steps mentioned on the console.

  Step to restore the NFC :
  1. Once you are done with Cockpit. kill the runing script and run restore_NFC_HAL.bat script.
     it will restore the previous HAL.

##########################################################################################################
