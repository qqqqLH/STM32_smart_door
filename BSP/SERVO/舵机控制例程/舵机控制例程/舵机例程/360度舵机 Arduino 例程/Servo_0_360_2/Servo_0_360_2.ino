/***********************************************************************
 * 使用舵机库
 * 360°舵机慢速正转3秒，停止1秒；再快速正转3秒，停止1秒；再慢速反转3秒，停止1秒；再快速反转3秒，停止1秒；如此往返。
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, in version 3.
 * learn more you can see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 */
#include <Servo.h>  //调用舵机库
   Servo myservo;  // 定义舵机对象
   void setup()
   {
      myservo.attach(7);  // 设置舵机控制针脚接数字7引脚
   }
   void loop()
  {
        myservo.write(45);  delay(3000); //占空比为1.0ms的PWM信号旋转约3秒时间
        myservo.write(90);  delay(1000); //占空比为1.5ms的PWM信号停止1秒
        
        myservo.write(0);   delay(3000); //占空比为0.5ms的PWM信号旋转约3秒时间
        myservo.write(90);  delay(1000); //占空比为1.5ms的PWM信号停止1秒
        
        myservo.write(135); delay(3000); //占空比为2.0ms的PWM信号旋转约3秒时间
        myservo.write(90);  delay(1000); //占空比为1.5ms的PWM信号停止1秒
        
        myservo.write(180); delay(3000); //占空比为2.5ms的PWM信号旋转约3秒时间
        myservo.write(90);  delay(1000); //占空比为1.5ms的PWM信号停止1秒
   }
