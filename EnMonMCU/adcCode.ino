//const byte adcPin = 0;  // A0
const byte adc_inputs [] = {0, 1, 2};
int adc_index;

const unsigned int adc_samples = 1480;
//volatile  int lastSampleI;
volatile  int sampleI;
//volatile  double lastFilteredI;
volatile  long filteredI;
//volatile  double sqI;
volatile  unsigned long sumI;
volatile unsigned int adc_sample_index;
volatile unsigned long timer = 0;

#define FILTERSHIFT 13
#define FILTERROUNDING (1<<12)
volatile int offsetI[sizeof(adc_inputs)] = {512, 512, 512};
volatile long shiftedOffsetI[sizeof(adc_inputs)] = {512L<<13, 512L<<13, 512L<<13};

// ADC complete ISR
ISR (ADC_vect){
  
  if (adc_sample_index++ >= adc_samples){
    //ADCSRA = 0;  // turn off ADC
    ADCSRA &= ~(bit(ADIE) | bit (ADATE));
    conversionCompleted = true;
  }
  else
  {
    sampleI = ADC;
    shiftedOffsetI[adc_index] = shiftedOffsetI[adc_index] + (sampleI - offsetI[adc_index]);
    offsetI[adc_index] = (int)((shiftedOffsetI[adc_index]+FILTERROUNDING)>>FILTERSHIFT);
    filteredI = sampleI - offsetI[adc_index];
    
    sumI += filteredI * filteredI;
  }
}  // end of ADC_vect
  
ISR (TIMER1_COMPB_vect){
  timer++;
}

void prepareAdcTimer(){

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1B = bit (CS11) | bit (WGM12);  // CTC, prescaler of 8
  TIMSK1 = bit (OCIE1B);  // WTF?
  OCR1A = 199;    
  OCR1B = 199;   // 200 uS - sampling frequency 10 kHz

  ADCSRA =  bit (ADEN) | bit (ADPS2);   // turn ADC on, set prescaler of 16
  ADCSRB = bit (ADTS2) | bit (ADTS0);  // Timer/Counter1 Compare Match B
  
  sei();
}

void startConversion(int index){

  adc_index=index;
  adc_sample_index = 0;
  sumI = 0;
//  sampleI = 0;
//  filteredI = 0;
//  offsetI = 512;
//  shiftedOffsetI = 512L<<13;
  
  //ADCSRA =  bit (ADEN) | (bit (ADIE) | bit (ADIF)) | bit (ADPS2);   // turn ADC on, want interrupt on completion, set prescaler of 16
  //ADCSRB = bit (ADTS2) | bit (ADTS0);  // Timer/Counter1 Compare Match B  
  ADMUX = bit (REFS0) | (adc_inputs[index] & 7);
  startADC = false; 
  conversionCompleted = false; 
  ADCSRA |= bit (ADIE) | bit (ADIF);    // want interrupt on completion 
  ADCSRA |= bit (ADATE);                // turn on automatic triggering 

}


#define READVCC_CALIBRATION_CONST 1126400L
long readVcc() {
  long result;

  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  
  //ADCSRA =  bit (ADEN) | bit (ADPS2);  // turn ADC on, set prescaler of 16  
  delay(2);                                        // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);                             // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = READVCC_CALIBRATION_CONST / result;  //1100mV*1024 ADC steps http://openenergymonitor.org/emon/node/1186
  return result;

}

