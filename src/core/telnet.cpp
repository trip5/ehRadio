#include "options.h"
#include <stdarg.h>
#include "battery.h"
#include "config.h"
#include "netserver.h" // For launchPlaybackTask
#include "network.h"
#include "player.h"
#include "telnet.h"

Telnet telnet;

static void normalize_to_crlf(const char *buf, char *outbuf, size_t outbuf_len) {
  size_t oi = 0;
  size_t buf_len = strlen(buf);
  
  // Quick optimization: check if conversion is needed (look for lone \n without \r)
  bool needs_conversion = false;
  for (size_t i = 0; i < buf_len; ++i) {
    if (buf[i] == '\n' && (i == 0 || buf[i-1] != '\r')) {
      needs_conversion = true;
      break;
    }
  }
  
  // If no conversion needed, just copy the string
  if (!needs_conversion) {
    size_t copy_len = (buf_len < outbuf_len - 1) ? buf_len : outbuf_len - 1;
    memcpy(outbuf, buf, copy_len);
    outbuf[copy_len] = '\0';
    return;
  }
  
  // Perform conversion
  for (size_t i = 0; i < buf_len && oi + 1 < outbuf_len; ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (i > 0 && buf[i-1] == '\r') {
        // already CRLF, copy '\n'
        outbuf[oi++] = '\n';
      } else {
        // insert CR then LF - ensure space for both characters and null terminator
        if (oi + 2 < outbuf_len) {
          outbuf[oi++] = '\r';
          outbuf[oi++] = '\n';
        } else {
          break;  // Not enough space
        }
      }
    } else {
      outbuf[oi++] = c;
    }
  }
  // Null-terminate
  if (oi < outbuf_len) outbuf[oi] = '\0'; else outbuf[outbuf_len-1] = '\0';
}

bool Telnet::_isIPSet(IPAddress ip) {
  return ip.toString() == "0.0.0.0";
}

bool Telnet::begin(bool quiet) {
  if (network.status==SDREADY) {
    BOOTLOG("Ready in SD Mode!");
    BOOTLOG("------------------------------------------------");
    Serial.println("##[BOOT]#");
    return true;
  }
  if (!quiet) Serial.print("##[BOOT]#\ttelnet.begin\t");
  if (WiFi.status() == WL_CONNECTED || _isIPSet(WiFi.softAPIP())) {
    server.begin();
    server.setNoDelay(true);
    if (!quiet) {
      Serial.println("done");
      Serial.println("##[BOOT]#");
      BOOTLOG("Ready! Go to http:/%s/ to configure", WiFi.localIP().toString().c_str());
      BOOTLOG("------------------------------------------------");
      Serial.println("##[BOOT]#");
    }
    return true;
  } else {
    return false;
  }
}

void Telnet::stop() {
  server.stop();
}

void Telnet::emptyClientStream(WiFiClient client) {
  client.flush();
  delay(50);
  while (client.available()) {
    client.read();
  }
}

void Telnet::cleanupClients() {
  for (int i = 0; i < MAX_TLN_CLIENTS; i++) {
    if (!clients[i].connected()) {
      if (clients[i]) {
        Serial.printf("Client [%d] is %s\r\n", i, clients[i].connected() ? "connected" : "disconnected");
        clients[i].stop();
      }
    }
  }
}

void Telnet::handleSerial() {
  if (Serial.available()) {
    String request = Serial.readStringUntil('\n'); request.trim();
    on_input(request.c_str(), 100);
  }
}

void Telnet::loop() {
  if (network.status==SDREADY || network.status!=CONNECTED) {
    handleSerial();
    return;
  }
  uint8_t i;
  if (WiFi.status() == WL_CONNECTED) {
    if (server.hasClient()) {
      for (i = 0; i < MAX_TLN_CLIENTS; i++) {
        if (!clients[i] || !clients[i].connected()) {
          if (clients[i]) {
            clients[i].stop();
          }
          clients[i] = server.available();
          if (!clients[i]) Serial.println("available broken");
          on_connect(clients[i].remoteIP().toString().c_str(), i);
          clients[i].setNoDelay(true);
          emptyClientStream(clients[i]);
          break;
        }
      }
      if (i >= MAX_TLN_CLIENTS) {
        server.available().stop();
      }
    }
    for (i = 0; i < MAX_TLN_CLIENTS; i++) {
      if (clients[i] && clients[i].connected() && clients[i].available()) {
        String inputstr = clients[i].readStringUntil('\n');
        inputstr.trim();
        on_input(inputstr.c_str(), i);
      }
    }
  } else {
    for (i = 0; i < MAX_TLN_CLIENTS; i++) {
      if (clients[i]) {
        clients[i].stop();
      }
    }
    delay(1000);
  }
  handleSerial();
}

