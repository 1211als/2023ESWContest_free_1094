#include "driver/gpio.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "soc/rtc_wdt.h"


#define RXD2 16  // UART2 RX 핀 번호 (필요한 경우 변경)
#define TXD2 17  // UART2 TX 핀 번호 (필요한 경우 변경)

#define I2C_SLAVE_ADDRESS 0x40


const int MAX_INPUT_I2C = 13;  //12 digit + null

char inputBuffer[MAX_INPUT_I2C];
int inputIndex = 0;

char received_data[14];

int modeset = 1;


QueueHandle_t xQueue;

QueueHandle_t yQueue;

QueueHandle_t zQueue;

QueueHandle_t mQueue;

int adcValue1 = 0;
int adcValue2 = 0;
int currentState = -1;   //-1: Initial state, 0: Stop, 1: Forward, 2: Reverse

bool isValidDataFormat1(const char *data) {
  if (strlen(data) != 8) {
    return false;
  }
  for (int i = 0; i < 8; i++) {
    if (!isdigit(data[i])) {
      return false;
    }
  }
  return true;
}

bool isValidDataFormat2(const char *data) {
  if (strlen(data) != 4) {
    return false;
  }
  for (int i = 0; i < 4; i++) {
    if (!isdigit(data[i])) {
      return false;
    }
  }
  return true;
}

void stepper(void *pvParameters) {  //Task to drive the stepper motor with the input coordinates
  int receivedValue;
  char receivedData[9];
  static int prevReceivedValue = 640;  //Set initial value to 640
  int difference;
  int distancevalue;
  int moving = 0;
  int delayS = 4;
  int divide = 10;

  char part1[5];
  char part2[5];
  int intPart1;
  int intPart2;

  while (1) {
    if (!moving && xQueueReceive(xQueue, &receivedData, portMAX_DELAY)) {
      if (isValidDataFormat1(receivedData)) {
        strncpy(part1, receivedData, 4);
        part1[4] = '\0';  // Null terminate the substring

        strncpy(part2, receivedData + 4, 4);
        part2[4] = '\0';  // Null terminate the substring

        receivedValue = atoi(part1);
        distancevalue = atoi(part2);

        if (distancevalue == 0 || distancevalue > 5000) continue;

        if (distancevalue > 2100) {
          delayS = 4;
          divide = 20;
        }
        else if (distancevalue <= 2100) {
          delayS = 2;
          divide = 16;
        }

        if (receivedValue == 0) continue;
        moving = 1;
        if (receivedValue >= 1279) receivedValue = 1279;
        if (receivedValue <= 0) receivedValue = 0;

        difference = receivedValue - prevReceivedValue;  //Calculate the difference

        if (abs(difference) <= 10) {
          moving = 0;
          continue;
        }

        int halfDifference = abs(difference) / divide;

        if (difference >= 0) { //Forward
          gpio_set_level(GPIO_NUM_18, 1);
          gpio_set_level(GPIO_NUM_4, 1);
          for (int i = 0; i < halfDifference; i++) { //Repeat as much as the absolute value of the difference
            gpio_set_level(GPIO_NUM_19, 1);
            gpio_set_level(GPIO_NUM_5, 1);
            vTaskDelay(delayS / portTICK_PERIOD_MS);

            gpio_set_level(GPIO_NUM_19, 0);
            gpio_set_level(GPIO_NUM_5, 0);
            vTaskDelay(delayS / portTICK_PERIOD_MS);

          }
        }
        else if (difference < 0) { //Reverse
          gpio_set_level(GPIO_NUM_18, 0);
          gpio_set_level(GPIO_NUM_4, 0);
          for (int i = 0; i < halfDifference; i++) { //Repeat as much as the absolute value of the difference
            gpio_set_level(GPIO_NUM_19, 1);
            gpio_set_level(GPIO_NUM_5, 1);
            vTaskDelay(delayS / portTICK_PERIOD_MS);

            gpio_set_level(GPIO_NUM_19, 0);
            gpio_set_level(GPIO_NUM_5, 0);
            vTaskDelay(delayS / portTICK_PERIOD_MS);

          }
        }
        prevReceivedValue = 640;
        moving = 0;



        //      int dummy;
        //      while (xQueueReceive(xQueue, &dummy, 0)) {  //Empty the queue
        //      }
      }
      else {
        // Handle unexpected format
        continue;
      }
//      UBaseType_t uxHighWaterMark;

      // 현재 태스크의 High Water Mark 값을 가져옵니다.
//      uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

      // 사용되지 않은 최소 스택 크기를 Serial로 출력합니다.
//      Serial.print("Unused stack space: ");
//      Serial.println(uxHighWaterMark * sizeof(StackType_t)); // 바이트 단위로 변환
    }
  }
}

