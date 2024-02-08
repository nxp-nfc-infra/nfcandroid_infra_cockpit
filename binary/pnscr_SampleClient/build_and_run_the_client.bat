del Cockpit_Client.exe
gcc cockpit_Client_sample.cpp -lws2_32 -o Cockpit_Client
adb forward tcp:8050 tcp:8059
Cockpit_Client