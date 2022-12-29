//Once connected to the access point for WiFi Config, the ESP32 sends current setting values in one long string to websocket
//After user clicks 'save', data is validated via main.js and a long string of values is returned.

//Start webserver in AP mode
void startWebServer()
{
#ifdef COMPILE_WIFI
#ifdef COMPILE_AP

  ntripClientStop(true); //Do not allocate new wifiClient
  wifiStartAP();

  incomingSettings = (char*)malloc(AP_CONFIG_SETTING_SIZE);

  //Clear any garbage from settings array
  memset(incomingSettings, 0, AP_CONFIG_SETTING_SIZE);

  webserver = new AsyncWebServer(80);
  websocket = new AsyncWebSocket("/ws");

  websocket->onEvent(onWsEvent);
  webserver->addHandler(websocket);

  // * index.html (not gz'd)
  // * favicon.ico

  // * /src/bootstrap.bundle.min.js - Needed for popper
  // * /src/bootstrap.min.css
  // * /src/bootstrap.min.js
  // * /src/jquery-3.6.0.min.js
  // * /src/main.js (not gz'd)
  // * /src/rtk-setup.png
  // * /src/style.css

  // * /src/fonts/icomoon.eot
  // * /src/fonts/icomoon.svg
  // * /src/fonts/icomoon.ttf
  // * /src/fonts/icomoon.woof

  // * /listfiles responds with a CSV of files and sizes in root
  // * /file allows the download or deletion of a file

  webserver->onNotFound(notFound);

  webserver->onFileUpload(handleUpload); // Run handleUpload function when any file is uploaded. Must be before server.on() calls.

  webserver->on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  webserver->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", favicon_ico, sizeof(favicon_ico));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", bootstrap_bundle_min_js, sizeof(bootstrap_bundle_min_js));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", bootstrap_min_css, sizeof(bootstrap_min_css));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", bootstrap_min_js, sizeof(bootstrap_min_js));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/jquery-3.6.0.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", jquery_js, sizeof(jquery_js));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/main.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/javascript", main_js);
  });

  webserver->on("/src/rtk-setup.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", rtkSetup_png, sizeof(rtkSetup_png));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", style_css, sizeof(style_css));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/fonts/icomoon.eot", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", icomoon_eot, sizeof(icomoon_eot));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/fonts/icomoon.svg", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", icomoon_svg, sizeof(icomoon_svg));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/fonts/icomoon.ttf", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", icomoon_ttf, sizeof(icomoon_ttf));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver->on("/src/fonts/icomoon.woof", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", icomoon_woof, sizeof(icomoon_woof));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //Handler for the /upload form POST
  webserver->on("/upload", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200);
  }, handleFirmwareFileUpload);

  //Handlers for file manager
  webserver->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    systemPrintln(logmessage);
    request->send(200, "text/plain", getFileList());
  });

  webserver->on("/file", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    //This section does not tolerate semaphore transactions

    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (request->hasParam("name") && request->hasParam("action"))
    {
      const char *fileName = request->getParam("name")->value().c_str();
      const char *fileAction = request->getParam("action")->value().c_str();

      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + String(fileName) + "&action=" + String(fileAction);

      if (sd->exists(fileName) == false)
      {
        systemPrintln(logmessage + " ERROR: file does not exist");
        request->send(400, "text/plain", "ERROR: file does not exist");
      }
      else
      {
        systemPrintln(logmessage + " file exists");

        if (strcmp(fileAction, "download") == 0)
        {
          logmessage += " downloaded";

          if (managerFileOpen == false)
          {
            if (managerTempFile.open(fileName, O_READ) == true)
              managerFileOpen = true;
            else
              systemPrintln("Error: File Manager failed to open file");
          }
          else
          {
            //File is already in use. Wait your turn.
            request->send(202, "text/plain", "ERROR: File already downloading");
            //return (0);
          }

          int dataAvailable = managerTempFile.size() - managerTempFile.position();

          AsyncWebServerResponse *response = request->beginResponse("text/plain", dataAvailable,
                                             [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t
          {
            uint32_t bytes = 0;
            uint32_t availableBytes = managerTempFile.available();

            if (availableBytes > maxLen)
            {
              bytes = managerTempFile.read(buffer, maxLen);
            }
            else
            {
              bytes = managerTempFile.read(buffer, availableBytes);
              managerFileOpen = false;
              managerTempFile.close();

              //xSemaphoreGive(sdCardSemaphore);

              //systemPrintln("Send me more");
              websocket->textAll("fmNext,1,"); //Tell browser to send next file if needed
            }

            return bytes;
          });

          response->addHeader("Cache-Control", "no-cache");
          response->addHeader("Content-Disposition", "attachment; filename=" + String(fileName));
          response->addHeader("Access-Control-Allow-Origin", "*");
          request->send(response);
        }
        else if (strcmp(fileAction, "delete") == 0)
        {
          logmessage += " deleted";
          sd->remove(fileName);
          request->send(200, "text/plain", "Deleted File: " + String(fileName));
        }
        else
        {
          logmessage += " ERROR: invalid action param supplied";
          request->send(400, "text/plain", "ERROR: invalid action param supplied");
        }
        systemPrintln(logmessage);
      }
    }
    else
    {
      request->send(400, "text/plain", "ERROR: name and action params required");
    }
  });

  webserver->begin();

  //Pre-load settings CSV
  settingsCSV = (char*)malloc(AP_CONFIG_SETTING_SIZE);
  createSettingsString(settingsCSV);

  log_d("Web Server Started");
  reportHeapNow();

#endif //COMPILE_AP

  wifiSetState(WIFI_NOTCONNECTED);
#endif //COMPILE_WIFI

}

