#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ui.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <base64.h>  // Include if not already

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const char* togglApiKey = "YOUR_TOGGL_API_KEY"; //PP

const char* togglHost = "api.track.toggl.com";
const char* togglPath = "/api/v9/me/time_entries?start_date=2025-05-01T00:00:00Z&end_date=2025-05-28T23:59:59Z";

/*Change to Screen resolution*/
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char* buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
  uint16_t touchX = 0, touchY = 0;

  bool touched = false;  //tft.getTouch( &touchX, &touchY, 600 );

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;

    Serial.print("Data x ");
    Serial.println(touchX);

    Serial.print("Data y ");
    Serial.println(touchY);
  }
}

String jsonBuffer;

String getTogglDuration() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  // const char* togglHost = "api.track.toggl.com";
  // const char* togglPath = "/api/v9/me/time_entries";

  // https.begin(client, togglHost, 443, togglPath, true);

  String url = "https://api.track.toggl.com/api/v9/me/time_entries";
  https.begin(client, url);  

  String auth = base64::encode(String(togglApiKey) + ":api_token");
  https.addHeader("Authorization", "Basic " + auth);

  int httpCode = https.GET();
  String payload = "{}";

  if (httpCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    payload = https.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpCode);
  }

  https.end();
  return payload;
}


void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // NTP servers
  Serial.println("Waiting for time sync...");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime synced.");
}

void updateDateLabel() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", timeinfo);

  lv_label_set_text(ui_PresentDate, dateStr);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected to WiFi: ");
  Serial.println(WiFi.localIP());

  // Set Europe/Berlin timezone (handles daylight savings too)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "Europe/Berlin", 1);
  tzset();
  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println(LVGL_Arduino);
  Serial.println("I am LVGL_Arduino");

  lv_init();


#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  tft.begin();        /* TFT init */
  tft.setRotation(3); /* Landscape orientation, flipped */

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);


  ui_init();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
  Serial.println("Setup done");
}

time_t parseISOTimestamp(const char* iso) {
  struct tm t = {};
  strptime(iso, "%Y-%m-%dT%H:%M:%S", &t);
  return mktime(&t);
}

// Parse the timestamp and adjust for the timezone offset if any.
time_t parseTimeWithTimezone(const String& isoDate) {
  // Find the time zone offset (if any)
  int tzIndex = isoDate.indexOf('Z');  // UTC (Z)
  if (tzIndex == -1) {
    tzIndex = isoDate.indexOf('+');                     // Check for '+'
    if (tzIndex == -1) tzIndex = isoDate.indexOf('-');  // Check for '-'
  }

  // Remove timezone info for parsing
  String cleanIso = isoDate;
  if (tzIndex != -1) {
    cleanIso = isoDate.substring(0, tzIndex);
  }

  // Now convert the clean ISO date without timezone info
  struct tm tmStruct = {};
  strptime(cleanIso.c_str(), "%Y-%m-%dT%H:%M:%S", &tmStruct);

  // If there's a timezone offset, adjust the time accordingly
  if (tzIndex != -1) {
    String timezoneOffset = isoDate.substring(tzIndex);
    int offsetHours = timezoneOffset.substring(1, 3).toInt();    // Hours offset
    int offsetMinutes = timezoneOffset.substring(4, 6).toInt();  // Minutes offset

    // Convert to seconds and adjust the time
    int totalOffsetSeconds = (offsetHours * 3600) + (offsetMinutes * 60);
    if (timezoneOffset[0] == '-') {
      totalOffsetSeconds = -totalOffsetSeconds;
    }

    // Adjust time based on the offset
    time_t rawTime = mktime(&tmStruct);
    rawTime -= totalOffsetSeconds;
    return rawTime;
  }

  return mktime(&tmStruct);  // If no timezone, return as is.
}

time_t timegm_utc(struct tm* tm) {
  time_t localTime = mktime(tm);                           // Interprets as local time
  time_t offset = localTime - mktime(gmtime(&localTime));  // Calculate offset
  return localTime - offset;                               // Adjust to get correct UTC time
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);

  String response = getTogglDuration();
  Serial.println("Raw JSON:");
  Serial.println(response);

  JSONVar timeEntries = JSON.parse(response);

  if (JSON.typeof(timeEntries) == "undefined") {
    Serial.println("Parsing failed!");
    return;
  }

  if (timeEntries.length() > 0) {
    int dayDuration = 0;
    int weekDuration = 0;
    int monthDuration = 0;

    time_t now = time(nullptr);
    struct tm* nowTm = localtime(&now);

    for (int i = 0; i < timeEntries.length(); i++) {
      JSONVar entry = timeEntries[i];
      int duration = int(entry["duration"]);
      if (duration < 0) continue;  // skip running timers

      time_t entryTime = parseISOTimestamp((const char*)entry["start"]);
      struct tm* entryTm = localtime(&entryTime);

      // Compare day
      bool sameDay = (entryTm->tm_mday == nowTm->tm_mday &&
                      entryTm->tm_mon  == nowTm->tm_mon &&
                      entryTm->tm_year == nowTm->tm_year);

      // Compare month
      bool sameMonth = (entryTm->tm_mon == nowTm->tm_mon &&
                        entryTm->tm_year == nowTm->tm_year);

      // Compare week (within 7 days difference)
      double secondsDiff = difftime(now, entryTime);
      bool sameWeek = (secondsDiff >= 0 && secondsDiff <= 7 * 24 * 3600);

      if (sameDay)
        dayDuration += duration;

      if (sameWeek)
        weekDuration += duration;

      if (sameMonth)
        monthDuration += duration;
    }

    // Debug output
    Serial.printf("Day duration: %d seconds\n", dayDuration);
    Serial.printf("Week duration: %d seconds\n", weekDuration);
    Serial.printf("Month duration: %d seconds\n", monthDuration);

    // Update day arc
    lv_arc_set_range(ui_DayBar, 0, 12);
    lv_arc_set_value(ui_DayBar, dayDuration / 3600);

    // Update week bar
    lv_bar_set_range(ui_WeekBar, 0, 36000);
    lv_bar_set_value(ui_WeekBar, weekDuration, LV_ANIM_ON);

    // Format labels
    char dayStr[16], weekStr[16], monthStr[16];
    snprintf(dayStr, sizeof(dayStr), "%02d:%02d Hrs", dayDuration / 3600, (dayDuration % 3600) / 60);
    snprintf(weekStr, sizeof(weekStr), "%02d:%02d Hrs", weekDuration / 3600, (weekDuration % 3600) / 60);
    snprintf(monthStr, sizeof(monthStr), "%02d:%02d Hrs", monthDuration / 3600, (monthDuration % 3600) / 60);

    lv_label_set_text(ui_dayHours, dayStr);
    lv_label_set_text(ui_weekHours, weekStr);
    lv_label_set_text(ui_monthHours, monthStr);

    updateDateLabel();

  } else {
    Serial.println("No entries found");
  }

  delay(5000);
}

