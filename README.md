# “电控组第二次培训作业”
- **赵志展**
- ***20253006896***

方式：使用依靠 SysTick 滴答定时器   
原理：HAL_GetTick()返回系统上电后毫秒时间戳；不原地等待，每次循环对比时间差，到 500ms 才切换 LED，其余时间主循环可以跑别的代码。   

核心代码：   
uint32_t last_tick = 0;   // 设置初始LED的时间点,32位无序号整形   
uint8_t led_state = 0;    // 流水灯标记：0=LED0，1=LED1，2=LED2

while (1)   
{  
    uint32_t now = HAL_GetTick();         
  // 读取当前系统毫秒时间戳，保存到now变量   
 
  if(now - last_tick >= 500)    
   // 判断当前时间 - 上次动作时间 >= 500ms是否成立，成立代表延时时间到   
  {    
    last_tick = now;   // 更新“上次动作时间”为当前时间，为下一次计时做准备

// 先把三个LED全部熄灭，保证同一时间只有一个灯亮，实现流水效果
HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

// 根据状态点亮对应LED   
 switch(led_state)   
{
 case 0:   
HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET); break;     
case 1:    
HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET); break;    
case 2:     
HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET); break;  
    }   
// led自增，对3取模，实现 0→1→2→0 循环   
    led_state = (led_state + 1) % 3;   
  }   
  // ========== 这里可以放其他任务代码 ==========
  
}