void stopWebServer()
{
#ifdef COMPILE_WIFI
#ifdef COMPILE_AP

  if (webserver != NULL)
  {
    webserver->end();

    free(settingsCSV);
    free(websocket);
    free(webserver);

    webserver = NULL;
  }

  log_d("Web Server Stopped");
  reportHeapNow();

#endif
#endif
}

#ifdef COMPILE_WIFI
#ifdef COMPILE_AP
void notFound(AsyncWebServerRequest *request) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  systemPrintln(logmessage);
  request->send(404, "text/plain", "Not found");
}
#endif
#endif

//Handler for firmware file upload
#ifdef COMPILE_WIFI
#ifdef COMPILE_AP
static void handleFirmwareFileUpload(AsyncWebServerRequest * request, String fileName, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    //Check file name against valid firmware names
    const char* BIN_EXT = "bin";
    const char* BIN_HEADER = "RTK_Surveyor_Firmware";

    char fname[50]; //Handle long file names
    fileName.toCharArray(fname, sizeof(fname));
    fname[fileName.length()] = '\0'; //Terminate array

    //Check 'bin' extension
    if (strcmp(BIN_EXT, &fname[strlen(fname) - strlen(BIN_EXT)]) == 0)
    {
      //Check for 'RTK_Surveyor_Firmware' start of file name
      if (strncmp(fname, BIN_HEADER, strlen(BIN_HEADER)) == 0)
      {
        //Begin update process
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
          Update.printError(Serial);
          return request->send(400, "text/plain", "OTA could not begin");
        }
      }
      else
      {
        systemPrintf("Unknown: %s\r\n", fname);
        return request->send(400, "text/html", "<b>Error:</b> Unknown file type");
      }
    }
    else
    {
      systemPrintf("Unknown: %s\r\n", fname);
      return request->send(400, "text/html", "<b>Error:</b> Unknown file type");
    }
  }

  // Write chunked data to the free sketch space
  if (len)
  {
    if (Update.write(data, len) != len)
      return request->send(400, "text/plain", "OTA could not begin");
    else
    {
      binBytesSent += len;

      //Send an update to browser every 100k
      if (binBytesSent - binBytesLastUpdate > 100000)
      {
        binBytesLastUpdate = binBytesSent;

        char bytesSentMsg[100];
        sprintf(bytesSentMsg, "%'d bytes sent", binBytesSent);

        systemPrintf("bytesSentMsg: %s\r\n", bytesSentMsg);

        char statusMsg[200] = {'\0'};
        stringRecord(statusMsg, "firmwareUploadStatus", bytesSentMsg); //Convert to "firmwareUploadMsg,11214 bytes sent,"

        systemPrintf("msg: %s\r\n", statusMsg);
        websocket->textAll(statusMsg);
      }

    }
  }

  if (final)
  {
    if (!Update.end(true))
    {
      Update.printError(Serial);
      return request->send(400, "text/plain", "Could not end OTA");
    }
    else
    {
      websocket->textAll("firmwareUploadComplete,1,");
      systemPrintln("Firmware update complete. Restarting");
      delay(500);
      ESP.restart();
    }
  }
}
#endif
#endif

//Events triggered by web sockets
#ifdef COMPILE_WIFI
#ifdef COMPILE_AP

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT) {
    client->text(settingsCSV);
    wifiSetState(WIFI_CONNECTED);

    lastCoordinateUpdate = millis();
  }
  else if (type == WS_EVT_DISCONNECT) {
    log_d("Websocket client disconnected");
    wifiSetState(WIFI_NOTCONNECTED);

    //User has either refreshed the page or disconnected. Recompile the current settings.
    createSettingsString(settingsCSV);
  }
  else if (type == WS_EVT_DATA) {
    for (int i = 0; i < len; i++) {
      incomingSettings[incomingSettingsSpot++] = data[i];
      incomingSettingsSpot %= AP_CONFIG_SETTING_SIZE;
    }
    timeSinceLastIncomingSetting = millis();
  }
}
#endif
#endif