void Telnet::print(const char *buf) {
  for (int id = 0; id < MAX_TLN_CLIENTS; id++) {
    if (clients[id] && clients[id].connected()) {
      print(id, buf);
    }
  }
  Serial.print(buf);
}   

void Telnet::print(uint8_t id, const char *buf) {
  if (id >= MAX_TLN_CLIENTS) return; // Bounds check
  if (clients[id] && clients[id].connected()) {
    clients[id].print(buf);
  }
}

void Telnet::printf(const char *format, ...) {
  char buf[MAX_PRINTF_LEN];
  va_list args;
  va_start (args, format);
  vsnprintf(buf, MAX_PRINTF_LEN, format, args);
  va_end (args);

  // Normalize line endings: convert lone '\n' to "\r\n"
  // Use larger buffer to handle worst-case CRLF expansion (every \n -> \r\n doubles size)
  char outbuf[MAX_PRINTF_LEN * 2];
  normalize_to_crlf(buf, outbuf, sizeof(outbuf));

  // Check if this is a prompt or a message
  bool isPrompt = (strcmp(outbuf, "> ") == 0);
  
  // Send to all connected clients
  for (int id = 0; id < MAX_TLN_CLIENTS; id++) {
    if (clients[id] && clients[id].connected()) {
      // For broadcasts (not prompts), clear any existing prompt first, then redraw after
      if (!isPrompt) {
        clients[id].print("\r");  // Move to start of line
        clients[id].print(outbuf);
        // If message ends with newline, redraw prompt
        size_t len = strlen(outbuf);
        if (len > 0 && outbuf[len-1] == '\n') {
          clients[id].print("> ");
        }
      } else {
        clients[id].print(outbuf);
      }
    }
  }

  // Don't print lone prompts to serial
  if (isPrompt) return;
  // Trim trailing prompt for Serial print (handle CRLF before prompt)
  char *nl = strstr(outbuf, "\r\n> ");
  if (nl == NULL) nl = strstr(outbuf, "\n> ");
  if (nl != NULL) { outbuf[nl-outbuf+1] = '\0'; }
  Serial.print(outbuf);
}

void Telnet::printf(uint8_t id, const char *format, ...) {
  char buf[MAX_PRINTF_LEN];
  va_list argptr;
  va_start(argptr, format);
  vsnprintf(buf, MAX_PRINTF_LEN, format, argptr);
  va_end(argptr);

  // Normalize line endings
  // Use larger buffer to handle worst-case CRLF expansion (every \n -> \r\n doubles size)
  char outbuf[MAX_PRINTF_LEN * 2];
  normalize_to_crlf(buf, outbuf, sizeof(outbuf));

  if(id>MAX_TLN_CLIENTS){
    Serial.print(outbuf);
    return;
  }
  if (clients[id] && clients[id].connected()) {
    clients[id].print(outbuf);
  }
}

void Telnet::on_connect(const char* str, uint8_t clientId) {
  Serial.printf("Telnet: [%d] %s connected\r\n", clientId, str);
  print(clientId, "Welcome to ehRadio!\r\n(Use ^] + q  ( Ctrl+] + q ) to disconnect.)\r\n");
  showPromptNow(clientId);
}

void Telnet::info() {
  telnet.printf("##CLI.INFO#\r\n");
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S+03:00", &network.timeinfo);
  telnet.printf("##SYS.DATE#: %s\r\n", timeStringBuff);
  telnet.printf("##CLI.TZNAME#: %s\r\n", config.store.tz_name);
  telnet.printf("##CLI.TZPOSIX#: %s\r\n", config.store.tzposix);
  telnet.printf("##CLI.NAMESET#: %d %s\r\n", config.lastStation(), config.station.name);
  if (player.status() == PLAYING) {
    telnet.printf("##CLI.META#: %s\r\n",  config.station.title);
  }
  telnet.printf("##CLI.VOL#: %d\r\n", config.store.volume);
  if (player.status() == PLAYING) {
    telnet.printf("##CLI.PLAYING#\r\n");
  } else {
    telnet.printf("##CLI.STOPPED#\r\n");
  }
}

