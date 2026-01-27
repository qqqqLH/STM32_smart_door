/***********************************************************************
 * 不使用舵机库
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
int servopin = 7;    //定义舵机信号引脚接数字7脚
void servopulse(int angle)  //PWM型号产生函数
{
  int pulsewidth = (angle * 11) + 500; 
  digitalWrite(servopin, HIGH);   
  delayMicroseconds(pulsewidth); 
  digitalWrite(servopin, LOW);    
  delayMicroseconds(20000 - pulsewidth);
}
void setup()
{
  pinMode(servopin, OUTPUT);  //定义数字7引脚为输出
}
void loop()
{       
  for(int i=0;i<1000;i++) { servopulse(45); }  //占空比为1.0ms的PWM信号旋转约3秒时间
  delay(1000);                                 //停止1秒钟
  
  for(int i=0;i<1000;i++) { servopulse(0) ; }  //占空比为0.5ms的PWM信号旋转约3秒时间  
  delay(1000);                                 //停止1秒钟    
  
  for(int i=0;i<1000;i++) {servopulse(135); }  //占空比为2.0ms的PWM信号旋转约3秒时间 
  delay(1000);                                 //停止1秒钟 
  
  for(int i=0;i<1000;i++) {servopulse(180); }  //占空比为2.5ms的PWM信号旋转约3秒时间 
  delay(1000);                                 //停止1秒钟 
  






      
}
