#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>

ThreeWire myWire(14, 12, 27); // DAT, CLK, RST
RtcDS1302<ThreeWire> rtc(myWire);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Encoder myEnc(5, 18);

#define SWITCH_PIN 19
#define FEED_PIN 23
#define PUMP_PIN 25
#define HEATER_PIN 26

#define TRM_WATER 32

// NTC Settings
#define THERMISTORNOMINAL 10000 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25   // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 5            // how many samples to take and average, more takes longer but is more 'smooth'
#define BCOEFFICIENT 3950       // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 10000   // the value of the 'other' resistor 

// settings addresses
#define TARGET_T           0
#define IGNITION_TM        1
#define STABILIZATION_TM   2
#define CLEAN_INT          3
#define CLEAN_DUR          4
#define START_DOSE         5
#define P1_DOSE            6
#define P2_DOSE            7
#define P3_DOSE            8
#define P1_FAN             9
#define P2_FAN            10
#define P3_FAN            11
#define STABILIZIE_DOSE   12
#define STABILIZIE_FAN    13
#define CUT_OFF_T         14
#define SMOKE_MIN_T       15
#define SMOKE_MAX_T       16
#define PUMP_T            17
#define TIME_YEAR         18
#define TIME_MONTH        19
#define TIME_DAY          20
#define TIME_HOUR         21
#define TIME_MINUTE       22

int tmp_water = 20;
int tmp_water_old = 0;
String timeStr = "";
String status = "stand-by";
long oldPosition  = 0;
long initPosition = -999;
unsigned long menuTriggeredTime = 0;
RtcDateTime now, ignitionStartTime, shutdownStartTime;
const int numOfScreens = 23;
int currentScreen = -1;
String screens[numOfScreens][3] = {
  {"Target tmp.", "Celsius", "50"}, //TARGET_T
  {"Ignition time", "Minutes", "18"}, //IGNITION_TM
  {"Stabilization", "Minutes", "6"}, //STABILIZATION_TM
  {"Cleaning interval", "Minutes", "30"}, //CLEAN_INT
  {"Cleaning duration", "Seconds", "15"}, //CLEAN_DUR
  {"Start dose", "Seconds", "10"}, //START_DOSE
  {"P1 dose", "%", "10"}, //P1_DOSE
  {"P2 dose", "%", "25"}, //P2_DOSE
  {"P3 dose", "%", "30"}, //P3_DOSE
  {"P1 fan", "%", "10"}, //P1_FAN
  {"P2 fan", "%", "25"}, //P2_FAN
  {"P3 fan", "%", "30"}, //P3_FAN
  {"Stabilize dose", "%", "10"}, //STABILIZIE_DOSE
  {"Stabilize fan", "%", "10"}, //STABILIZIE_FAN
  {"Cut-off tmp.", "Celsius", "80"}, //CUT_OFF_T
  {"Smoke tmp. min.", "Celsius", "42"}, //SMOKE_MIN_T
  {"Smoke tmp. max.", "Celsius", "150"}, //SMOKE_MAX_T
  {"Pump tmp.", "Celsius", "30"}, //PUMP_T
  {"Year", "", "2021"}, //TIME_YEAR
  {"Month", "", "1"}, //TIME_MONTH
  {"Day", "", "1"}, //TIME_DAY
  {"Hour", "", "0"}, //TIME_HOUR
  {"Minute", "", "0"}, //TIME_MINUTE
};
int parameters[numOfScreens];
bool updateScreen = true;
bool updateStatus = true;
bool updateTime = false;

void setup() {
  //TODO: Add check for existing settings and load from memory
  for (int i = 0; i < numOfScreens; i++) {
    parameters[i] = (int) screens[i][2].toInt();
  }
  //TODO: Remove serial when no longer needed
  Serial.begin(115200);
  Serial.println(ignitionStartTime);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(FEED_PIN, OUTPUT); //feed
  digitalWrite(FEED_PIN, HIGH);
  pinMode(PUMP_PIN, OUTPUT); //pump
  digitalWrite(PUMP_PIN, HIGH);
  pinMode(HEATER_PIN, OUTPUT); //heater
  digitalWrite(HEATER_PIN, HIGH);
  attachInterrupt(SWITCH_PIN, handleButtonChange, CHANGE);
  initScreen();
  initTime();
  delay(2000);
  printOperatingTemperature();
  clearLCDLine(3);
  updateLCDStatus();
}

