@echo off
set filename=/vendor/lib64/nfc_nci_nxp_pn72xx.so

adb wait-for-device
adb root
adb wait-for-device
adb remount

:: Restore the hal lib
adb shell mv %filename%.backup %filename%

pause
