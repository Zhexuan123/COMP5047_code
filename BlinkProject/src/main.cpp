#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BluetoothSerial.h>
#include <Adafruit_NeoPixel.h>
#include "SD.h"                    // SD卡库
#include "DFRobotDFPlayerMini.h"    // 引入DFPlayer Mini库

#define LIGHT_SENSOR_PIN_ENV 34  // 环境光传感器引脚
#define LIGHT_SENSOR_PIN_HAT 35  // 帽子检测光传感器引脚
#define SD_CARD_CS 5              // SD卡的CS引脚
#define SPEAKER_PIN 25            // 扬声器引脚
#define LED_PIN 14                // LED灯带数据引脚 (已连接到D6)
#define NUM_LEDS 30               // LED灯带上LED的数量
#define DFPLAYER_TX 3             // DFPlayer TX引脚 (已连接到D3)
#define DFPLAYER_RX 2             // DFPlayer RX引脚 (已连接到D2)

Adafruit_MPU6050 mpu;                        // MPU6050传感器对象
BluetoothSerial SerialBT;                    // 蓝牙串口对象
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800); // LED灯带对象
HardwareSerial mySerial(1);                  // 使用 ESP32 的 UART1 硬件串口
DFRobotDFPlayerMini myDFPlayer;              // DFPlayer Mini对象

int distanceTravelled = 0;                   // 行驶的距离
int stepCounter = 0;                         // 步数计数器
bool hatOn = false;                          // 用于记录是否检测到帽子戴上
int litLeds = 0;                             // 当前点亮的LED数量
bool isDark = false;                         // 用于判断环境是否变暗
String currentAction = "jump";               // 当前任务（开始为跳跃任务）

void setup() {
    Serial.begin(115200);                    // 初始化串口
    pinMode(LIGHT_SENSOR_PIN_ENV, INPUT);    // 设置环境光传感器引脚为输入
    pinMode(LIGHT_SENSOR_PIN_HAT, INPUT);    // 设置帽子检测光传感器引脚为输入
    strip.begin();                           // 初始化LED灯带
    strip.show();                            // 更新LED灯带状态

    // SD卡初始化
    if (!SD.begin(SD_CARD_CS)) {
        Serial.println("SD 卡初始化失败");
        return;
    }
    Serial.println("SD 卡初始化成功");

    // DFPlayer Mini初始化
    mySerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);  // 初始化硬件串口
    if (!myDFPlayer.begin(mySerial)) {        // 初始化DFPlayer Mini
        Serial.println("DFPlayer Mini 初始化失败");
        while (true);
    }

    // MPU6050初始化
    if (!mpu.begin()) {
        Serial.println("未找到MPU6050芯片");
        while (1) { delay(10); }
    }
    SerialBT.begin("Hat_BT");                // 初始化蓝牙
}

void playActionPrompt(String action) {
    if (action == "jump") {
        myDFPlayer.play(1);                 // 播放SD卡上的第1个音频文件（跳跃提示）
    } else if (action == "run") {
        myDFPlayer.play(2);                 // 播放SD卡上的第2个音频文件（跑步提示）
    }
}

void playTaskCompletePrompt() {
    myDFPlayer.play(4);                     // 播放SD卡上的第4个音频文件（任务完成提示）
}

void playHatOnPrompt() {
    myDFPlayer.play(3);                     // 播放SD卡上的第3个音频文件（戴上帽子的提示）
}

void turnOnAllWhite() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));  // 全部LED灯变为白色
    }
    strip.show();
}

// 关闭所有LED灯
void turnOffBrimLight() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));  // 关闭所有灯光
    }
    strip.show();
}
bool isJumping(float ax, float ay, float az) {
    float totalAcceleration = sqrt(ax * ax + ay * ay + az * az);
    return totalAcceleration > 15;  // 检测加速度是否大于15，判断是否跳跃
}

// 根据加速度计算距离
int calculateDistance(float ax, float ay, float az) {
    float acceleration = sqrt(ax * ax + ay * ay + az * az);
    int distance = acceleration * 0.05;  // 计算距离
    return distance;
}

// 更新已点亮的LED数量
void updateLeds() {
    if (litLeds < strip.numPixels()) {
        litLeds++;  // 每次增加点亮的LED数量
    }
}

void updateLedsWithColor() {
    for (int i = 0; i < litLeds; i++) {
        uint32_t color = strip.Color(0, 0, 255);  // 默认蓝色

        // 为不同的LED位置分配不同的颜色
        if (i % 3 == 0) {
            color = strip.Color(255, 0, 0);  // 红色
        } else if (i % 3 == 1) {
            color = strip.Color(0, 255, 0);  // 绿色
        }

        strip.setPixelColor(i, color);
    }
    strip.show();
}

void loop() {
    int lightValueEnv = analogRead(LIGHT_SENSOR_PIN_ENV);  // 读取环境光传感器值
    int lightValueHat = analogRead(LIGHT_SENSOR_PIN_HAT);  // 读取帽子检测光传感器值

    // 检测帽子是否戴上
    if (lightValueHat < 500) {
        if (!hatOn) {
            hatOn = true;
            playHatOnPrompt();  // 提示戴上帽子后开始任务
            delay(3000);        // 等待提示音播放完成

            // 提示第一个任务
            playActionPrompt(currentAction);  // 提示当前任务（跳跃或跑步）
        }
    } else {
        hatOn = false;  // 如果帽子未戴上，重置状态
    }

    // 控制LED灯光
    if (lightValueEnv < 500) {  // 如果环境光变暗
        if (!isDark) {           // 只在第一次变暗时执行
            isDark = true;
            turnOnAllWhite();    // 将所有LED灯变为白色
        }
    } else {
        isDark = false;
        updateLedsWithColor();   // 显示彩色灯光
    }

    // 获取加速度数据
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;

    // 检测跳跃并点亮新LED
    if (isJumping(ax, ay, az)) {
        stepCounter++;
        if (stepCounter >= 10) {  // 每10次跳跃点亮一个新的LED
            updateLeds();
            stepCounter = 0;

            // 当完成跳跃任务后
            playTaskCompletePrompt();  // 提示任务完成
            delay(3000);               // 等待提示音播放完成

            // 切换到下一个任务
            currentAction = (currentAction == "jump") ? "run" : "jump";
            playActionPrompt(currentAction);  // 提示下一步的动作
        }
    }

    // 累积距离并点亮新的LED
    distanceTravelled += calculateDistance(ax, ay, az);
    if (distanceTravelled > 100) { // 每100米点亮一个新的LED
        updateLeds();

        // 当完成跑步任务后
        playTaskCompletePrompt();  // 提示任务完成
        delay(3000);               // 等待提示音播放完成

        // 切换到下一个任务
        currentAction = (currentAction == "run") ? "jump" : "run";
        playActionPrompt(currentAction);  // 提示下一步的动作

        distanceTravelled = 0;
    }
    if (SerialBT.available()) {
        String otherHatSignal = SerialBT.readString();
        if (otherHatSignal == "close") {
            updateLeds();  // 在蓝牙互动期间点亮新的LED
            playTaskCompletePrompt();  // 提示任务完成
            delay(3000);
            currentAction = (currentAction == "jump") ? "run" : "jump";
            playActionPrompt(currentAction);  // 提示下一步的动作
        }
    }

    delay(100);
}