void loop() {
  long newPosition = myEnc.read();
  now = rtc.GetDateTime();
  //encoder turn handling
  if (newPosition != oldPosition && newPosition % 2 == 0) {
    if (menuTriggeredTime != 0 && currentScreen != -1) {
      if (newPosition > oldPosition) {
        parameters[currentScreen]++;
      } else {
        parameters[currentScreen]--;
      }
      if(currentScreen >= TIME_YEAR) {
        updateTime = true;
      }
      //reset menu trigger time on parameter change
      menuTriggeredTime = millis();
      updateScreen = true;
    } else {
      //update operating temperature
      if(newPosition > oldPosition) {
        parameters[TARGET_T]++;
      } else if(newPosition < oldPosition){
        parameters[TARGET_T]--;
      }
      printOperatingTemperature();
    }
    oldPosition = newPosition;
  }

  //settings trigger handling
  if (menuTriggeredTime != 0 && currentScreen != -1) {
    displaySettingsMenu();
    if (menuTriggeredTime + 4000 < millis()) {
      menuTriggeredTime = 0;
      currentScreen = -1;
      //Serial.println("Init pos:");
      //Serial.println(initPosition);
      myEnc.write(initPosition);
      oldPosition = initPosition;
      newPosition = initPosition;
      initPosition = -999;
      printOperatingTemperature();
      printCurrentTemperature();
      updateLCDStatus();
    }
  } else {
    //no clicking, handle other logic
    updateScreenTime(now);
    if (!now.IsValid())
    {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
    }

    //measure current temperature
    tmp_water = measureTemperature(TRM_WATER);
    if(tmp_water != tmp_water_old) {
      printCurrentTemperature();
      tmp_water_old = tmp_water;
    }
  }

  resolveStatusState();
  resolveHeaterState();
  resolvePumpState();
  resolveFeedState();

  if(updateStatus) {
    updateLCDStatus();
    updateStatus = false;
  }

  delay(10);
}

void resolveStatusState() {
  if(status == "ignition") {
    //TODO: Add proper settings delay check for ignition time instead of 30
    if(ignitionStartTime + 30 < now) {
      //TODO: Add exaust temperature check to confirm flame
      status = "stabilization";
      updateLCDStatus();
    }
  } else if(status == "stabilization") {
    //TODO: Add proper delay for ignition + stabilization time
    if(ignitionStartTime + 60 < now) {
      status = "heating";
      updateLCDStatus();
    }
  } else if(status == "shutdown") {
    //TODO: Add proper delay for ignition + stabilization time
    if(shutdownStartTime + 30 < now) {
      //TODO: Add exaust temperature check to confirm shutdown
      status = "stand-by";
      ignitionStartTime = NULL;
      shutdownStartTime = NULL;
      Serial.println(ignitionStartTime);
      updateLCDStatus();
    }
  }
}

void resolveHeaterState() {
  if(status == "ignition") {
    //activate
    digitalWrite(HEATER_PIN, LOW);
  } else {
    digitalWrite(HEATER_PIN, HIGH);
  }
}

void resolvePumpState() {
  if(tmp_water >= parameters[PUMP_T]) {
    //activate
    digitalWrite(PUMP_PIN, LOW);
  } else if (tmp_water < parameters[PUMP_T] - 2) {
    digitalWrite(PUMP_PIN, HIGH);
  }
}

void resolveFeedState() {
  if(status == "ignition") {
    //load start dose 
    
    if(ignitionStartTime + parameters[START_DOSE] <= now) {
      //still in start dose
      digitalWrite(FEED_PIN, LOW);
      Serial.println("ON");
    } else {
      //past start dose
      digitalWrite(FEED_PIN, HIGH);
      Serial.println("OFF");
    }
  } else if(status == "stabilization") {
    //add stabilization dose
  } else if(status == "heating") {
    //add stabilization dose
  } else {
    //turn off feed in any other case
    digitalWrite(FEED_PIN, HIGH);
    Serial.println("OFF 2");
  }
}