//Create a csv string with current settings
void createSettingsString(char* settingsCSV)
{
#ifdef COMPILE_AP
  char tagText[32];
  char nameText[64];

  settingsCSV[0] = '\0'; //Erase current settings string

  //System Info
  stringRecord(settingsCSV, "platformPrefix", platformPrefix);

  char apRtkFirmwareVersion[86];
  sprintf(apRtkFirmwareVersion, "RTK %s Firmware: v%d.%d-%s", platformPrefix, FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, __DATE__);
  stringRecord(settingsCSV, "rtkFirmwareVersion", apRtkFirmwareVersion);

  char apZedPlatform[50];
  if (zedModuleType == PLATFORM_F9P)
    strcpy(apZedPlatform, "ZED-F9P");
  else if (zedModuleType == PLATFORM_F9R)
    strcpy(apZedPlatform, "ZED-F9R");

  char apZedFirmwareVersion[80];
  sprintf(apZedFirmwareVersion, "%s Firmware: %s", apZedPlatform, zedFirmwareVersion);
  stringRecord(settingsCSV, "zedFirmwareVersion", apZedFirmwareVersion);

  char apDeviceBTID[30];
  sprintf(apDeviceBTID, "Device Bluetooth ID: %02X%02X", btMACAddress[4], btMACAddress[5]);
  stringRecord(settingsCSV, "deviceBTID", apDeviceBTID);

  //GNSS Config
  stringRecord(settingsCSV, "measurementRateHz", 1000.0 / settings.measurementRate, 2); //2 = decimals to print
  stringRecord(settingsCSV, "dynamicModel", settings.dynamicModel);
  stringRecord(settingsCSV, "ubxConstellationsGPS", settings.ubxConstellations[0].enabled); //GPS
  stringRecord(settingsCSV, "ubxConstellationsSBAS", settings.ubxConstellations[1].enabled); //SBAS
  stringRecord(settingsCSV, "ubxConstellationsGalileo", settings.ubxConstellations[2].enabled); //Galileo
  stringRecord(settingsCSV, "ubxConstellationsBeiDou", settings.ubxConstellations[3].enabled); //BeiDou
  stringRecord(settingsCSV, "ubxConstellationsGLONASS", settings.ubxConstellations[5].enabled); //GLONASS
  for (int x = 0 ; x < MAX_UBX_MSG ; x++)
    stringRecord(settingsCSV, settings.ubxMessages[x].msgTextName, settings.ubxMessages[x].msgRate);

  //Base Config
  stringRecord(settingsCSV, "baseTypeSurveyIn", !settings.fixedBase);
  stringRecord(settingsCSV, "baseTypeFixed", settings.fixedBase);
  stringRecord(settingsCSV, "observationSeconds", settings.observationSeconds);
  stringRecord(settingsCSV, "observationPositionAccuracy", settings.observationPositionAccuracy, 2);

  if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
  {
    stringRecord(settingsCSV, "fixedBaseCoordinateTypeECEF", true);
    stringRecord(settingsCSV, "fixedBaseCoordinateTypeGeo", false);
  }
  else
  {
    stringRecord(settingsCSV, "fixedBaseCoordinateTypeECEF", false);
    stringRecord(settingsCSV, "fixedBaseCoordinateTypeGeo", true);
  }

  stringRecord(settingsCSV, "fixedEcefX", settings.fixedEcefX, 3);
  stringRecord(settingsCSV, "fixedEcefY", settings.fixedEcefY, 3);
  stringRecord(settingsCSV, "fixedEcefZ", settings.fixedEcefZ, 3);
  stringRecord(settingsCSV, "fixedLat", settings.fixedLat, haeNumberOfDecimals);
  stringRecord(settingsCSV, "fixedLong", settings.fixedLong, haeNumberOfDecimals);
  stringRecord(settingsCSV, "fixedAltitude", settings.fixedAltitude, 4);

  stringRecord(settingsCSV, "enableNtripServer", settings.enableNtripServer);
  stringRecord(settingsCSV, "ntripServer_CasterHost", settings.ntripServer_CasterHost);
  stringRecord(settingsCSV, "ntripServer_CasterPort", settings.ntripServer_CasterPort);
  stringRecord(settingsCSV, "ntripServer_CasterUser", settings.ntripServer_CasterUser);
  stringRecord(settingsCSV, "ntripServer_CasterUserPW", settings.ntripServer_CasterUserPW);
  stringRecord(settingsCSV, "ntripServer_MountPoint", settings.ntripServer_MountPoint);
  stringRecord(settingsCSV, "ntripServer_MountPointPW", settings.ntripServer_MountPointPW);

  stringRecord(settingsCSV, "enableNtripClient", settings.enableNtripClient);
  stringRecord(settingsCSV, "ntripClient_CasterHost", settings.ntripClient_CasterHost);
  stringRecord(settingsCSV, "ntripClient_CasterPort", settings.ntripClient_CasterPort);
  stringRecord(settingsCSV, "ntripClient_CasterUser", settings.ntripClient_CasterUser);
  stringRecord(settingsCSV, "ntripClient_CasterUserPW", settings.ntripClient_CasterUserPW);
  stringRecord(settingsCSV, "ntripClient_MountPoint", settings.ntripClient_MountPoint);
  stringRecord(settingsCSV, "ntripClient_MountPointPW", settings.ntripClient_MountPointPW);
  stringRecord(settingsCSV, "ntripClient_TransmitGGA", settings.ntripClient_TransmitGGA);

  //Sensor Fusion Config
  stringRecord(settingsCSV, "enableSensorFusion", settings.enableSensorFusion);
  stringRecord(settingsCSV, "autoIMUmountAlignment", settings.autoIMUmountAlignment);

  //System Config
  stringRecord(settingsCSV, "enableLogging", settings.enableLogging);
  stringRecord(settingsCSV, "maxLogTime_minutes", settings.maxLogTime_minutes);
  stringRecord(settingsCSV, "maxLogLength_minutes", settings.maxLogLength_minutes);

  stringRecord(settingsCSV, "sdFreeSpace", stringHumanReadableSize(sdFreeSpace));
  stringRecord(settingsCSV, "sdUsedSpace", stringHumanReadableSize(sdCardSize - sdFreeSpace));

  stringRecord(settingsCSV, "enableResetDisplay", settings.enableResetDisplay);

  //Turn on SD display block last
  stringRecord(settingsCSV, "sdMounted", online.microSD);

  //Port Config
  stringRecord(settingsCSV, "dataPortBaud", settings.dataPortBaud);
  stringRecord(settingsCSV, "radioPortBaud", settings.radioPortBaud);
  stringRecord(settingsCSV, "dataPortChannel", settings.dataPortChannel);

  //L-Band
  char hardwareID[13];
  sprintf(hardwareID, "%02X%02X%02X%02X%02X%02X", lbandMACAddress[0], lbandMACAddress[1], lbandMACAddress[2], lbandMACAddress[3], lbandMACAddress[4], lbandMACAddress[5]); //Get ready for JSON
  stringRecord(settingsCSV, "hardwareID", hardwareID);

  char apDaysRemaining[20];
  if (strlen(settings.pointPerfectCurrentKey) > 0)
  {
#ifdef COMPILE_L_BAND
    uint8_t daysRemaining = daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);
    sprintf(apDaysRemaining, "%d", daysRemaining);
#endif
  }
  else
    sprintf(apDaysRemaining, "No Keys");

  stringRecord(settingsCSV, "daysRemaining", apDaysRemaining);

  stringRecord(settingsCSV, "pointPerfectDeviceProfileToken", settings.pointPerfectDeviceProfileToken);
  stringRecord(settingsCSV, "enablePointPerfectCorrections", settings.enablePointPerfectCorrections);
  stringRecord(settingsCSV, "autoKeyRenewal", settings.autoKeyRenewal);

  //External PPS/Triggers
  stringRecord(settingsCSV, "enableExternalPulse", settings.enableExternalPulse);
  stringRecord(settingsCSV, "externalPulseTimeBetweenPulse_us", settings.externalPulseTimeBetweenPulse_us);
  stringRecord(settingsCSV, "externalPulseLength_us", settings.externalPulseLength_us);
  stringRecord(settingsCSV, "externalPulsePolarity", settings.externalPulsePolarity);
  stringRecord(settingsCSV, "enableExternalHardwareEventLogging", settings.enableExternalHardwareEventLogging);

  //Profiles
  stringRecord(settingsCSV, "profileName", profileNames[profileNumber]); //Must come before profile number so AP config page JS has name before number
  stringRecord(settingsCSV, "profileNumber", profileNumber);
  for (int index = 0; index < MAX_PROFILE_COUNT; index++)
  {
    sprintf(tagText, "profile%dName", index);
    sprintf(nameText, "%d: %s", index + 1, profileNames[index]);
    stringRecord(settingsCSV, tagText, nameText);
  }
  //stringRecord(settingsCSV, "activeProfiles", activeProfiles);

  //System state at power on. Convert various system states to either Rover or Base.
  int lastState = 0; //0 = Rover, 1 = Base
  if (settings.lastState >= STATE_BASE_NOT_STARTED && settings.lastState <= STATE_BASE_FIXED_TRANSMITTING) lastState = 1;
  stringRecord(settingsCSV, "baseRoverSetup", lastState);

  //Bluetooth radio type
  stringRecord(settingsCSV, "bluetoothRadioType", settings.bluetoothRadioType);

  //Current coordinates come from HPPOSLLH call back
  stringRecord(settingsCSV, "geodeticLat", latitude, haeNumberOfDecimals);
  stringRecord(settingsCSV, "geodeticLon", longitude, haeNumberOfDecimals);
  stringRecord(settingsCSV, "geodeticAlt", altitude, 3);

  double ecefX = 0;
  double ecefY = 0;
  double ecefZ = 0;

  geodeticToEcef(latitude, longitude, altitude, &ecefX, &ecefY, &ecefZ);

  stringRecord(settingsCSV, "ecefX", ecefX, 3);
  stringRecord(settingsCSV, "ecefY", ecefY, 3);
  stringRecord(settingsCSV, "ecefZ", ecefZ, 3);

  //Antenna height and ARP
  stringRecord(settingsCSV, "antennaHeight", settings.antennaHeight);
  stringRecord(settingsCSV, "antennaReferencePoint", settings.antennaReferencePoint, 1);

  //Radio / ESP-Now settings
  char radioMAC[18];   //Send radio MAC
  sprintf(radioMAC, "%02X:%02X:%02X:%02X:%02X:%02X",
          wifiMACAddress[0],
          wifiMACAddress[1],
          wifiMACAddress[2],
          wifiMACAddress[3],
          wifiMACAddress[4],
          wifiMACAddress[5]
         );
  stringRecord(settingsCSV, "radioMAC", radioMAC);
  stringRecord(settingsCSV, "radioType", settings.radioType);
  stringRecord(settingsCSV, "espnowPeerCount", settings.espnowPeerCount);
  for (int index = 0; index < settings.espnowPeerCount; index++)
  {
    sprintf(tagText, "peerMAC%d", index);
    sprintf(nameText, "%02X:%02X:%02X:%02X:%02X:%02X",
            settings.espnowPeers[index][0],
            settings.espnowPeers[index][1],
            settings.espnowPeers[index][2],
            settings.espnowPeers[index][3],
            settings.espnowPeers[index][4],
            settings.espnowPeers[index][5]
           );
    stringRecord(settingsCSV, tagText, nameText);
  }
  stringRecord(settingsCSV, "espnowBroadcast", settings.espnowBroadcast);

  //Add ECEF and Geodetic station data
  for (int index = 0; index < MAX_STATIONS ; index++) //Arbitrary 50 station limit
  {
    //stationInfo example: LocationA,-1280206.568,-4716804.403,4086665.484
    char stationInfo[100];

    //Try SD, then LFS
    if (getFileLineSD(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) == true) //fileName, lineNumber, array, arraySize
    {
      trim(stationInfo); //Remove trailing whitespace
      //log_d("ECEF SD station %d - found: %s", index, stationInfo);
      replaceCharacter(stationInfo, ',', ' '); //Change all , to ' ' for easier parsing on the JS side
      sprintf(tagText, "stationECEF%d", index);
      stringRecord(settingsCSV, tagText, stationInfo);
    }
    else if (getFileLineLFS(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) == true) //fileName, lineNumber, array, arraySize
    {
      trim(stationInfo); //Remove trailing whitespace
      //log_d("ECEF LFS station %d - found: %s", index, stationInfo);
      replaceCharacter(stationInfo, ',', ' '); //Change all , to ' ' for easier parsing on the JS side
      sprintf(tagText, "stationECEF%d", index);
      stringRecord(settingsCSV, tagText, stationInfo);
    }
    else
    {
      //We could not find this line
      break;
    }
  }

  for (int index = 0; index < MAX_STATIONS ; index++) //Arbitrary 50 station limit
  {
    //stationInfo example: LocationA,40.09029479,-105.18505761,1560.089
    char stationInfo[100];

    //Try SD, then LFS
    if (getFileLineSD(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) == true) //fileName, lineNumber, array, arraySize
    {
      trim(stationInfo); //Remove trailing whitespace
      //log_d("Geo SD station %d - found: %s", index, stationInfo);
      replaceCharacter(stationInfo, ',', ' '); //Change all , to ' ' for easier parsing on the JS side
      sprintf(tagText, "stationGeodetic%d", index);
      stringRecord(settingsCSV, tagText, stationInfo);
    }
    else if (getFileLineLFS(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) == true) //fileName, lineNumber, array, arraySize
    {
      trim(stationInfo); //Remove trailing whitespace
      //log_d("Geo LFS station %d - found: %s", index, stationInfo);
      replaceCharacter(stationInfo, ',', ' '); //Change all , to ' ' for easier parsing on the JS side
      sprintf(tagText, "stationGeodetic%d", index);
      stringRecord(settingsCSV, tagText, stationInfo);
    }
    else
    {
      //We could not find this line
      break;
    }
  }

  //Add WiFi credential table
  for (int x = 0 ; x < MAX_WIFI_NETWORKS ; x++)
  {
    sprintf(tagText, "wifiNetwork%dSSID", x);
    stringRecord(settingsCSV, tagText, settings.wifiNetworks[x].ssid);

    sprintf(tagText, "wifiNetwork%dPassword", x);
    stringRecord(settingsCSV, tagText, settings.wifiNetworks[x].password);
  }
  stringRecord(settingsCSV, "wifiConfigOverAP", settings.wifiConfigOverAP);

  //New settings not yet integrated
  //...

  strcat(settingsCSV, "\0");
  systemPrintf("settingsCSV len: %d\r\n", strlen(settingsCSV));
  systemPrintf("settingsCSV: %s\r\n", settingsCSV);
