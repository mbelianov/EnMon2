#include <avr/pgmspace.h>
#include <SimpleTimer.h>
#include <Wire.h> 
#include <PCD8544.h>
#include <TimeLib.h>
#include <DS1307RTC.h> 
#include <Timezone.h>
#include <EasyTransfer.h>
#include "OpCodes.h" 
#include <avr/wdt.h>


EasyTransfer ET;
comm_struct data;


#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)
bool startADC = true;
volatile bool conversionCompleted = false;
uint8_t current_channel = 0;
int SUPPLYVOLTAGE;
double I_RATIO;
double ICAL;
double Irms;
extern volatile unsigned long sumI;
extern const unsigned int adc_samples;
extern volatile unsigned int adc_sample_index;
extern volatile unsigned long timer;

// There must be one global SimpleTimer object.
// More SimpleTimer objects can be created and run,
// although there is little point in doing so.
SimpleTimer t;

static PCD8544 lcd;
int pos = 0;




struct sample_struct{
  time_t timestamp;
  float ch[3];
} sample;

void getMeasurement() {

  static char c[15];
  static byte i=0;

  i++;
 
  sample.timestamp = now();
  
  lcd.setCursor(0, 3);
  snprintf(c, sizeof(c), "%2d.%1d%3d.%1d%3d.%1d", (int)(sample.ch[0]), (int)(10.0*(sample.ch[0] - (int)(sample.ch[0]))),
                                                  (int)(sample.ch[1]), (int)(10.0*(sample.ch[1] - (int)(sample.ch[1]))),
                                                  (int)(sample.ch[2]), (int)(10.0*(sample.ch[2] - (int)(sample.ch[2]))));
  lcd.print(c);

  data.op = OP_SAMPLE;
  data.id = i;
  memcpy (data.dt, &sample, sizeof(sample)<=sizeof(data.dt)?sizeof(sample):sizeof(data.dt));
  ET.sendData();
  i2c_eeprom_write(data.dt, sizeof(data.dt));
}


void setup(void){
  wdt_enable(WDTO_4S);
  Wire.begin();
  Serial.begin(57600);
  lcd.begin();
  ET.begin(details(data), &Serial);


  syncRTCWithNTP();
  t.setInterval(1*24*3600L*1000L, syncRTCWithNTP);//sync once per pay 
  setSyncProvider(RTC.get);

  prepareAdcTimer();
  ICAL = 50.0;
  t.setInterval(1000, getMeasurement);

  //setPrintLine (message);
  pos = -84;
  generateBaseTitle();

  printDateTimeStr();
  t.setInterval(10000, printDateTimeStr);
  
  printFooterEx();
  t.setInterval(200, printFooterEx);
}



void loop(void){ 
  wdt_reset();
  // this is where the "polling" occurs
  t.run(); 
  
  if (startADC){
    startADC = false;
    startConversion(current_channel % 3);
  }
///lcd.setCursor(0,4); lcd.print(adc_sample_index);
    
  if (conversionCompleted){
    I_RATIO = ICAL *((readVcc()/1000.0) / (ADC_COUNTS));
    Irms = I_RATIO * sqrt((double)sumI / adc_samples); 
    sample.ch[current_channel  % 3] = 0.230 * Irms; //V=230V, P in kW
    current_channel++;
    startADC = true;
  }

  handleHistoryDataDownload();

}