int measureTemperature(int pinNumber) {
  uint8_t i;
  float average;
  int samples[NUMSAMPLES];
 
  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(pinNumber);
   delay(10);
  }
 
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;

  // convert the value to resistance
  average = 4095 / average - 1;
  average = SERIESRESISTOR / average;

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C

  return round(steinhart);
  //TODO: Add temperature offset for adjustment
}

void initScreen() {

  lcd.init();
  lcd.init(); // double init clears any previous text
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("PELLET BURNER");
  lcd.setCursor(7, 1);
  lcd.print("v 0.1");
  lcd.setCursor(3, 3);
  lcd.print("Taste The Code");
}

void initTime() {
  rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  timeStr = returnDateTime(compiled);
  Serial.println(timeStr);

  if (!rtc.IsDateTimeValid())
  {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    rtc.SetDateTime(compiled);
  }

  if (rtc.GetIsWriteProtected())
  {
    Serial.println("RTC was write protected, enabling writing now");
    rtc.SetIsWriteProtected(false);
  }

  if (!rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    rtc.SetIsRunning(true);
  }

  now = rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    rtc.SetDateTime(compiled);
  }
  else if (now > compiled)
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled)
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
  //update parameters with date/time
  parameters[TIME_YEAR] = now.Year();
  parameters[TIME_MONTH] = now.Month();
  parameters[TIME_DAY] = now.Day();
  parameters[TIME_HOUR] = now.Hour();
  parameters[TIME_MINUTE] = now.Minute();
}

void printOperatingTemperature() {
  clearLCDLine(1);
  lcd.setCursor(0, 1);
  lcd.print("Target tmp: ");
  lcd.print(parameters[0]);
  lcd.print((char)223);
  lcd.print("C");
}

void printCurrentTemperature() {
    clearLCDLine(2);
    lcd.setCursor(0, 2);
    lcd.print("Current tmp: ");
    lcd.print(tmp_water);
    lcd.print((char)223);
    lcd.print("C");
}

void updateLCDStatus() {
  clearLCDLine(3);
  lcd.setCursor(0, 3);
  lcd.print("State: ");
  lcd.print(status);
}

void clearLCDLine(int line) {
  lcd.setCursor(0, line);
  for (int n = 0; n < 20; n++) // 20 indicates symbols in line. For 2x16 LCD write - 16
  {
    lcd.print(" ");
  }
}

void updateScreenTime(RtcDateTime time) {
  if(updateTime) {
    time = RtcDateTime(parameters[TIME_YEAR], parameters[TIME_MONTH], parameters[TIME_DAY], parameters[TIME_HOUR], parameters[TIME_MINUTE], 0);
    rtc.SetDateTime(time);
    updateTime = false;
  }
  String tStr = returnDateTime(time);
  if (timeStr != tStr) {
    lcd.setCursor(0, 0);
    lcd.print(tStr);
    timeStr = tStr;
  }
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

String returnDateTime(const RtcDateTime& dt) {
  char datestring[21];
  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u  %02u:%02u:%02u"),
             dt.Day(),
             dt.Month(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  return datestring;
}

void handleButtonChange()
{
  if(digitalRead(SWITCH_PIN) == HIGH) {
    //release
    if(menuTriggeredTime + 3000 <= millis()) {
      if(status == "stand-by") {
        ignitionStartTime = now;
        status = "ignition";
      } else {
        shutdownStartTime = now;
        status = "shutdown";
      }
      updateStatus = true;
    } else {
      menuTriggeredTime = millis();
      currentScreen++;
      if (currentScreen >= numOfScreens) {
        currentScreen = 0;
      }
    }
  } else {
    //push
    if (menuTriggeredTime == 0) {
      currentScreen = -1;
      initPosition = oldPosition;
    }
    menuTriggeredTime = millis();
  }
  updateScreen = true;
}

void displaySettingsMenu() {
  if (updateScreen) {
    lcd.clear();
    lcd.print(" ***  SETTINGS  *** ");
    lcd.setCursor(0, 1);
    lcd.print(screens[currentScreen][0]);
    lcd.setCursor(0, 2);
    lcd.print(parameters[currentScreen]);
    lcd.print(" ");
    lcd.print(screens[currentScreen][1]);
    updateScreen = false;
  }
}