void linearact(void *pvParameters) {  //Task to drive the linear actuator with the input coordinates
  int receivedValue;
  static int prevValue = 360;  //Set initial value to 360
  int moving = 0;
  int difference = 1;

  char receivedData[5];

  int distancevalue;

  int divide = 12;

  while (1) {
    if (!moving && xQueueReceive(yQueue, &receivedData, portMAX_DELAY)) {
      if (isValidDataFormat2(receivedData)) {
        receivedValue = atoi(receivedData);
        moving = 1;
        if (receivedValue == 0) continue;
        difference = (receivedValue - prevValue) / divide;  //Calculate the difference

        //      Serial.println(abs(difference));

        if (abs(difference) <= 3) {
          moving = 0;
          continue;
        }

        if (difference >= 0) {
          for (int i = 0; i < (difference); i++) { //Forward
            ledcWrite(0, 970);
            ledcWrite(1, 0);

            ledcWrite(2, 1000);
            ledcWrite(3, 0);

            vTaskDelay(3 / portTICK_PERIOD_MS);
          }

          ledcWrite(0, 0);
          ledcWrite(1, 0);
          ledcWrite(2, 0);
          ledcWrite(3, 0);
        }

        else if (difference < 0) {
          for (int i = 0; i < abs(difference); i++) { //Reverse
            ledcWrite(0, 0);
            ledcWrite(1, 965);

            ledcWrite(2, 0);
            ledcWrite(3, 1000);

            vTaskDelay(3 / portTICK_PERIOD_MS);
          }
          ledcWrite(0, 0);
          ledcWrite(1, 0);
          ledcWrite(2, 0);
          ledcWrite(3, 0);
        }

        prevValue = 360;
        moving = 0;

        //      int dummy;
        //      while (xQueueReceive(yQueue, &dummy, 0)) {  //Empty the queue
        //      }
      }
      else {
        // Handle unexpected format
        continue;
      }
    }
  }
}

void linearsol(void *pvParameters) {  //Task to drive the linear solenoid

  int lastsens1 = 1;
  int currentsens1 = 1;
  int receivedValue = 0;

  while (1) {
    xQueueReceive(mQueue, &receivedValue, 0);
    if (receivedValue == 0) {
      currentsens1 = gpio_get_level(GPIO_NUM_13);  //fire
      if (lastsens1 == 1 && currentsens1 == 0) {
        gpio_set_level(GPIO_NUM_27, 1);
        vTaskDelay(3 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_27, 0);
        vTaskDelay(27 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_27, 0);
      }
      lastsens1 = currentsens1;
    }

    if (receivedValue == 1) {
      currentsens1 = gpio_get_level(GPIO_NUM_13);  //fire
      if (lastsens1 == 1 && currentsens1 == 0) {
        for (int i = 0; i < 3; i++) {
          gpio_set_level(GPIO_NUM_27, 1);
          vTaskDelay(3 / portTICK_PERIOD_MS);
          gpio_set_level(GPIO_NUM_27, 0);
          vTaskDelay(27 / portTICK_PERIOD_MS);
          gpio_set_level(GPIO_NUM_27, 0);
        }
      }
      lastsens1 = currentsens1;
    }

    if (receivedValue == 2) {
      currentsens1 = gpio_get_level(GPIO_NUM_13);  //fire
      if (lastsens1 == 1 && currentsens1 == 0) {
        while (1) {
          gpio_set_level(GPIO_NUM_27, 1);
          vTaskDelay(3 / portTICK_PERIOD_MS);
          gpio_set_level(GPIO_NUM_27, 0);
          vTaskDelay(37 / portTICK_PERIOD_MS);
          gpio_set_level(GPIO_NUM_27, 0);

          if (gpio_get_level(GPIO_NUM_13) == 1) break;
        }
      }
      lastsens1 = currentsens1;
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void receiveData() {
  int intPart3;
  while (Serial2.available()) {
    int i = 0;
    while (Serial2.available() && i < 13) {
      char c = Serial2.read();
      if (c != '\0') {
        received_data[i] = c;
        i++;
      }
    }
    received_data[i] = '\0';  //Add NULL to end of string

    char part1[5];
    char part2[5];
    char part3[5];
    char part4[2];

    char combinedParts1[9];
    char combinedParts2[9];

    strncpy(part1, received_data, 4);
    part1[4] = '\0';

    strncpy(part2, received_data + 4, 4);
    part2[4] = '\0';

    strncpy(part3, received_data + 8, 4);
    part3[4] = '\0';

    strncpy(part4, received_data + 12, 1);
    part4[1] = '\0';


    strncpy(combinedParts1, part1, 4);
    strncat(combinedParts1, part3, 4);
    combinedParts1[8] = '\0';

    //    strncpy(combinedParts2, part2, 4);
    //    strncat(combinedParts2, part3, 4);
    //    combinedParts2[8] = '\0';

    //    int intPart2 = atoi(part2);  //y
    int intPart4 = atoi(part4);  //mode


    //    Serial.println(combinedParts1);  // USB-to-Serial로 디버그 출력


    if (atoi(part1) != 0) xQueueSend(xQueue, &combinedParts1, portMAX_DELAY);
    if (atoi(part2) != 0) xQueueSend(yQueue, &part2, portMAX_DELAY);
    //    xQueueSend(zQueue, &intPart3, portMAX_DELAY);
    //    xQueueSend(z2Queue, &intPart3_v2, portMAX_DELAY);
    xQueueSend(mQueue, &intPart4, portMAX_DELAY);
  }
}


void setup() {
  xQueue = xQueueCreate(15, 15);  //Create Queue

  yQueue = xQueueCreate(15, 15);  //Create Queue

  mQueue = xQueueCreate(15, sizeof(int));  //Create Queue


  xTaskCreatePinnedToCore(
    stepper,    //Task functions
    "stepper",  //Task name
    4000,     //Stack size
    NULL,      //Task parameters
    1,         //Task priority
    NULL,      //Task handle
    0          //Cores to assign task
  );

  xTaskCreatePinnedToCore(
    linearact,    //Task functions
    "linearact",  //Task name
    4000,     //Stack size
    NULL,      //Task parameters
    1,         //Task priority
    NULL,      //Task handle
    0          //Cores to assign task
  );

  xTaskCreatePinnedToCore(
    linearsol,    //Task functions
    "linearsol",  //Task name
    4000,     //Stack size
    NULL,      //Task parameters
    1,         //Task priority
    NULL,      //Task handle
    1          //Cores to assign task
  );

  ledcSetup(0, 1000, 10);
  ledcAttachPin(25, 0);

  ledcSetup(1, 1000, 10);
  ledcAttachPin(26, 1);

  ledcSetup(2, 1000, 10);
  ledcAttachPin(32, 2);

  ledcSetup(3, 1000, 10);
  ledcAttachPin(33, 3);

  gpio_pad_select_gpio(GPIO_NUM_19);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_NUM_5);
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_NUM_18);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

  gpio_pad_select_gpio(GPIO_NUM_4);
  gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);


  gpio_pad_select_gpio(GPIO_NUM_13);
  gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);

  gpio_pad_select_gpio(GPIO_NUM_27);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);


  adc2_config_channel_atten(ADC2_CHANNEL_6, ADC_ATTEN_DB_11); //adc14
  adc2_config_channel_atten(ADC2_CHANNEL_5, ADC_ATTEN_DB_11); //adc12

  //  rtc_wdt_protect_off();    // Turns off the automatic wdt service
  //  rtc_wdt_enable();         // Turn it on manually
  // rtc_wdt_set_time(RTC_WDT_STAGE0, 20000);  // Define how long you desire to let dog wait.

  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  //  int setupdata = 640;
  //  xQueueSend(xQueue, &setupdata, portMAX_DELAY);

  int setupmode = 1;
  xQueueSend(mQueue, &setupmode, portMAX_DELAY);
}