void Telnet::showPromptNow(uint8_t clientId) {
  if (clientId < MAX_TLN_CLIENTS && clients[clientId] && clients[clientId].connected()) {
    clients[clientId].print("> ");
  }
}

void Telnet::on_input(const char* str, uint8_t clientId) {
  if (strlen(str) == 0) return;
  if (network.status == CONNECTED) {
    if (strcmp(str, "cli.prev") == 0 || strcmp(str, "prev") == 0) {
      player.prev();
      goto show_prompt;
    }
    if (strcmp(str, "cli.next") == 0 || strcmp(str, "next") == 0) {
      player.next();
      goto show_prompt;
    }
    if (strcmp(str, "cli.toggle") == 0 || strcmp(str, "toggle") == 0) {
      player.toggle();
      goto show_prompt;
    }
    if (strcmp(str, "cli.stop") == 0 || strcmp(str, "stop") == 0) {
      player.sendCommand({PR_STOP, 0});
      //info();
      goto show_prompt;
    }
    if (strcmp(str, "cli.start") == 0 || strcmp(str, "start") == 0 || strcmp(str, "cli.play") == 0 || strcmp(str, "play") == 0) {
      player.sendCommand({PR_PLAY, config.lastStation()});
      goto show_prompt;
    }
    if (strcmp(str, "cli.vol") == 0 || strcmp(str, "vol") == 0) {
      printf(clientId, "##CLI.VOL#: %d\r\n", config.store.volume);
      goto show_prompt;
    }
    if (strcmp(str, "cli.vol-") == 0 || strcmp(str, "vol-") == 0) {
      player.stepVol(false);
      goto show_prompt;
    }
    if (strcmp(str, "cli.vol+") == 0 || strcmp(str, "vol+") == 0) {
      player.stepVol(true);
      goto show_prompt;
    }
    #if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
      // Battery status commands
      if (strcmp(str, "cli.batt now") == 0 || strcmp(str, "batt now") == 0 || strcmp(str, "battery now") == 0) {
        // Force an immediate ADC read so CLI reports the latest values.
        battery_recalc_now();
      }
      if (strcmp(str, "cli.batt") == 0 || strcmp(str, "batt") == 0 || strcmp(str, "battery") == 0 ||
          strcmp(str, "cli.batt now") == 0 || strcmp(str, "batt now") == 0 || strcmp(str, "battery now") == 0) {
        // If 'now' was requested we already did battery_recalc_now(), otherwise we use cached readings.
        BatteryStatus b = battery_get_status();
        char status_line[256];  // Increased buffer for extended charging info with peak/trough/rate
        battery_format_status_line(b, status_line, sizeof(status_line), true);
        printf(clientId, "%s\r\n", status_line);
        goto show_prompt;
      }
  
      // Calibration helper: compute suggested BATTERY_ADC_REF_MV from a measured millivolt value
      int meas_mv;
      // Support: 'calbatt <mV>' to compute and save a new ADC ref, or 'calbatt reset' to clear saved value
      if (strcmp(str, "calbatt reset") == 0 || strcmp(str, "cal.batt reset") == 0) {
        config.saveValue(&config.store.battery_adc_ref_mv, (uint16_t)0);
        printf(clientId, "##CLI.CALIB#: Reset saved battery ADC ref (will use compile-time default)\r\n");
        goto show_prompt;
      }
      if (sscanf(str, "calbatt %d", &meas_mv) == 1 || sscanf(str, "cal.batt %d", &meas_mv) == 1) {
        // Validate measured voltage is in valid Li-Po range
        if (meas_mv < 2500 || meas_mv > 4500) {
          printf(clientId, "##CLI.CALIB#: measured voltage %dmV out of valid range (2500-4500mV)\r\n", meas_mv);
          goto show_prompt;
        }
        BatteryStatus b = battery_get_status();
        if (!b.valid) {
          printf(clientId, "##CLI.CALIB#: battery not detected\r\n");
          goto show_prompt;
        }
        if (b.voltage_mv == 0) {
          printf(clientId, "##CLI.CALIB#: cannot calibrate (firmware voltage 0)\r\n");
          goto show_prompt;
        }
        // Sanity check: if measured and firmware values differ by more than 50%, warn user
        int32_t diff_pct = abs((int32_t)meas_mv - (int32_t)b.voltage_mv) * 100 / b.voltage_mv;
        if (diff_pct > 50) {
          printf(clientId, "##CLI.CALIB#: Warning: measured %dmV differs from firmware %dmV by %ld%%. Verify multimeter reading.\r\n", meas_mv, b.voltage_mv, (long)diff_pct);
        }
        // Calculate ratio and suggested ADC ref (linear scaling)
        // Note: b.voltage_mv already checked for zero above
        double ratio = ((double)meas_mv) / ((double)b.voltage_mv);
        // Sanity check for unreasonable ratio (should be close to 1.0)
        if (ratio < 0.5 || ratio > 2.0) {
          printf(clientId, "##CLI.CALIB#: Computed ratio %.2f is unreasonable. Check multimeter reading.\r\n", ratio);
          goto show_prompt;
        }
        uint32_t curr_ref = (uint32_t)(config.store.battery_adc_ref_mv ? config.store.battery_adc_ref_mv : BATTERY_ADC_REF_MV);
        uint32_t suggested_ref = (uint32_t)( (double)curr_ref * ratio + 0.5 );
        // Validate computed reference is in valid range
        if (suggested_ref < 2000 || suggested_ref > 4000) {
          printf(clientId, "##CLI.CALIB#: Computed ref %u out of valid range (2000-4000mV). Check multimeter reading.\r\n", (unsigned)suggested_ref);
          goto show_prompt;
        }
        uint16_t newref = (uint16_t)suggested_ref;
        // Persist to config and save
        config.saveValue(&config.store.battery_adc_ref_mv, newref);
        // Recalculate immediately so 'batt' reflects saved value
        battery_recalc_now();
        BatteryStatus nb = battery_get_status();
        printf(clientId, "##CLI.CALIB#: Saved BATTERY_ADC_REF_MV=%u to prefs; new firmware Volt=%dmV, %d%%\r\n", newref, nb.voltage_mv, nb.percentage);
        goto show_prompt;
      }
  
      if (strcmp(str, "calbatt") == 0 || strcmp(str, "cal.batt") == 0) {
        BatteryStatus b = battery_get_status();
        uint32_t saved = (uint32_t)config.store.battery_adc_ref_mv;
        uint32_t effective = saved ? saved : (uint32_t)BATTERY_ADC_REF_MV;
        if (!b.valid) {
          printf(clientId, "##CLI.CALIB#: battery not detected\r\n");
        } else {
          printf(clientId, "##CLI.CALIB#: current firmware Volt=%dmV, saved_ref=%u, effective_ref=%u. Run 'calbatt <measured_mV>' to compute and save a new ADC ref.\r\n", b.voltage_mv, (unsigned)saved, (unsigned)effective);
        }
        goto show_prompt;
      }
    #endif //#if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
  
    if (strcmp(str, "sys.date") == 0 || strcmp(str, "date") == 0 || strcmp(str, "time") == 0) {
      network.requestTimeSync(true, clientId > MAX_TLN_CLIENTS?clientId:0);
      goto show_prompt;
    }
    int volume;
    if (sscanf(str, "vol(%d)", &volume) == 1 || sscanf(str, "cli.vol(\"%d\")", &volume) == 1 || sscanf(str, "vol %d", &volume) == 1) {
      if (volume < 0) volume = 0;
      if (volume > 254) volume = 254;
      player.setVol(volume);
      goto show_prompt;
    }
    if (strcmp(str, "cli.audioinfo") == 0 || strcmp(str, "audioinfo") == 0) {
      printf(clientId, "##CLI.AUDIOINFO#: %d\r\n", config.store.audioinfo > 0);
      goto show_prompt;
    }
    int ainfo;
    if (sscanf(str, "audioinfo(%d)", &ainfo) == 1 || sscanf(str, "cli.audioinfo(\"%d\")", &ainfo) == 1 || sscanf(str, "audioinfo %d", &ainfo) == 1) {
      config.saveValue(&config.store.audioinfo, ainfo > 0);
      printf(clientId, "new audioinfo value is: %d\r\n", config.store.audioinfo);
      goto show_prompt;
    }
    if (strcmp(str, "cli.smartstart") == 0 || strcmp(str, "smartstart") == 0) {
      printf(clientId, "##CLI.SMARTSTART#: %s\r\n", config.store.smartstart ? "true" : "false");
      goto show_prompt;
    }
    int sstart;
    if (sscanf(str, "smartstart(%d)", &sstart) == 1 || sscanf(str, "cli.smartstart(\"%d\")", &sstart) == 1 || sscanf(str, "smartstart %d", &sstart) == 1) {
      config.saveValue(&config.store.smartstart, (bool)(sstart != 0));
      printf(clientId, "new smartstart value is: %s\r\n", config.store.smartstart ? "true" : "false");
      goto show_prompt;
    }
    if (strcmp(str, "cli.list") == 0 || strcmp(str, "list") == 0) {
      printf(clientId, "#CLI.LIST#\r\n");
      File file = SPIFFS.open(PLAYLIST_PATH, "r");
      if (!file || file.isDirectory()) {
        return;
      }
      char sName[BUFLEN], sUrl[BUFLEN];
      int sOvol;
      uint8_t c = 1;
      while (file.available()) {
        if (config.parseCSV(file.readStringUntil('\n').c_str(), sName, sUrl, sOvol)) {
          // Use chunks to avoid buffer overflow when sName+sUrl are both long
          printf(clientId, "#CLI.LISTNUM#: %*d: ", 3, c);
          printf(clientId, "%s, %s\r\n", sName, sUrl);
          c++;
        }
      }
      printf(clientId, "##CLI.LIST#\r\n");
      goto show_prompt;
    }
    if (strcmp(str, "cli.info") == 0 || strcmp(str, "info") == 0) {
      printf(clientId, "##CLI.INFO#\r\n");
      char timeStringBuff[50];
      strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &network.timeinfo);
      printf(clientId, "##SYS.DATE#: %s\r\n", timeStringBuff);
      printf(clientId, "##CLI.TZNAME#: %s\r\n", config.store.tz_name);
      printf(clientId, "##CLI.TZPOSIX#: %s\r\n", config.store.tzposix);
      printf(clientId, "##CLI.NAMESET#: %d %s\r\n", config.lastStation(), config.station.name);
      if (player.status() == PLAYING) {
        printf(clientId, "##CLI.META#: %s\r\n", config.station.title);
      }
      printf(clientId, "##CLI.VOL#: %d\r\n", config.store.volume);
      if (player.status() == PLAYING) {
        printf(clientId, "##CLI.PLAYING#\r\n");
      } else {
        printf(clientId, "##CLI.STOPPED#\r\n");
      }
      goto show_prompt;
    }
    char playarg[512];
    if (sscanf(str, "play(%511[^)])", playarg) == 1 || sscanf(str, "cli.play(\"%511[^\"]\")", playarg) == 1 || sscanf(str, "play %511s", playarg) == 1) {
      // Check if playarg is a number or a URL
      if (strncmp(playarg, "http", 4) == 0) {
        config.loadStation(0);
        launchPlaybackTask(String(playarg), String(playarg)); // from netserver.cpp
        printf(clientId, "Playing URL: %s\r\n", playarg);
      } else {
        int sb = atoi(playarg);
        if (sb < 1) sb = 1;
        uint16_t cs = config.playlistLength();
        if (sb >= cs) sb = cs;
        player.sendCommand({PR_PLAY, (uint16_t)sb});
      }
      goto show_prompt;
    }
    #ifdef USE_SD
      int mm;
      if (sscanf(str, "mode %d", &mm) == 1) {
        if (mm > 2) mm = 0;
        if (mm==2)
          config.changeMode();
        else
          config.changeMode(mm);
        goto show_prompt;
      }
    #endif
    if (strcmp(str, "sys.tz") == 0 || strcmp(str, "tz") == 0)  {
      char timeStringBuff[50];
      strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &network.timeinfo);
      printf(clientId, "##SYS.DATE#: %s\r\n", timeStringBuff);

      // Display actual current local time (for debugging time zones)
      //time_t now = time(NULL);
      //struct tm localNow;
      //localtime_r(&now, &localNow);
      //strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &localNow);      
      //printf(clientId, "##ACTUAL TIME#: %s\r\n", timeStringBuff);

      printf(clientId, "##SYS.TZNAME#: %s\r\n", config.store.tz_name);
      printf(clientId, "##SYS.TZPOSIX#: %s\r\n", config.store.tzposix);
      goto show_prompt;

    }
    if (strcmp(str, "sys.tzo") == 0 || strcmp(str, "tzo") == 0)  {
      printf(clientId, "##SYS.TZPOSIX#: %s\r\n", config.store.tzposix);
      goto show_prompt;
    }
    if (strcmp(str, "sys.tzname") == 0 || strcmp(str, "tzname") == 0 )  {
      printf(clientId, "##SYS.TZNAME#: %s\r\n", config.store.tz_name);
      goto show_prompt;
    }
    //int16_t tzh, tzm; (re-used multiple times below)
    int tzh, tzm;
    if (sscanf(str, "tzo(%d:%d)", &tzh, &tzm) == 2 || sscanf(str, "sys.tzo(\"%d:%d\")", &tzh, &tzm) == 2 || sscanf(str, "tzo %d:%d", &tzh, &tzm) == 2) {
      if (tzh < -12) tzh = -12;
      if (tzh > 14) tzh = 14;
      if (tzm < 0) tzm = 0;
      if (tzm > 59) tzm = 59;
      // Construct POSIX TZ string (inverting sign)
      snprintf(config.store.tzposix, sizeof(config.store.tzposix), "GMT%+03d:%02d", -tzh, tzm);
      snprintf(config.store.tz_name, sizeof(config.store.tz_name), "%s", config.store.tzposix);
      printf(clientId, "New timezone offset: %c%02d:%02d\r\n", (tzh < 0 ? '-' : '+'), abs(tzh), tzm);
      printf(clientId, "##SYS.TZNAME#: %s\r\n", config.store.tz_name);
      printf(clientId, "##SYS.TZPOSIX#: %s\r\n", config.store.tzposix);
      config.saveValue(config.store.tzposix, config.store.tzposix);
      config.saveValue(config.store.tz_name, config.store.tz_name);
      network.forceTimeSync = true;
      network.requestTimeSync(true);
      return;
    }
    if (sscanf(str, "tzo(%d)", &tzh) == 1 || sscanf(str, "sys.tzo(\"%d\")", &tzh) == 1 || sscanf(str, "tzo %d", &tzh) == 1) {
      if (tzh < -12) tzh = -12;
      if (tzh > 14) tzh = 14;
      // Construct proper tz_name like "<+08>-8" or "<-04>4"
      snprintf(config.store.tz_name, sizeof(config.store.tz_name), "Etc/GMT%+d", -tzh);  // POSIX/Etc/GMT is inverted
      if (tzh >= 0) {
        snprintf(config.store.tzposix, sizeof(config.store.tzposix), "<-%02d>%d", tzh, tzh);
      } else {
        snprintf(config.store.tzposix, sizeof(config.store.tzposix), "<+%02d>%d", -tzh, tzh);
     }
      printf(clientId, "New timezone offset: %+d\r\n", tzh);
      printf(clientId, "##SYS.TZNAME#: %s\r\n", config.store.tz_name);
      printf(clientId, "##SYS.TZPOSIX#: %s\r\n", config.store.tzposix);
      config.saveValue(config.store.tzposix, config.store.tzposix);
      config.saveValue(config.store.tz_name, config.store.tz_name);
      network.forceTimeSync = true;
      network.requestTimeSync(true);
      return;
    }
    if (strcmp(str, "sys.tzposix") == 0 || strcmp(str, "tzposix") == 0)  {
      printf(clientId, "##SYS.TZPOSIX#: %s\r\n", config.store.tzposix);
      printf(clientId, "WARNING! USE THIS COMMAND CAREFULLY!\r\n");
      goto show_prompt;
    }
    char tzString[70]; // only used by the below section
    if (sscanf(str, "tzposix(%69[^)])", tzString) == 1 || sscanf(str, "sys.tzposix(\"%69[^\"]\")", tzString) == 1 || sscanf(str, "tzposix %69s", tzString) == 1) {
      // Temporarily set TZ env var
      if (setenv("TZ", tzString, 1) == 0) {
        tzset();  // Apply TZ setting
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        if (tm_info != NULL) {
          // POSIX string passes basic test. Save it.
          snprintf(config.store.tzposix, sizeof(config.store.tzposix), "%s", tzString);
          snprintf(config.store.tz_name, sizeof(config.store.tz_name), "%s", "Custom Timezone");
          printf(clientId, "POSIX timezone string applied. If not valid, expect strange behavior!\r\n");
          printf(clientId, "##SYS.TZNAME#: %s\r\n", config.store.tz_name);
          printf(clientId, "##SYS.TZPOSIX: %s\r\n", config.store.tzposix);
          config.saveValue(config.store.tzposix, config.store.tzposix);
          config.saveValue(config.store.tz_name, config.store.tz_name);
          network.forceTimeSync = true;
          network.requestTimeSync(true);
        } else {
          printf(clientId, "Invalid POSIX timezone (localtime returned NULL)\r\n");
        }
      } else {
        printf(clientId, "Failed to set TZ environment variable\r\n");
      }
      goto show_prompt;
    }
    if (sscanf(str, "dspon(%d)", &tzh) == 1 || sscanf(str, "cli.dspon(\"%d\")", &tzh) == 1 || sscanf(str, "dspon %d", &tzh) == 1) {
      config.setDspOn(tzh!=0);
      goto show_prompt;
    }
    if (sscanf(str, "dim(%d)", &tzh) == 1 || sscanf(str, "cli.dim(\"%d\")", &tzh) == 1 || sscanf(str, "dim %d", &tzh) == 1) {
      if (tzh < 0) tzh = 0;
      if (tzh > 100) tzh = 100;
      config.store.brightness = (uint8_t)tzh;
      config.setBrightness(true);
      goto show_prompt;
    }
    if (sscanf(str, "sleep(%d,%d)", &tzh, &tzm) == 2 || sscanf(str, "cli.sleep(\"%d\",\"%d\")", &tzh, &tzm) == 2 || sscanf(str, "sleep %d %d", &tzh, &tzm) == 2) {
      if (tzh>0 && tzm>0) {
        printf(clientId, "sleep for %d minutes after %d minutes ...\r\n", tzh, tzm);
        config.sleepForAfter(tzh, tzm);
      } else {
        printf(clientId, "##CMD_ERROR#\tunknown command <%s>\r\n", str);
      }
      goto show_prompt;
    }
    if (sscanf(str, "sleep(%d)", &tzh) == 1 || sscanf(str, "cli.sleep(\"%d\")", &tzh) == 1 || sscanf(str, "sleep %d", &tzh) == 1) {
      if (tzh>0) {
        printf(clientId, "sleep for %d minutes ...\r\n", tzh);
        config.sleepForAfter(tzh);
      } else {
        printf(clientId, "##CMD_ERROR#\tunknown command <%s>\r\n", str);
      }
      goto show_prompt;
    }
  }
  if (strcmp(str, "sys.version") == 0 || strcmp(str, "version") == 0) {
    printf(clientId, "##SYS.VERSION#: %s\r\n", RADIOVERSION);
    goto show_prompt;
  }
  if (strcmp(str, "sys.boot") == 0 || strcmp(str, "boot") == 0 || strcmp(str, "reboot") == 0) {
    ESP.restart();
    goto show_prompt;
  }
  if (strcmp(str, "sys.reset") == 0 || strcmp(str, "reset") == 0) {
    config.reset();
    goto show_prompt;
  }
  if (strcmp(str, "wifi.list") == 0 || strcmp(str, "wifi") == 0) {
    printf(clientId, "#WIFI.SCAN#\r\n");
    int n = WiFi.scanNetworks();
    if (n == 0) {
        printf(clientId, "no networks found\r\n");
    } else {
      for (int i = 0; i < n; ++i) {
        printf(clientId, "%d", i + 1);
        printf(clientId, ": ");
        printf(clientId, "%s", WiFi.SSID(i));
        printf(clientId, " (");
        printf(clientId, "%d", WiFi.RSSI(i));
        printf(clientId, ")");
        printf(clientId, (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
        printf(clientId, "\r\n");
        delay(10);
      }
    }
    printf(clientId, "#WIFI.SCAN#\r\n");
    goto show_prompt;
  }
  if (strcmp(str, "wifi.con") == 0 || strcmp(str, "conn") == 0) {
    printf(clientId, "#WIFI.CON#\r\n");
    File file = SPIFFS.open(SSIDS_PATH, "r");
    if (file && !file.isDirectory()) {
      char sSid[BUFLEN], sPas[BUFLEN];
      uint8_t c = 1;
      while (file.available()) {
        if (config.parseSsid(file.readStringUntil('\n').c_str(), sSid, sPas)) {
          printf(clientId, "%d: %s, %s\r\n", c, sSid, sPas);
          c++;
        }
      }
    }
    printf(clientId, "##WIFI.CON#\r\n");
    goto show_prompt;
  }
  if (strcmp(str, "wifi.station") == 0 || strcmp(str, "station") == 0 || strcmp(str, "ssid") == 0) {
    printf(clientId, "#WIFI.STATION#\r\n");
    File file = SPIFFS.open(SSIDS_PATH, "r");
    if (file && !file.isDirectory()) {
      char sSid[BUFLEN], sPas[BUFLEN];
      uint8_t c = 1;
      while (file.available()) {
        if (config.parseSsid(file.readStringUntil('\n').c_str(), sSid, sPas)) {
          if (c==config.store.lastSSID) printf(clientId, "%d: %s, %s\r\n", c, sSid, sPas);
          c++;
        }
      }
    }
    printf(clientId, "##WIFI.STATION#\r\n");
    goto show_prompt;
  }
  char newssid[sizeof(config.ssids[1].ssid)], newpass[sizeof(config.ssids[1].password)];
  if (sscanf(str, "wifi.con(\"%[^\"]\",\"%[^\"]\")", newssid, newpass) == 2 || sscanf(str, "wifi.con(%[^,],%[^)])", newssid, newpass) == 2 || sscanf(str, "wifi.con(%[^ ] %[^)])", newssid, newpass) == 2 || sscanf(str, "wifi %[^ ] %s", newssid, newpass) == 2) {
    char buf[BUFLEN];
    snprintf(buf, BUFLEN, "New SSID: \"%s\" with PASS: \"%s\" for next boot\r\n", newssid, newpass);
    printf(clientId, buf);
    printf(clientId, "...REBOOTING...\r\n");
    memset(buf, 0, BUFLEN);
    snprintf(buf, BUFLEN, "%s\t%s", newssid, newpass);
    config.saveWifi(buf);
    goto show_prompt;
  }
  if (strcmp(str, "wifi.status") == 0 || strcmp(str, "status") == 0) {
    printf(clientId, "#WIFI.STATUS#\r\nStatus:\t\t%d\r\nMode:\t\t%s\r\nIP:\t\t%s\r\nMask:\t\t%s\r\nGateway:\t\t%s\r\nRSSI:\t\t%d dBm\r\n##WIFI.STATUS#\r\n", 
      WiFi.status(), WiFi.getMode()==WIFI_STA?"WIFI_STA":"WIFI_AP", 
      WiFi.getMode()==WIFI_STA?WiFi.localIP().toString():WiFi.softAPIP().toString(),
      WiFi.getMode()==WIFI_STA?WiFi.subnetMask().toString():"255.255.255.0",
      WiFi.getMode()==WIFI_STA?WiFi.gatewayIP().toString():WiFi.softAPIP().toString(),
      WiFi.RSSI()
   );
    goto show_prompt;
  }
  if (strcmp(str, "wifi.rssi") == 0 || strcmp(str, "rssi") == 0) {
    printf(clientId, "#WIFI.RSSI#\t%d dBm\r\n", WiFi.RSSI());
    goto show_prompt;
  }
  if (strcmp(str, "sys.heap") == 0 || strcmp(str, "heap") == 0) {
    printf(clientId, "Free heap:\t%d bytes\r\n", xPortGetFreeHeapSize());
    goto show_prompt;
  }
  if (strcmp(str, "sys.config") == 0 || strcmp(str, "config") == 0) {
    config.bootInfo();
    printf(clientId, "Free heap:\t%d bytes\r\n", xPortGetFreeHeapSize());
    goto show_prompt;
  }
    if (strcmp(str, "sys.store") == 0 || strcmp(str, "store") == 0) {
    printf(clientId, "Config store struct size: %d bytes\r\n", sizeof(config.store));
    goto show_prompt;
  }
  if (strcmp(str, "wifi.discon") == 0 || strcmp(str, "discon") == 0 || strcmp(str, "disconnect") == 0) {
    printf(clientId, "#WIFI.DISCON#\tdisconnected...\r\n");
    WiFi.disconnect();
  }
  telnet.printf(clientId, "##CMD_ERROR#\tunknown command <%s>\r\n", str);
  
show_prompt:
  showPromptNow(clientId);
}
