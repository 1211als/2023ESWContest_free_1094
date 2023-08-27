int currentsens1 = 0;
int currentsens2 = 0;
int currentsens3 = 0;
int currentsens4 = 0;


unsigned long currentMillis1 = 0;
unsigned long previousMillis1 = 0;

unsigned long currentMillis2 = 0;
unsigned long previousMillis2 = 0;

unsigned long currentMillis3 = 0;
unsigned long previousMillis3 = 0;

unsigned long currentMillis4 = 0;
unsigned long previousMillis4 = 0;


void adc_init(void){    
  ADMUX = (1 << REFS0);              
  ADMUX &= ~(1 << ADLAR);            
  ADCSRA = (1 << ADEN) | 4;          
}

uint16_t adc_read(unsigned char channel){         
  ADMUX &= (0xF0);
  ADMUX |= (channel & 0x0F);
  ADCSRA |= (1 << ADSC);
  while(!(ADCSRA & (1 << ADIF)));
  ADCSRA |= (1 << ADIF);
    
  return ADC;
}


void setup(){ 
  Serial.begin(9600);
  adc_init();
  DDRD |= _BV(7);  
  DDRB |= _BV(0)|_BV(1)|_BV(2)|_BV(3)|_BV(4); // Set GPIO 7 ~ 12 is output;
}

void loop(){       
  currentsens1 = adc_read(5);
  if(currentsens1 < 600){
    currentMillis1 = micros();   
    PORTD |= _BV(7);
    if(currentMillis1 - previousMillis1 > 6000){
      PORTD &= ~_BV(7);
    }
  }
  else if(currentsens1 > 600){
    previousMillis1 = micros();
    PORTD &= ~_BV(7);
  }


  currentsens2 = adc_read(4); 
  if(currentsens2 < 600){
    currentMillis2 = micros();   
    PORTB |= _BV(0);
    if(currentMillis2 - previousMillis2 > 4000){
      PORTB &= ~_BV(0);
    }
  } 
  else if(currentsens2 > 600){
    previousMillis2 = micros();
    PORTB &= ~_BV(0);
  }


  currentsens3 = adc_read(3); 
  if(currentsens3 < 600){
    currentMillis3 = micros();   
    PORTB |= _BV(1);
    if(currentMillis3 - previousMillis3 > 3000){
      PORTB &= ~_BV(1);
    }
  } 
  else if(currentsens3 > 600){
    previousMillis3 = micros();
    PORTB &= ~_BV(1);
  }


  currentsens4 = adc_read(2);
  if(currentsens4 < 600){
    currentMillis4 = micros();   
    PORTB |= _BV(2);
    if(currentMillis4 - previousMillis4 > 3000){
      PORTB &= ~_BV(2);
    }
  } 
  else if(currentsens4 > 600){
    previousMillis4 = micros();
    PORTB &= ~_BV(2);
  }

}
