#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>

ThreeWire myWire(8, 7, 9); // DAT, CLK, RST
RtcDS1302<ThreeWire> rtc(myWire);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Encoder myEnc(3, 4);
String timeStr = "";
String status = "stand-by";
long oldPosition  = 0;
long initPosition = -999;
unsigned long menuTriggeredTime = 0;
RtcDateTime now, ignitionStartTime, shutdownStartTime;
const int numOfScreens = 7;
int currentScreen = -1;
String screens[numOfScreens][3] = {
  {"Operating Tmp", "Celsius", "50"},
  {"Ignition Time", "Minutes", "18"},
  {"Stabilization", "Minutes", "6"},
  {"Cleaning interval", "Minutes", "30"},
  {"Cleaning duration", "Seconds", "15"},
  {"Start dose", "Seconds", "10"},
  {"Cut-off Tmp", "Celsius", "80"},
};
int parameters[numOfScreens];
bool updateScreen = true;

void setup() {
  //TODO: Add check for existing settings and load from memory
  for (int i = 0; i < numOfScreens; i++) {
    parameters[i] = (int) screens[i][2].toInt();
  }
  //TODO: Remove serial when no longer needed
  Serial.begin(115200);
  Serial.println(ignitionStartTime);
  pinMode(2, INPUT_PULLUP);
  pinMode(10, OUTPUT); //feed
  digitalWrite(10, HIGH);
  pinMode(11, OUTPUT); //pump
  digitalWrite(11, HIGH);
  pinMode(12, OUTPUT); //heater
  digitalWrite(12, HIGH);
  attachInterrupt(digitalPinToInterrupt(2), handleButtonChange, CHANGE);
  initScreen();
  initTime();
  delay(2000);
  clearLCDLine(2);
  clearLCDLine(3);
}

void loop() {
  long newPosition = myEnc.read();
  now = rtc.GetDateTime();
  if (newPosition != oldPosition && newPosition % 2 == 0) {
    if (menuTriggeredTime != 0 && currentScreen != -1) {
      if (newPosition > oldPosition) {
        parameters[currentScreen]++;
      } else {
        parameters[currentScreen]--;
      }
      //reset menu trigger time on parameter change
      menuTriggeredTime = millis();
    } else {
      //update operating temperature
      if(newPosition > oldPosition) {
        parameters[0]++;
      } else if(newPosition < oldPosition){
        parameters[0]--;
      }
    }
    oldPosition = newPosition;
    updateScreen = true;
  }

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
      updateScreen = true;
    }
  } else {
    updateTime(returnDateTime(now));
    if (!now.IsValid())
    {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
    }
  }

  resolveStatusState();

  resolveHeaterState();

  if(updateScreen) {
    updateLCDStatus();
    printOperatingTemperature();
    printCurrentTemperature();
    updateScreen = false;
  }

  delay(10);
}

void resolveStatusState() {
  if(status == "ignition") {
    //TODO: Add proper settings delay check for ifnition time instead of 30
    if(ignitionStartTime + 30 < now) {
      //TODO: Add exaust temperature check to confirm flame
      status = "stabilization";
      updateScreen = true;
    }
  } else if(status == "stabilization") {
    //TODO: Add proper delay for ignition + stabilization time
    if(ignitionStartTime + 60 < now) {
      status = "heating";
      updateScreen = true;
    }
  } else if(status == "shutdown") {
    //TODO: Add proper delay for ignition + stabilization time
    if(shutdownStartTime + 30 < now) {
      //TODO: Add exaust temperature check to confirm shutdown
      status = "stand-by";
      ignitionStartTime = NULL;
      shutdownStartTime = NULL;
      Serial.println(ignitionStartTime);
      updateScreen = true;
    }
  }
}

void resolveHeaterState() {
  if(status == "ignition") {
    digitalWrite(12, LOW);
  } else {
    digitalWrite(12, HIGH);
  }
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

  RtcDateTime now = rtc.GetDateTime();
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
  lcd.print("xx"); //TODO: Add reading from temperature probe
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

void updateTime(String tStr) {
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
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  return datestring;
}

void handleButtonChange()
{
  if(digitalRead(2) == HIGH) {
    //release
    if(menuTriggeredTime + 3000 <= millis()) {
      if(status == "stand-by") {
        ignitionStartTime = now;
        status = "ignition";
      } else {
        shutdownStartTime = now;
        status = "shutdown";
      }
    } else {
      menuTriggeredTime = millis();
      currentScreen++;
      if (currentScreen >= numOfScreens) {
        currentScreen = 0;
      }
    }
    updateScreen = true;
  } else {
    //push
    if (menuTriggeredTime == 0) {
      initPosition = oldPosition;
    }
    menuTriggeredTime = millis();
  }
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
