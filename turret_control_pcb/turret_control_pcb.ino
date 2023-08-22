#include "Wire.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_SLAVE_ADDRESS 0x40


const int MAX_INPUT_I2C = 13; // 12자리 숫자 + NULL 종료 문자

char inputBuffer[MAX_INPUT_I2C];
int inputIndex = 0;

char received_data[13];


QueueHandle_t xQueue;

QueueHandle_t yQueue;

int adcValue1 = 0;
int adcValue2 = 0;
int currentState = -1;  // -1: 초기 상태, 0: 정지, 1: 정방향, 2: 역방향


void stepper(void *pvParameters) {
  int receivedValue;
  static int prevReceivedValue = 432;  // 초기값을 512로 설정하여 시작점을 중앙으로 가정
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
      if(receivedValue >= 1023) receivedValue = 1023;
      if(receivedValue <= 0) receivedValue = 0;  

      difference = receivedValue - prevReceivedValue;  // 현재 좌표와 이전 좌표의 차이 계산

      if (abs(difference) <= 8) {       
        moving = 0;
        continue;
      }

      int halfDifference = abs(difference) / 8;

      if(difference > 0) {  // 양의 방향으로 회전
        gpio_set_level(GPIO_NUM_18, 1);
        gpio_set_level(GPIO_NUM_4, 1);
        for(int i=0; i<halfDifference; i++) {
          gpio_set_level(GPIO_NUM_19, 1);  
          gpio_set_level(GPIO_NUM_5, 1);          
          vTaskDelay(4 / portTICK_PERIOD_MS);
        
          gpio_set_level(GPIO_NUM_19, 0);  
          gpio_set_level(GPIO_NUM_5, 0);         
          vTaskDelay(4 / portTICK_PERIOD_MS);
        }
      } 
      else if(difference < 0) {  // 음의 방향으로 회전     
        gpio_set_level(GPIO_NUM_18, 0);
        gpio_set_level(GPIO_NUM_4, 0);
        for(int i=0; i<halfDifference; i++) {  // 차이의 절대값만큼 반복
          gpio_set_level(GPIO_NUM_19, 1);  
          gpio_set_level(GPIO_NUM_5, 1);          
          vTaskDelay(4 / portTICK_PERIOD_MS);
        
          gpio_set_level(GPIO_NUM_19, 0);  
          gpio_set_level(GPIO_NUM_5, 0);         
          vTaskDelay(4 / portTICK_PERIOD_MS);
        }
      }
      prevReceivedValue = 432; 
      moving = 0;

      int dummy;
      while (xQueueReceive(xQueue, &dummy, 0)) {
      }
    }
  }
}

void linear_act(void *pvParameters) {
  int receivedValue;
  static int prevValue = 240;
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
      int difference = (receivedValue - prevValue)*0.5;
      if (difference >= 2000) difference = 2000;

      if (abs(difference) <= 10) {       
        moving = 0;
        continue;
      }

      if(difference > 0) {  
        ledcWrite(0, 985);
        ledcWrite(1, 0);
    
        ledcWrite(2, 1020);
        ledcWrite(3, 0);
        
        vTaskDelay(abs(difference) / portTICK_PERIOD_MS);  
        
        ledcWrite(0, 0);
        ledcWrite(1, 0);    
        ledcWrite(2, 0);
        ledcWrite(3, 0);
      } 
      else if(difference < 0) {  
        ledcWrite(0, 0);
        ledcWrite(1, 985);
    
        ledcWrite(2, 0);
        ledcWrite(3, 1020); 
        
        vTaskDelay(abs(difference) / portTICK_PERIOD_MS);  

        ledcWrite(0, 0);
        ledcWrite(1, 0);    
        ledcWrite(2, 0);
        ledcWrite(3, 0);
      }

      prevValue = 240;  // 현재 값을 이전값으로 저장
      moving = 0;

      int dummy;
      while (xQueueReceive(yQueue, &dummy, 0)) {
      }
    }
  }
}

void receiveData(int bytes) {
  int i = 0;
  while (Wire.available()) {
    char c = Wire.read();
    if (c != '\0' && i < 12) {  // NULL 문자를 건너뛰도록 수정
      received_data[i] = c;
      i++;
    }
  }
  received_data[i] = '\0';  // 문자열 마지막에 NULL 추가

  char part1[5];
  char part2[5];
  char part3[5];

  strncpy(part1, received_data, 4);
  part1[4] = '\0';

  strncpy(part2, received_data + 4, 4);
  part2[4] = '\0';

  strncpy(part3, received_data + 8, 4);
  part3[4] = '\0';

  Serial.println("receive");

  int intPart1 = atoi(part1);
  int intPart2 = atoi(part2);
  int intPart3 = atoi(part3);

  xQueueSend(xQueue, &intPart1, portMAX_DELAY);
  xQueueSend(yQueue, &intPart2, portMAX_DELAY);
}


void setup() {
  xQueue = xQueueCreate(10, sizeof(int));

  yQueue = xQueueCreate(10, sizeof(int));
  
  xTaskCreatePinnedToCore(
    linear_act,    // 태스크 함수
    "linear_act",  // 태스크 이름
    10000,     // 스택 크기
    NULL,      // 태스크에 전달할 매개변수
    1,         // 우선순위
    NULL,      // 태스크 핸들 (필요하지 않을 경우 NULL)
    0          // 태스크를 할당할 코어 번호 (0번 코어에 고정)
  );

  xTaskCreatePinnedToCore(
    stepper,    // 태스크 함수
    "stepper",  // 태스크 이름
    10000,     // 스택 크기
    NULL,      // 태스크에 전달할 매개변수
    1,         // 우선순위
    NULL,      // 태스크 핸들 (필요하지 않을 경우 NULL)
    0          // 태스크를 할당할 코어 번호 (0번 코어에 고정)
  );

  ledcSetup(0, 1000, 10);
  ledcAttachPin(25, 0);

  ledcSetup(1, 1000, 10);
  ledcAttachPin(26, 1);

  ledcSetup(2, 1000, 10);
  ledcAttachPin(32, 2);

  ledcSetup(3, 1000, 10);
  ledcAttachPin(33, 3);
  
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(receiveData);
  
  Serial.begin(115200);
  
  int setupdata = 432;
  xQueueSend(xQueue, &setupdata, portMAX_DELAY);
}

void loop() {
    
  adcValue1 = analogRead(14);
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

  adcValue2 = analogRead(12);
  if(adcValue2 > 3000 && currentState != 1) {
    ledcWrite(0, 985);
    ledcWrite(1, 0);
    ledcWrite(2, 1020);
    ledcWrite(3, 0);
    currentState = 1;
  }

  if(adcValue2 < 1000 && currentState != 2) {
    ledcWrite(0, 0);
    ledcWrite(1, 985);
    ledcWrite(2, 0);
    ledcWrite(3, 1020);
    currentState = 2;
  }

  if(adcValue2 >= 1000 && adcValue2 <= 3000 && currentState != 0) {
    ledcWrite(0, 0);
    ledcWrite(1, 0);    
    ledcWrite(2, 0);
    ledcWrite(3, 0);
    currentState = 0;
  }
}
