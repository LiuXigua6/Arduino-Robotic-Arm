#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// 记录5个关节的当前角度（0=底座, 1=大臂, 2=小臂, 3=腕部, 4=夹爪）
int angles[5] = {90, 90, 90, 90, 90};

void setup() {
  Serial.begin(9600);
  pwm.begin();
  pwm.setPWMFreq(50); 
  delay(100);
  
  Serial.println("=== 系统就绪 ===");
  Serial.println("【点动模式】输入 w/s/a/d/q/e/r/f/t/g 控制关节微调");
  Serial.println("【目标模式】输入 关节号,角度 (例: 0,90)");
  Serial.println("【一键复位】输入 R");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // ================= 1. 一键复位模式 =================
    if (cmd == "R" || cmd == "r") {
      Serial.println("【复位】所有关节回到90度，请等待...");
      // 依次将5个关节平滑移动到90度
       for (int i = 0; i < 5; i++) {
        angles[i] = 90;          // 1. 先把软件记忆更新为90
        setServoAngle(i, 90);    // 2. 直接发送脉冲，强制舵机回90度
        delay(300);              // 3. 等300毫秒，让舵机转到位
      }
      Serial.println("【复位完成】");
      return; // 执行完复位，直接跳过本次循环
    }

    // ================= 2. 点动模式（单字符） =================
    if (cmd.length() == 1) {
      char key = cmd.charAt(0); // 提取这个字母
      int joint = -1;           // 记录要动哪个关节
      int step = 0;             // 记录是加角度还是减角度

      // 判断按了哪个键，对应哪个关节
      if (key == 'w') { joint = 0; step = 2; }
      if (key == 's') { joint = 0; step = -2; }
      if (key == 'a') { joint = 1; step = 2; }
      if (key == 'd') { joint = 1; step = -2; }
      if (key == 'q') { joint = 2; step = 2; }
      if (key == 'e') { joint = 2; step = -2; }
      if (key == 'r') { joint = 3; step = 2; }
      if (key == 'f') { joint = 3; step = -2; }
      if (key == 't') { joint = 4; step = 2; }
      if (key == 'g') { joint = 4; step = -2; }

      // 如果确实是有效按键
      if (joint != -1) {
        int newAngle = angles[joint] + step; // 计算新角度
        smoothMove(joint, newAngle);         // 平滑移动过去
      }
      return;
    }

    // ================= 3. 目标位置模式（文本解析） =================
    // 如果输入包含逗号（例如 0,90）
    int commaIndex = cmd.indexOf(',');
    if (commaIndex != -1) {
      // 逗号前面是关节号，逗号后面是角度
      int joint = cmd.substring(0, commaIndex).toInt();
      int ang = cmd.substring(commaIndex + 1).toInt();

      // 检查关节号是否合法 (0 到 4)
      if (joint >= 0 && joint < 5) {
        smoothMove(joint, ang); // 直接调用平滑移动函数（里面包含限位保护）
      } else {
        Serial.println("【错误】关节号只能输入 0 到 4");
      }
    } else {
      Serial.println("【错误】格式不对，请使用 关节号,角度 或 单字母点动");
    }
  }
}

// 核心函数 1：平滑移动函数
void smoothMove(uint8_t channel, int targetAngle) {
  // 1. 安全限位保护
  if (channel == 0) { //底座保护
    if (targetAngle < 0) targetAngle = 0;
    if (targetAngle > 180) targetAngle = 180;
  }
  if (channel == 1) { // 大臂保护
    if (targetAngle < 30) targetAngle = 0;
    if (targetAngle > 150) targetAngle = 180;
  }
  if (channel == 2) { // 小臂保护
    if (targetAngle < 20) targetAngle = 0;
    if (targetAngle > 160) targetAngle = 180;
  }
  if (channel == 3) { //腕部保护
    if (targetAngle < 0) targetAngle = 0;
    if (targetAngle > 180) targetAngle = 180;
  }
  if (channel == 4) { // 夹爪保护
    if (targetAngle < 40) targetAngle = 40;
    if (targetAngle > 140) targetAngle = 140;
  }

  // 2. 开始平滑移动
  int currentAngle = angles[channel]; // 从当前记录的角度开始
  int step = (targetAngle > currentAngle) ? 1 : -1; // 判断是加还是减

  // 只要还没到目标角度，就一步一步走
  while (currentAngle != targetAngle) {
    currentAngle += step;
    setServoAngle(channel, currentAngle); // 发送脉冲让舵机走1度
    delay(8); // 每走1度停8毫秒。
  }
  
  // 3. 更新大脑记忆
  angles[channel] = targetAngle;
  Serial.print("关节 "); Serial.print(channel);
  Serial.print(" 已转到 "); Serial.print(targetAngle); Serial.println(" 度");
}

// 核心函数 2：底层脉冲输出函数
void setServoAngle(uint8_t channel, int angle) {
  // 发现旋向不一致，增加方向反转
  if (channel == 0 || channel == 2 || channel == 4) {
    angle = 180 - angle; 
  }
  
  // 将 0-180 度映射到 PCA9685 的 102-512 脉冲值
  int pulselen = map(angle, 0, 180, 102, 512);
  
  // 发送给驱动板
  pwm.setPWM(channel, 0, pulselen);
}