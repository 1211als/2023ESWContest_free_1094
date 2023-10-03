#include "Wire.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_SLAVE_ADDRESS 0x40


const int MAX_INPUT_I2C = 13;  //12 digit + null

char inputBuffer[MAX_INPUT_I2C];
int inputIndex = 0;

char received_data[13];


QueueHandle_t xQueue;

QueueHandle_t yQueue;

int adcValue1 = 0;
int adcValue2 = 0;
int currentState = -1;   //-1: Initial state, 0: Stop, 1: Forward, 2: Reverse


int lastsens1 = 1;
int currentsens1 = 1;

void stepper(void *pvParameters) {  //Task to drive the stepper motor with the input coordinates
  int receivedValue;
  static int prevReceivedValue = 640;  //Set initial value to 640
  int difference;
  int moving = 0;
  
  gpio_pad_select_gpio(GPIO_NUM_19);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
  
  gpio_pad_select_gpio(GPIO_NUM_5);
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_NUM_18);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_NUM_4);
  gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
  
  while(1) {
    if (!moving && xQueueReceive(xQueue, &receivedValue, portMAX_DELAY)) {
      moving = 1;
      if(receivedValue >= 1279) receivedValue = 1279;
      if(receivedValue <= 0) receivedValue = 0;  

      difference = receivedValue - prevReceivedValue;  //Calculate the difference

      if (abs(difference) <= 8) {       
        moving = 0;
        continue;
      }

      int halfDifference = abs(difference) / 4;

      if(difference > 0) {  //Forward
        gpio_set_level(GPIO_NUM_18, 1);
        gpio_set_level(GPIO_NUM_4, 1);
        for(int i=0; i<halfDifference; i++) {  //Repeat as much as the absolute value of the difference
          gpio_set_level(GPIO_NUM_19, 1);  
          gpio_set_level(GPIO_NUM_5, 1);          
          vTaskDelay(3 / portTICK_PERIOD_MS);
        
          gpio_set_level(GPIO_NUM_19, 0);  
          gpio_set_level(GPIO_NUM_5, 0);         
          vTaskDelay(3 / portTICK_PERIOD_MS);
        }
      } 
      else if(difference < 0) {  //Reverse     
        gpio_set_level(GPIO_NUM_18, 0);
        gpio_set_level(GPIO_NUM_4, 0);
        for(int i=0; i<halfDifference; i++) {  //Repeat as much as the absolute value of the difference
          gpio_set_level(GPIO_NUM_19, 1);  
          gpio_set_level(GPIO_NUM_5, 1);          
          vTaskDelay(3 / portTICK_PERIOD_MS);
        
          gpio_set_level(GPIO_NUM_19, 0);  
          gpio_set_level(GPIO_NUM_5, 0);         
          vTaskDelay(3 / portTICK_PERIOD_MS);
        }
      }
      prevReceivedValue = 640; 
      moving = 0;

      int dummy;
      while (xQueueReceive(xQueue, &dummy, 0)) {  //Empty the queue
      }
    }
  }
}

void linear_act(void *pvParameters) {  //Task to drive the linear actuator with the input coordinates
  int receivedValue;
  static int prevValue = 360;  //Set initial value to 360
  int moving = 0;

  ledcSetup(0, 1000, 10);
  ledcAttachPin(25, 0);

  ledcSetup(1, 1000, 10);
  ledcAttachPin(26, 1);

  ledcSetup(2, 1000, 10);
  ledcAttachPin(32, 2);

  ledcSetup(3, 1000, 10);
  ledcAttachPin(33, 3);

  while(1) {
    if(!moving && xQueueReceive(yQueue, &receivedValue, portMAX_DELAY)) { 
      moving = 1;  
      int difference = (receivedValue - prevValue)*0.5;  //Calculate the difference
      if (difference >= 2000) difference = 2000;

      if (abs(difference) <= 10) {       
        moving = 0;
        continue;
      }

      if(difference > 0) {  //Forward
        ledcWrite(0, 970);
        ledcWrite(1, 0);
    
        ledcWrite(2, 1000);
        ledcWrite(3, 0);
        
        vTaskDelay(abs(difference) / portTICK_PERIOD_MS);  
        
        ledcWrite(0, 0);
        ledcWrite(1, 0);    
        ledcWrite(2, 0);
        ledcWrite(3, 0);
      } 
      else if(difference < 0) {  //Reverse
        ledcWrite(0, 0);
        ledcWrite(1, 965);
    
        ledcWrite(2, 0);
        ledcWrite(3, 1000); 
        
        vTaskDelay(abs(difference) / portTICK_PERIOD_MS);  

        ledcWrite(0, 0);
        ledcWrite(1, 0);    
        ledcWrite(2, 0);
        ledcWrite(3, 0);
      }

      prevValue = 360; 
      moving = 0;

      int dummy;
      while (xQueueReceive(yQueue, &dummy, 0)) {  //Empty the queue
      }
    }
  }
}

void receiveData(int bytes) {  //Data received from Jetson Nano via i2c
  int i = 0;
  while (Wire.available()) {
    char c = Wire.read();
    if (c != '\0' && i < 12) {  //Skip NULL
      received_data[i] = c;
      i++;
    }
  }
  received_data[i] = '\0';  //Add NULL to end of string

  char part1[5];
  char part2[5];
  char part3[5];

  strncpy(part1, received_data, 4);
  part1[4] = '\0';

  strncpy(part2, received_data + 4, 4);
  part2[4] = '\0';

  strncpy(part3, received_data + 8, 4);
  part3[4] = '\0';

  Serial.println(received_data);

  int intPart1 = atoi(part1);  //x coordinate
  int intPart2 = atoi(part2);  //y coordinate
  int intPart3 = atoi(part3);  //Distance
  
  //Queue each task to transfer data
  xQueueSend(xQueue, &intPart1, portMAX_DELAY);  
  xQueueSend(yQueue, &intPart2, portMAX_DELAY);
}


