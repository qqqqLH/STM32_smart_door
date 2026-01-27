/***********************************************************************
 * 不使用舵机库
 * 舵机在180°内自动往返旋转
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
  for ( int angle = 0; angle < 180; angle += 2)
  {
    for (int i = 0; i < 5; i++) 
    {
      servopulse(angle);  
    }
  }
  for ( int angle = 180; angle>0; angle -= 2)
  {
    for (int i = 0; i < 5; i++) 
    {
      servopulse(angle); 
    }
  }
}
