#include "Wire.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_SLAVE_ADDRESS 0x40

char receivedData[12];
int dataIndex = 0;


QueueHandle_t xQueue;

QueueHandle_t yQueue;

int adcValue1 = 0;
int adcValue2 = 0;

void stepper(void *pvParameters) {
  int receivedValue;
  
  gpio_pad_select_gpio(GPIO_NUM_19);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
  
  gpio_pad_select_gpio(GPIO_NUM_5);
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
  
  while(1) {
    if (xQueueReceive(xQueue, &receivedValue, portMAX_DELAY)) {
      for(int i=0; i<receivedValue; i++) {
        gpio_set_level(GPIO_NUM_19, 1);  
        gpio_set_level(GPIO_NUM_5, 1);          
        vTaskDelay(3 / portTICK_PERIOD_MS);
        
        gpio_set_level(GPIO_NUM_19, 0);  
        gpio_set_level(GPIO_NUM_5, 0);         
        vTaskDelay(3 / portTICK_PERIOD_MS);
      }
    }
  }
}

void linear_act(void *pvParameters) {
  int receivedValue;
  unsigned long previousMillis = 0;
  bool isHigh = false;
  while(1) {
    if (xQueueReceive(yQueue, &receivedValue, portMAX_DELAY)) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= receivedValue) {
        previousMillis = currentMillis;
        if (isHigh) {

        } else {

        }
        isHigh = !isHigh;
      }
    }
  }
}

void receiveData(int byteCount) {
  Serial.print("Data received: ");
  while (Wire.available()) {
    char c = Wire.read(); // 한 바이트를 읽습니다.
    Serial.print(c);
  }
  Serial.println(); // 줄 바꿈을 출력합니다.
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

  gpio_pad_select_gpio(GPIO_NUM_18);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_NUM_17);
  gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);


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
}

void loop() {
  adcValue1 = analogRead(14);        
  if(adcValue1 > 2000){
    gpio_set_level(GPIO_NUM_18, 1);
    gpio_set_level(GPIO_NUM_17, 1); 
    int i = 1;
    xQueueSend(xQueue, &i, portMAX_DELAY);
  }

  if(adcValue1 < 1500){
    gpio_set_level(GPIO_NUM_18, 0);
    gpio_set_level(GPIO_NUM_17, 0);
    int j = 1;
    xQueueSend(xQueue, &j, portMAX_DELAY);
  }

//  xQueueSend(xQueue, &j, portMAX_DELAY);

  adcValue2 = analogRead(12);
  if(adcValue2 > 2000){
    ledcWrite(0, 985);
    ledcWrite(1, 0);
    
    ledcWrite(2, 1020);
    ledcWrite(3, 0);
  }

  if(adcValue2 < 2000 && adcValue2 > 1500){
    ledcWrite(0, 0);
    ledcWrite(1, 0);    
    ledcWrite(2, 0);
    ledcWrite(3, 0);
  }

  if(adcValue2 < 1500){
    ledcWrite(0, 0);
    ledcWrite(1, 985);
    
    ledcWrite(2, 0);
    ledcWrite(3, 1020);
  }

  if(adcValue2 > 1500 && adcValue2 < 2000){
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    
    ledcWrite(2, 0);
    ledcWrite(3, 0);
  }

}