void setup() {
  xQueue = xQueueCreate(10, sizeof(int));  //Create Queue

  yQueue = xQueueCreate(10, sizeof(int));  //Create Queue
  
  xTaskCreatePinnedToCore(
    linear_act,    //Task functions
    "linear_act",  //Task name
    10000,     //Stack size
    NULL,      //Task parameters
    1,         //Task priority
    NULL,      //Task handle
    0          //Cores to assign task
  );

  xTaskCreatePinnedToCore(
    stepper,    //Task functions
    "stepper",  //Task name
    10000,     //Stack size
    NULL,      //Task parameters
    1,         //Task priority
    NULL,      //Task handle
    0          //Cores to assign task
  );

  Wire.onReceive(receiveData);

  gpio_pad_select_gpio(GPIO_NUM_13);
  gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
  
  gpio_pad_select_gpio(GPIO_NUM_27);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);

  adc2_config_channel_atten(ADC2_CHANNEL_6, ADC_ATTEN_DB_11); //adc14
  adc2_config_channel_atten(ADC2_CHANNEL_5, ADC_ATTEN_DB_11); //adc12

  ledcSetup(0, 1000, 10);
  ledcAttachPin(25, 0);

  ledcSetup(1, 1000, 10);
  ledcAttachPin(26, 1);

  ledcSetup(2, 1000, 10);
  ledcAttachPin(32, 2);

  ledcSetup(3, 1000, 10);
  ledcAttachPin(33, 3);
  
  Wire.begin(I2C_SLAVE_ADDRESS);
  
  Serial.begin(115200);
  
  int setupdata = 640;
  xQueueSend(xQueue, &setupdata, portMAX_DELAY);
}

void loop() {
  int adcValue1, adcValue2;  
  adc2_get_raw(ADC2_CHANNEL_6, ADC_WIDTH_12Bit, &adcValue1); //adc14
  if(adcValue1 > 3000){
    gpio_set_level(GPIO_NUM_18, 1);
    gpio_set_level(GPIO_NUM_4, 1);
    if(adcValue1 > 4086){
      for(int x = 0; x < 10; x++){
        gpio_set_level(GPIO_NUM_19, 1);  
        gpio_set_level(GPIO_NUM_5, 1);          
        vTaskDelay(1 / portTICK_PERIOD_MS);
        
        gpio_set_level(GPIO_NUM_19, 0);  
        gpio_set_level(GPIO_NUM_5, 0);         
        vTaskDelay(1 / portTICK_PERIOD_MS);
      }
    }
    else{
      for(int x = 0; x < 10; x++){
        gpio_set_level(GPIO_NUM_19, 1);  
        gpio_set_level(GPIO_NUM_5, 1);          
        vTaskDelay(2 / portTICK_PERIOD_MS);
        
        gpio_set_level(GPIO_NUM_19, 0);  
        gpio_set_level(GPIO_NUM_5, 0);         
        vTaskDelay(2 / portTICK_PERIOD_MS);
      }
    }
  }

  if(adcValue1 < 1000){
    gpio_set_level(GPIO_NUM_18, 0);
    gpio_set_level(GPIO_NUM_4, 0);
    if(adcValue1 < 5){
      for(int x = 0; x < 10; x++){
        gpio_set_level(GPIO_NUM_19, 1);  
        gpio_set_level(GPIO_NUM_5, 1);          
        vTaskDelay(1 / portTICK_PERIOD_MS);
        
        gpio_set_level(GPIO_NUM_19, 0);  
        gpio_set_level(GPIO_NUM_5, 0);         
        vTaskDelay(1 / portTICK_PERIOD_MS);
      }
    }
    else{
      for(int x = 0; x < 10; x++){
        gpio_set_level(GPIO_NUM_19, 1);  
        gpio_set_level(GPIO_NUM_5, 1);          
        vTaskDelay(2 / portTICK_PERIOD_MS);
        
        gpio_set_level(GPIO_NUM_19, 0);  
        gpio_set_level(GPIO_NUM_5, 0);         
        vTaskDelay(2 / portTICK_PERIOD_MS);
      }
    }
  }
  
  adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_12Bit, &adcValue2); //adc12
  if(adcValue2 > 3000 && currentState != 1) {
    ledcWrite(0, 970);
    ledcWrite(1, 0);
    ledcWrite(2, 1000);
    ledcWrite(3, 0);
    currentState = 1;
  }

  if(adcValue2 < 1000 && currentState != 2) {
    ledcWrite(0, 0);
    ledcWrite(1, 965);
    ledcWrite(2, 0);
    ledcWrite(3, 1015);
    currentState = 2;
  }

  if(adcValue2 >= 1000 && adcValue2 <= 3000 && currentState != 0) {
    ledcWrite(0, 0);
    ledcWrite(1, 0);    
    ledcWrite(2, 0);
    ledcWrite(3, 0);
    currentState = 0;
  }

  currentsens1 = gpio_get_level(GPIO_NUM_13);  //fire
  if(lastsens1 == 1 && currentsens1 == 0){
    for(int i = 0; i < 3; i++){
      gpio_set_level(GPIO_NUM_27, 1);
      vTaskDelay(4 / portTICK_PERIOD_MS);
      gpio_set_level(GPIO_NUM_27, 0);
      vTaskDelay(26 / portTICK_PERIOD_MS);
      gpio_set_level(GPIO_NUM_27, 0);
    }
  }
  lastsens1 = currentsens1;
}
