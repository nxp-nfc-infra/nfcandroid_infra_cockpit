:: Copyright 2022-2023 NXP.

:: NXP Confidential. This software is owned or controlled by NXP and may only be used
:: strictly in accordance with the applicable license terms. By expressly accepting such
:: terms or by downloading, installing, activating and/or otherwise using the software,
:: you are agreeing that you have read, and that you agree to comply with and are bound
:: by, such license terms. If you do not agree to be bound by the applicable license
:: terms, then you may not retain,install, activate or otherwise use the software.

@echo on
set filename=/vendor/lib64/nfc_nci_nxp_snxxx.so
set prefix=nfc_nci

adb wait-for-device
adb root
adb wait-for-device
adb remount

:: Backup hal lib
adb shell mv %filename% %filename%.backup
::adb shell mv /vendor/lib64/nfc_nci_nxp_snxxx.so /vendor/lib64/nfc_nci_nxp_snxxx.so.backup

:: Run script 
adb shell killall android.hardware.nfc_snxxx@1.2-service
adb shell killall android.hardware.nfc-service.nxp

adb push ../binary/pnscr_nfc /system/bin/pnscr
adb push sn110_init.txt /data/nfc/
adb shell pnscr -t 1 -d nxp-nci -f /data/nfc/sn110_init.txt
timeout 3
adb push pnscr_t4t_test.txt /data/nfc/
adb shell pnscr -t 1 -d nxp-nci -f /data/nfc/pnscr_t4t_test.txt


:: Restore the hal lib
adb shell mv %filename%.backup %filename%
::adb shell mv /vendor/lib64/nfc_nci_nxp_snxxx.so.backup /vendor/lib64/nfc_nci_nxp_snxxx.so