void loop() {
  //  Serial.println(gpio_get_level(GPIO_NUM_13));
  receiveData();

  int adcValue1, adcValue2;
  adc2_get_raw(ADC2_CHANNEL_6, ADC_WIDTH_12Bit, &adcValue1); //adc14
  if (adcValue1 > 3000) {
    gpio_set_level(GPIO_NUM_18, 1);
    gpio_set_level(GPIO_NUM_4, 1);
    if (adcValue1 > 4086) {
      for (int x = 0; x < 10; x++) {
        gpio_set_level(GPIO_NUM_19, 1);
        gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_19, 0);
        gpio_set_level(GPIO_NUM_5, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
      }
    }
    else {
      for (int x = 0; x < 10; x++) {
        gpio_set_level(GPIO_NUM_19, 1);
        gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(12 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_19, 0);
        gpio_set_level(GPIO_NUM_5, 0);
        vTaskDelay(12 / portTICK_PERIOD_MS);
      }
    }
  }

  if (adcValue1 < 1000) {
    gpio_set_level(GPIO_NUM_18, 0);
    gpio_set_level(GPIO_NUM_4, 0);
    if (adcValue1 < 5) {
      for (int x = 0; x < 10; x++) {
        gpio_set_level(GPIO_NUM_19, 1);
        gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_19, 0);
        gpio_set_level(GPIO_NUM_5, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
      }
    }
    else {
      for (int x = 0; x < 10; x++) {
        gpio_set_level(GPIO_NUM_19, 1);
        gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(12 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_19, 0);
        gpio_set_level(GPIO_NUM_5, 0);
        vTaskDelay(12 / portTICK_PERIOD_MS);
      }
    }
  }

  adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_12Bit, &adcValue2); //adc12
  if (adcValue2 > 3000 && currentState != 1) {
    ledcWrite(0, 970);
    ledcWrite(1, 0);
    ledcWrite(2, 1000);
    ledcWrite(3, 0);
    currentState = 1;
  }

  if (adcValue2 < 1000 && currentState != 2) {
    ledcWrite(0, 0);
    ledcWrite(1, 965);
    ledcWrite(2, 0);
    ledcWrite(3, 1015);
    currentState = 2;
  }

  if (adcValue2 >= 1000 && adcValue2 <= 3000 && currentState != 0) {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    ledcWrite(2, 0);
    ledcWrite(3, 0);
    currentState = 0;
  }
}
