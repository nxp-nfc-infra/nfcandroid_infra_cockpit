@echo off
title PN722x Android Server Application

REM Backup original Android middle HAL.
set filename=/vendor/lib64/nfc_nci_nxp_pn72xx.so

REM Check if device is available and root
echo Waiting for devices
adb wait-for-device
adb root

REM Check if device is available and remount
adb wait-for-device
adb remount

REM Push the UcBal Android MW Server Binary
adb logcat -c
adb push pnscr_nfc /system/bin/pnscr

REM Backup original Android middle HAL and kill it
adb shell mv %filename% %filename%.backup
adb shell killall android.hardware.nfc_pn72xx@1.2-service
adb shell chmod 0777 /system/bin/pnscr
echo^.

REM Start Android MW Server
echo Server is running. Client can request for connectation.....
echo^.
echo Note [To stop the server Ctrl+C]
adb shell ./system/bin/pnscr false
echo^.
echo^.
echo **********************************************************
echo Mandatory!
echo Step to restore the original Android MW HAL (NFC HAL)
echo **********************************************************
echo Once you are done with Cockpit tool.
echo    1. Kill the pnscr by pressing Ctrl+C
echo    2. run restore_NFC_HAL.bat (to restore the NFC service)
echo **********************************************************
echo^.
echo^.
pause