#endif
}

//Create a csv string with the current coordinates
void createCoordinateString(char* settingsCSV)
{
#ifdef COMPILE_AP
  settingsCSV[0] = '\0'; //Erase current settings string

  //Current coordinates come from HPPOSLLH call back
  stringRecord(settingsCSV, "geodeticLat", latitude, haeNumberOfDecimals);
  stringRecord(settingsCSV, "geodeticLon", longitude, haeNumberOfDecimals);
  stringRecord(settingsCSV, "geodeticAlt", altitude, 3);

  double ecefX = 0;
  double ecefY = 0;
  double ecefZ = 0;

  geodeticToEcef(latitude, longitude, altitude, &ecefX, &ecefY, &ecefZ);

  stringRecord(settingsCSV, "ecefX", ecefX, 3);
  stringRecord(settingsCSV, "ecefY", ecefY, 3);
  stringRecord(settingsCSV, "ecefZ", ecefZ, 3);

  strcat(settingsCSV, "\0");
#endif
}

//Given a settingName, and string value, update a given setting
void updateSettingWithValue(const char *settingName, const char* settingValueStr)
{
#ifdef COMPILE_AP
  char* ptr;
  double settingValue = strtod(settingValueStr, &ptr);

  bool settingValueBool = false;
  if (strcmp(settingValueStr, "true") == 0) settingValueBool = true;

  if (strcmp(settingName, "maxLogTime_minutes") == 0)
    settings.maxLogTime_minutes = settingValue;
  else if (strcmp(settingName, "maxLogLength_minutes") == 0)
    settings.maxLogLength_minutes = settingValue;
  else if (strcmp(settingName, "measurementRateHz") == 0)
  {
    settings.measurementRate = (int)(1000.0 / settingValue);

    //This is one of the first settings to be received. If seen, remove the station files.
    removeFile(stationCoordinateECEFFileName);
    removeFile(stationCoordinateGeodeticFileName);
    log_d("Station coordinate files removed");
  }
  else if (strcmp(settingName, "dynamicModel") == 0)
    settings.dynamicModel = settingValue;
  else if (strcmp(settingName, "baseTypeFixed") == 0)
    settings.fixedBase = settingValueBool;
  else if (strcmp(settingName, "observationSeconds") == 0)
    settings.observationSeconds = settingValue;
  else if (strcmp(settingName, "observationPositionAccuracy") == 0)
    settings.observationPositionAccuracy = settingValue;
  else if (strcmp(settingName, "fixedBaseCoordinateTypeECEF") == 0)
    settings.fixedBaseCoordinateType = !settingValueBool; //When ECEF is true, fixedBaseCoordinateType = 0 (COORD_TYPE_ECEF)
  else if (strcmp(settingName, "fixedEcefX") == 0)
    settings.fixedEcefX = settingValue;
  else if (strcmp(settingName, "fixedEcefY") == 0)
    settings.fixedEcefY = settingValue;
  else if (strcmp(settingName, "fixedEcefZ") == 0)
    settings.fixedEcefZ = settingValue;
  else if (strcmp(settingName, "fixedLat") == 0)
    settings.fixedLat = settingValue;
  else if (strcmp(settingName, "fixedLong") == 0)
    settings.fixedLong = settingValue;
  else if (strcmp(settingName, "fixedAltitude") == 0)
    settings.fixedAltitude = settingValue;
  else if (strcmp(settingName, "dataPortBaud") == 0)
    settings.dataPortBaud = settingValue;
  else if (strcmp(settingName, "radioPortBaud") == 0)
    settings.radioPortBaud = settingValue;
  else if (strcmp(settingName, "enableLogging") == 0)
    settings.enableLogging = settingValueBool;
  else if (strcmp(settingName, "dataPortChannel") == 0)
    settings.dataPortChannel = (muxConnectionType_e)settingValue;
  else if (strcmp(settingName, "autoIMUmountAlignment") == 0)
    settings.autoIMUmountAlignment = settingValueBool;
  else if (strcmp(settingName, "enableSensorFusion") == 0)
    settings.enableSensorFusion = settingValueBool;
  else if (strcmp(settingName, "enableResetDisplay") == 0)
    settings.enableResetDisplay = settingValueBool;

  else if (strcmp(settingName, "enableExternalPulse") == 0)
    settings.enableExternalPulse = settingValueBool;
  else if (strcmp(settingName, "externalPulseTimeBetweenPulse_us") == 0)
    settings.externalPulseTimeBetweenPulse_us = settingValue;
  else if (strcmp(settingName, "externalPulseLength_us") == 0)
    settings.externalPulseLength_us = settingValue;
  else if (strcmp(settingName, "externalPulsePolarity") == 0)
    settings.externalPulsePolarity = (pulseEdgeType_e)settingValue;
  else if (strcmp(settingName, "enableExternalHardwareEventLogging") == 0)
    settings.enableExternalHardwareEventLogging = settingValueBool;
  else if (strcmp(settingName, "profileName") == 0)
  {
    strcpy(settings.profileName, settingValueStr);
    setProfileName(profileNumber);
  }
  else if (strcmp(settingName, "enableNtripServer") == 0)
    settings.enableNtripServer = settingValueBool;
  else if (strcmp(settingName, "ntripServer_CasterHost") == 0)
    strcpy(settings.ntripServer_CasterHost, settingValueStr);
  else if (strcmp(settingName, "ntripServer_CasterPort") == 0)
    settings.ntripServer_CasterPort = settingValue;
  else if (strcmp(settingName, "ntripServer_CasterUser") == 0)
    strcpy(settings.ntripServer_CasterUser, settingValueStr);
  else if (strcmp(settingName, "ntripServer_CasterUserPW") == 0)
    strcpy(settings.ntripServer_CasterUserPW, settingValueStr);
  else if (strcmp(settingName, "ntripServer_MountPoint") == 0)
    strcpy(settings.ntripServer_MountPoint, settingValueStr);
  else if (strcmp(settingName, "ntripServer_MountPointPW") == 0)
    strcpy(settings.ntripServer_MountPointPW, settingValueStr);

  else if (strcmp(settingName, "enableNtripClient") == 0)
    settings.enableNtripClient = settingValueBool;
  else if (strcmp(settingName, "ntripClient_CasterHost") == 0)
    strcpy(settings.ntripClient_CasterHost, settingValueStr);
  else if (strcmp(settingName, "ntripClient_CasterPort") == 0)
    settings.ntripClient_CasterPort = settingValue;
  else if (strcmp(settingName, "ntripClient_CasterUser") == 0)
    strcpy(settings.ntripClient_CasterUser, settingValueStr);
  else if (strcmp(settingName, "ntripClient_CasterUserPW") == 0)
    strcpy(settings.ntripClient_CasterUserPW, settingValueStr);
  else if (strcmp(settingName, "ntripClient_MountPoint") == 0)
    strcpy(settings.ntripClient_MountPoint, settingValueStr);
  else if (strcmp(settingName, "ntripClient_MountPointPW") == 0)
    strcpy(settings.ntripClient_MountPointPW, settingValueStr);
  else if (strcmp(settingName, "ntripClient_TransmitGGA") == 0)
    settings.ntripClient_TransmitGGA = settingValueBool;
  else if (strcmp(settingName, "serialTimeoutGNSS") == 0)
    settings.serialTimeoutGNSS = settingValue;
  else if (strcmp(settingName, "pointPerfectDeviceProfileToken") == 0)
    strcpy(settings.pointPerfectDeviceProfileToken, settingValueStr);
  else if (strcmp(settingName, "enablePointPerfectCorrections") == 0)
    settings.enablePointPerfectCorrections = settingValueBool;
  else if (strcmp(settingName, "autoKeyRenewal") == 0)
    settings.autoKeyRenewal = settingValueBool;
  else if (strcmp(settingName, "antennaHeight") == 0)
    settings.antennaHeight = settingValue;
  else if (strcmp(settingName, "antennaReferencePoint") == 0)
    settings.antennaReferencePoint = settingValue;
  else if (strcmp(settingName, "bluetoothRadioType") == 0)
    settings.bluetoothRadioType = (BluetoothRadioType_e)settingValue; //0 = SPP, 1 = BLE, 2 = Off
  else if (strcmp(settingName, "espnowBroadcast") == 0)
    settings.espnowBroadcast = settingValueBool;
  else if (strcmp(settingName, "radioType") == 0)
    settings.radioType = (RadioType_e)settingValue; //0 = Radio off, 1 = ESP-Now
  else if (strcmp(settingName, "baseRoverSetup") == 0)
  {
    settings.lastState = STATE_ROVER_NOT_STARTED; //Default
    if (settingValue == 1) settings.lastState = STATE_BASE_NOT_STARTED;
  }
  else if (strstr(settingName, "stationECEF") != NULL)
  {
    replaceCharacter((char *)settingValueStr, ' ', ','); //Replace all ' ' with ',' before recording to file
    recordLineToSD(stationCoordinateECEFFileName, settingValueStr);
    recordLineToLFS(stationCoordinateECEFFileName, settingValueStr);
    log_d("%s recorded", settingValueStr);
  }
  else if (strstr(settingName, "stationGeodetic") != NULL)
  {
    replaceCharacter((char *)settingValueStr, ' ', ','); //Replace all ' ' with ',' before recording to file
    recordLineToSD(stationCoordinateGeodeticFileName, settingValueStr);
    recordLineToLFS(stationCoordinateGeodeticFileName, settingValueStr);
    log_d("%s recorded", settingValueStr);
  }

  //Unused variables - read to avoid errors
  else if (strcmp(settingName, "measurementRateSec") == 0) {}
  else if (strcmp(settingName, "baseTypeSurveyIn") == 0) {}
  else if (strcmp(settingName, "fixedBaseCoordinateTypeGeo") == 0) {}
  else if (strcmp(settingName, "saveToArduino") == 0) {}
  else if (strcmp(settingName, "enableFactoryDefaults") == 0) {}
  else if (strcmp(settingName, "enableFirmwareUpdate") == 0) {}
  else if (strcmp(settingName, "enableForgetRadios") == 0) {}
  else if (strcmp(settingName, "nicknameECEF") == 0) {}
  else if (strcmp(settingName, "nicknameGeodetic") == 0) {}

  //Special actions
  else if (strcmp(settingName, "firmwareFileName") == 0)
  {
    mountSDThenUpdate(settingValueStr);

    //If update is successful, it will force system reset and not get here.

    requestChangeState(STATE_ROVER_NOT_STARTED); //If update failed, return to Rover mode.
  }
  else if (strcmp(settingName, "factoryDefaultReset") == 0)
    factoryReset();
  else if (strcmp(settingName, "exitAndReset") == 0)
  {
    //Confirm receipt
    log_d("Sending reset confirmation");
    websocket->textAll("confirmReset,1,");
    delay(500); //Allow for delivery

    systemPrintln("Reset after AP Config");

    ESP.restart();
  }


  else if (strcmp(settingName, "setProfile") == 0)
  {
    //Change to new profile
    changeProfileNumber(settingValue);

    //Load new profile into system
    loadSettings();

    //Send new settings to browser. Re-use settingsCSV to avoid stack.
    settingsCSV = (char*)malloc(AP_CONFIG_SETTING_SIZE);
    memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); //Clear any garbage from settings array
    createSettingsString(settingsCSV);
    log_d("Sending command: %s", settingsCSV);
    websocket->textAll(String(settingsCSV));
    free(settingsCSV);
  }
  else if (strcmp(settingName, "resetProfile") == 0)
  {
    settingsToDefaults(); //Overwrite our current settings with defaults

    recordSystemSettings(); //Overwrite profile file and NVM with these settings

    //Get bitmask of active profiles
    activeProfiles = loadProfileNames();

    //Send new settings to browser. Re-use settingsCSV to avoid stack.
    settingsCSV = (char*)malloc(AP_CONFIG_SETTING_SIZE);
    memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); //Clear any garbage from settings array
    createSettingsString(settingsCSV);
    log_d("Sending command: %s", settingsCSV);
    websocket->textAll(String(settingsCSV));
    free(settingsCSV);
  }
  else if (strcmp(settingName, "forgetEspNowPeers") == 0)
  {
    //Forget all ESP-Now Peers
    for (int x = 0 ; x < settings.espnowPeerCount ; x++)
      espnowRemovePeer(settings.espnowPeers[x]);
    settings.espnowPeerCount = 0;
  }
  else if (strcmp(settingName, "startNewLog") == 0)
  {
    if (settings.enableLogging == true && online.logging == true)
      endSD(false, true); //Close down file. A new one will be created at the next calling of updateLogs().
  }

  //Check for bulk settings (constellations and message rates)
  //Must be last on else list
  else
  {
    bool knownSetting = false;

    //Scan for WiFi credentials
    if (knownSetting == false)
    {
      for (int x = 0 ; x < MAX_WIFI_NETWORKS ; x++)
      {
        char tempString[100]; //wifiNetwork0Password=parachutes
        sprintf(tempString, "wifiNetwork%dSSID", x);
        settingsFile->println(tempString);
        if (strcmp(settingName, tempString) == 0)
        {
          settings.wifiNetworks[x].ssid = settingValueStr;
          knownSetting = true;
          break;
        }
        else
        {
          sprintf(tempString, "wifiNetwork%dPassword", x);
          settingsFile->println(tempString);
          if (strcmp(settingName, tempString) == 0)
          {
            settings.wifiNetworks[x].password = settingValueStr;
            knownSetting = true;
            break;
          }
        }
      }
    }

    //Scan for constellation settings
    if (knownSetting == false)
    {
      for (int x = 0 ; x < MAX_CONSTELLATIONS ; x++)
      {
        char tempString[50]; //ubxConstellationsSBAS
        sprintf(tempString, "ubxConstellations%s", settings.ubxConstellations[x].textName);

        if (strcmp(settingName, tempString) == 0)
        {
          settings.ubxConstellations[x].enabled = settingValueBool;
          knownSetting = true;
          break;
        }
      }
    }

    //Scan for message settings
    if (knownSetting == false)
    {
      for (int x = 0 ; x < MAX_UBX_MSG ; x++)
      {
        if (strcmp(settingName, settings.ubxMessages[x].msgTextName) == 0)
        {
          settings.ubxMessages[x].msgRate = settingValue;
          knownSetting = true;
          break;
        }
      }
    }

    //Last catch
    if (knownSetting == false)
    {
      systemPrintf("Unknown '%s': %0.3lf\r\n", settingName, settingValue);
    }
  } //End last strcpy catch
