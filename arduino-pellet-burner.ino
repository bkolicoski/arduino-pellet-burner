#include <ThreeWire.h>  
#include <RtcDS1302.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
//#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>

ThreeWire myWire(8,7,9); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);
LiquidCrystal_I2C lcd(0x27,20,4);
Encoder myEnc(3, 4);
String timeStr = "";
long oldPosition  = -999;

void setup() {
  Serial.begin(115200);
  //Wire.begin(D1, D2);
  initScreen();
  initTime();
  delay(2000);
}

void loop() {
  long newPosition = myEnc.read();
  if (newPosition != oldPosition && newPosition % 2 == 0) {
    oldPosition = newPosition;
    Serial.println(newPosition);
    printPosition(newPosition / 2);
  }
  
  RtcDateTime now = Rtc.GetDateTime();

  timeStr = returnDateTime(now);

  updateTime(timeStr);

  if (!now.IsValid())
  {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }

  delay(10);
}

void initScreen() {
  
  lcd.init();
  lcd.init(); // double init clears any previous text
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("Hello, world!");
  lcd.setCursor(0,1);
  lcd.print("20x4 LCD on NodeMCU!");
  lcd.setCursor(5,2);
  lcd.print("Subscribe!");
  lcd.setCursor(3,3);
  lcd.print("Taste The Code");
}

void initTime() {
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  timeStr = returnDateTime(compiled);
  Serial.println(timeStr);

  if (!Rtc.IsDateTimeValid()) 
  {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected())
  {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
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

void printPosition(long pos) {
  clearLCDLine(1);
  lcd.setCursor(0,1);
  lcd.print("Position: ");
  lcd.print(pos);
}

void clearLCDLine(int line)
{               
  lcd.setCursor(0,line);
  for(int n = 0; n < 20; n++) // 20 indicates symbols in line. For 2x16 LCD write - 16
  {
    lcd.print(" ");
  }
}

void updateTime(String tStr) {
  lcd.setCursor(0,0);
  lcd.print(tStr);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

String returnDateTime(const RtcDateTime& dt)
{
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
