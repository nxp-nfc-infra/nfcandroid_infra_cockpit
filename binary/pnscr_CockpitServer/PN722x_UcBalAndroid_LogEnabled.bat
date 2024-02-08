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

echo^.

REM Start Android MW Server
echo^.
echo **********************************************************
echo                Steps to start the server
echo **********************************************************
echo Perform the following steps to start Android MW Server
echo    1. cd /system/bin
echo    2. pnscr true/false (true  : Enable Debug info )
echo                        (false : Disable Debug info )
echo    3. This will start the server and keep waiting for client
echo **********************************************************
echo **********************************************************
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
adb wait-for-device
adb root
adb wait-for-device
adb remount
adb shell
pause