#endif
}

//Add record with int
void stringRecord(char* settingsCSV, const char *id, int settingValue)
{
  char record[100];
  sprintf(record, "%s,%d,", id, settingValue);
  strcat(settingsCSV, record);
}

//Add record with uint32_t
void stringRecord(char* settingsCSV, const char *id, uint32_t settingValue)
{
  char record[100];
  sprintf(record, "%s,%d,", id, settingValue);
  strcat(settingsCSV, record);
}

//Add record with double
void stringRecord(char* settingsCSV, const char *id, double settingValue, int decimalPlaces)
{
  char format[10];
  sprintf(format, "%%0.%dlf", decimalPlaces); //Create '%0.09lf'

  char formattedValue[20];
  sprintf(formattedValue, format, settingValue);

  char record[100];
  sprintf(record, "%s,%s,", id, formattedValue);
  strcat(settingsCSV, record);
}

//Add record with bool
void stringRecord(char* settingsCSV, const char *id, bool settingValue)
{
  char temp[10];
  if (settingValue == true)
    strcpy(temp, "true");
  else
    strcpy(temp, "false");

  char record[100];
  sprintf(record, "%s,%s,", id, temp);
  strcat(settingsCSV, record);
}

//Add record with string
void stringRecord(char* settingsCSV, const char *id, char* settingValue)
{
  char record[100];
  sprintf(record, "%s,%s,", id, settingValue);
  strcat(settingsCSV, record);
}

//Add record with uint64_t
void stringRecord(char* settingsCSV, const char *id, uint64_t settingValue)
{
  char record[100];
  sprintf(record, "%s,%lld,", id, settingValue);
  strcat(settingsCSV, record);
}

//Break CSV into setting constituents
//Can't use strtok because we may have two commas next to each other, ie measurementRateHz,4.00,measurementRateSec,,dynamicModel,0,
bool parseIncomingSettings()
{
  char settingName[100] = {'\0'};
  char valueStr[150] = {'\0'}; //stationGeodetic1,ANameThatIsTooLongToBeDisplayed 40.09029479 -105.18505761 1560.089

  char* commaPtr = incomingSettings;
  char* headPtr = incomingSettings;
  while (*headPtr) //Check if string is over
  {
    //Spin to first comma
    commaPtr = strstr(headPtr, ",");
    if (commaPtr != NULL) {
      *commaPtr = '\0';
      strcpy(settingName, headPtr);
      headPtr = commaPtr + 1;
    }

    commaPtr = strstr(headPtr, ",");
    if (commaPtr != NULL) {
      *commaPtr = '\0';
      strcpy(valueStr, headPtr);
      headPtr = commaPtr + 1;
    }

    //log_d("settingName: %s value: %s", settingName, valueStr);

    updateSettingWithValue(settingName, valueStr);
  }

  //Confirm receipt
#ifdef COMPILE_AP
  log_d("Sending save confirmation");
  websocket->textAll("confirmSave,1,");
#endif

  return (true);
}

//When called, responds with the root folder list of files on SD card
//Name and size are formatted in CSV, formatted to html by JS
String getFileList()
{
  //settingsCSV[0] = '\'0; //Clear array
  String returnText = "";
  char fileName[50]; //Handle long file names

  //Attempt to gain access to the SD card
  if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
  {
    markSemaphore(FUNCTION_FILEMANAGER_UPLOAD1);

    SdFile dir;
    dir.open("/"); //Open root
    uint16_t fileCount = 0;

    while (managerTempFile.openNext(&dir, O_READ))
    {
      if (managerTempFile.isFile())
      {
        fileCount++;

        managerTempFile.getName(fileName, sizeof(fileName));

        returnText += "fmName," + String(fileName) + ",fmSize," + stringHumanReadableSize(managerTempFile.fileSize()) + ",";
      }
    }

    dir.close();
    managerTempFile.close();

    xSemaphoreGive(sdCardSemaphore);
  }
  else
  {
    char semaphoreHolder[50];
    getSemaphoreFunction(semaphoreHolder);

    //This is an error because the current settings no longer match the settings
    //on the microSD card, and will not be restored to the expected settings!
    systemPrintf("sdCardSemaphore failed to yield, held by %s, Form.ino line %d\r\n", semaphoreHolder, __LINE__);
  }

  log_d("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());

  return returnText;
}

// Make size of files human readable
String stringHumanReadableSize(uint64_t bytes) {

  char suffix[5] = {'\0'};
  char readableSize[50] = {'\0'};

  if (bytes < 1024) strcpy(suffix, "B");
  else if (bytes < (1024 * 1024)) strcpy(suffix, "KB");
  else if (bytes < (1024 * 1024 * 1024)) strcpy(suffix, "MB");
  else strcpy(suffix, "GB");

  if (bytes < (1024 * 1024)) bytes = bytes / 1024.0; //KB
  else if (bytes < (1024 * 1024 * 1024)) bytes = bytes / 1024.0 / 1024.0; //MB
  else bytes = bytes / 1024.0 / 1024.0 / 1024.0; //GB

  sprintf(readableSize, "%lld %s", bytes, suffix); //Don't print decimal portion of bytes

  return String(readableSize);
}

#ifdef COMPILE_WIFI
#ifdef COMPILE_AP

// Handles uploading of user files to SD
void handleUpload(AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  String logmessage = "";

  if (!index)
  {
    logmessage = "Upload Start: " + String(filename);

    char tempFileName[50];
    filename.toCharArray(tempFileName, sizeof(tempFileName));

    //Attempt to gain access to the SD card
    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
      markSemaphore(FUNCTION_FILEMANAGER_UPLOAD1);
      managerTempFile.open(tempFileName, O_CREAT | O_APPEND | O_WRITE);
      xSemaphoreGive(sdCardSemaphore);
    }

    systemPrintln(logmessage);
  }

  if (len)
  {
    //Attempt to gain access to the SD card
    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
      markSemaphore(FUNCTION_FILEMANAGER_UPLOAD2);
      managerTempFile.write(data, len); // stream the incoming chunk to the opened file
      xSemaphoreGive(sdCardSemaphore);
    }
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);

    //Attempt to gain access to the SD card
    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
      markSemaphore(FUNCTION_FILEMANAGER_UPLOAD3);
      updateDataFileCreate(&managerTempFile); // Update the file create time & date

      managerTempFile.close();
      xSemaphoreGive(sdCardSemaphore);
    }

    systemPrintln(logmessage);
    request->redirect("/");
  }
}

#endif
#endif
