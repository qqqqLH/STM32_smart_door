#include "app.h"

//手册一页写了支持30级音量调节
static const uint8_t s_volume_table[APP_VOLUME_LEVELS] = { 10u, 15u, 20u, 25u, 30u };

//传入字符串控制在一行之内，屏幕只能显示16列字符，所以超过16列就改为16列
static void App_BuildLine(char *dst, const char *src)
{
  size_t len = strlen(src);
  if (len > 16u)
  {
    len = 16u;
  }
  //memset函数将目标缓冲区指向的16个字节全部填充位空格字符
  memset(dst, ' ', 16);
  //内存拷贝函数，将src的前len个字节全部拷贝到目标缓冲区，并且在第17个位置写入字符串结束符"\0"
  memcpy(dst, src, len);
  dst[16] = '\0';
}

//密码显示格式化函数，用来显示* * * _ _ _这样格式的屏幕密码格式
static void App_BuildPwdLine(uint8_t len, char *dst)
{
  //如果输入密码的长度大于6，也就是超过了6位密码，就返回6，如果显示长度没超过6就返回输入密码的长度
  uint8_t count = (len > APP_PWD_LEN) ? APP_PWD_LEN : len;
  memset(dst, ' ', 16);//清空这一块的16个位置的内容，这一块内容等会要写入屏幕，所以要先清空，填入空格
  for (uint8_t i = 0; i < APP_PWD_LEN; i++)
  {
    uint8_t pos = (uint8_t)(i * 2u);//i=0是pos在0，=1是pos在2，=2时pos在4，每个密码之间隔着一个空格保证美观
    dst[pos] = (i < count) ? '*' : '_';//如果输入了3位，那么len=3，在前三位就是*，后三位就是_,显示输入了几个数字了
  }
  dst[16] = '\0';//在数组的最后加入字符串结束符
}

//指纹模组的中断线ture为高电平也就是按下了就会高电平
//这个函数是用来释放指纹模组的，因为在上升沿触发的过程中我测试了可能触发两次中断
//这个原因可能是因为电平的震荡一会0一会1的，处理完指纹的识别或者录入等流程，手动再开启中断
//我们在进入一次中断之后一直循环读取是否，
static void App_FingerIrqRelease(void)
{
  while (HAL_GPIO_ReadPin(FINGER_IRQ_GPIO_Port, FINGER_IRQ_Pin) == GPIO_PIN_SET)//不断循环读取电平状态
  {
    vTaskDelay(pdMS_TO_TICKS(10));//10ms读取一次，等待指纹模组中断线返回为低电平，就是手指离开了指纹模组，退出循环
  }
  __HAL_GPIO_EXTI_CLEAR_IT(FINGER_IRQ_Pin);//退出循环后把中断标志位清空不然一直触发中断
  HAL_NVIC_EnableIRQ(FINGER_IRQ_EXTI_IRQn);//然后再次使能中断检测
}

//更改配置之后调用整个函数保存配置
static void App_SaveConfig(void)
{
  //1一个魔术数，再读取flash内容之前先比对flash的这个地址的内容是否和我们规定的是一样的，如果一样说明内容正确，不一样说明flash出错
  //2个密码位，8位密码需要8个字符存储，一个uint32_t为32/8=4个字节，所以需要2个字节存储然后把第二个字节剩下的2个字节覆盖为1
  //2位卡位有效性，在之前的指纹模组可以自动返回256个bit来判断哪些存入指纹哪些没有存入，现在我们在flash单独开辟64个bit也就是
  //两个uint32_t，可以记录64张卡是否被存入，0和1代表被存和没有被存
  //1个音量等级存储
  uint32_t data[6 + APP_MAX_CARDS];
  uint32_t idx = 0;//定义数组位置变量

  data[idx++] = APP_CFG_MAGIC;//第一个位置填写魔术数
  //和之前的指纹模组通信一样，stm32是低位在前，我们存入的时候为了好理解，需要把密码改为高位在前，把密码的位置进行调换
  data[idx++] = ((uint32_t)s_password[0] << 24) | ((uint32_t)s_password[1] << 16)
              | ((uint32_t)s_password[2] << 8) | (uint32_t)s_password[3];//前四位占了4字节
  data[idx++] = ((uint32_t)s_password[4] << 24) | ((uint32_t)s_password[5] << 16)
              | 0xFFFFu;//后两位密码
  data[idx++] = (uint32_t)(s_card_valid & 0xFFFFFFFFu);//uint64_t代表64个bit，把低32位打包进uint32_t拆分为两个uint32_t方便存储
  data[idx++] = (uint32_t)(s_card_valid >> 32);//高32位存入下一个数组
  data[idx++] = (uint32_t)s_volume_level;//打包音量等级

  //遍历所有的UID卡片存入data数组，一个卡片的UID是32个bit刚好是一个uint32_t
  for (uint32_t i = 0; i < APP_MAX_CARDS; i++)
  {
    data[idx++] = s_card_uids[i];
  }

  (void)Flash_ErasePage(APP_FLASH_CFG_ADDR);//擦除最后一页
  (void)Flash_WriteArray(APP_FLASH_CFG_ADDR, data, (uint16_t)idx);//把打包好的data数组一整串全部存入最后一页
}

static void App_LoadConfig(void)
{
  //读取一个字，一字4字节，对应魔术数，检验我们存入的flash最后一个位置的内容是否正确
  if (Flash_ReadWord(APP_FLASH_CFG_ADDR) != APP_CFG_MAGIC)
  {//如果不一致说明flash无效，可能是损坏了也可能识别的原因，或者是烧录代码后第一次上电，falsh没有保存我们的魔术数
    //执行初始化任务，也就是把密码清为000000，UID数组也清0，默认的音量为2
    memset(s_password, 0, sizeof(s_password));
    memset(s_card_uids, 0, sizeof(s_card_uids));
    s_card_valid = 0u;
    s_volume_level = 2u;
    App_SaveConfig();//执行保存配置函数
    return;
  }
  //如果正确读取 Flash 中的固定配置项
  uint32_t w1 = Flash_ReadWord(APP_FLASH_CFG_ADDR + 4u);//密码前四位
  uint32_t w2 = Flash_ReadWord(APP_FLASH_CFG_ADDR + 8u);//密码后两位+两个字节的1
  uint32_t w3 = Flash_ReadWord(APP_FLASH_CFG_ADDR + 12u);//卡片有效性低32位
  uint32_t w4 = Flash_ReadWord(APP_FLASH_CFG_ADDR + 16u);//卡片有效性高32位
  uint32_t w5 = Flash_ReadWord(APP_FLASH_CFG_ADDR + 20u);//音量等级
  //反向还原出密码，卡片有效性，音量等等
  //密码还原
  s_password[0] = (uint8_t)(w1 >> 24);
  s_password[1] = (uint8_t)(w1 >> 16);
  s_password[2] = (uint8_t)(w1 >> 8);
  s_password[3] = (uint8_t)w1;
  s_password[4] = (uint8_t)(w2 >> 24);
  s_password[5] = (uint8_t)(w2 >> 16);
  //卡片有效性还原
  s_card_valid = ((uint64_t)w4 << 32) | w3;
  s_volume_level = (uint8_t)w5;//音量还原
  if (s_volume_level >= APP_VOLUME_LEVELS)//如果音量等级大于5说明flash值损坏，把音量值改为默认值2
  {
    s_volume_level = 2u;
  }

  for (uint32_t i = 0; i < APP_MAX_CARDS; i++)//循环读取所有卡片
  {
    s_card_uids[i] = Flash_ReadWord(APP_FLASH_CFG_ADDR + 24u + i * 4u);
  }
}

//将四个8位的uid打包位一个32位的uid，方便存储和比较
static uint32_t App_PackUid(const uint8_t *uid)
{
  return ((uint32_t)uid[0] << 24) | ((uint32_t)uid[1] << 16)
       | ((uint32_t)uid[2] << 8) | (uint32_t)uid[3];
}

//判断指定的卡是否有效，有效返回1无效返回0
static uint8_t App_CardIsValid(uint8_t idx)
{
  if (idx >= APP_MAX_CARDS)//越界就返回
  {
    return 0u;
  }
  //返回卡片有效性，假如idx=0，那么右移0位&0x1等于0则代表无效，等于1则代表有效
  return (uint8_t)((s_card_valid >> idx) & 0x1u);
}

//设置卡的有效性，添加卡删除卡的时候使用
static void App_SetCardValid(uint8_t idx, uint8_t valid)
{
  if (idx >= APP_MAX_CARDS)//越界返回
  {
    return;
  }
  if (valid != 0u)//如果vail=1则设置为有效进入if
  {
    s_card_valid |= (uint64_t)1u << idx;//将1左移送到指定的idx位置与s_card_valid进行或运算，就可以得到指定位置有效
  }
  else
  {
    s_card_valid &= ~((uint64_t)1u << idx);
    s_card_uids[idx] = 0u;
  }
}

static uint8_t App_FindCardSlotByUid(uint32_t uid)
{
  for (uint8_t i = 0; i < APP_MAX_CARDS; i++)
  {//首先判断这个卡是否是1，就是判断有效性，不等于0且uid相等，就是我们要找的这个卡片找到了一模一样的uid了
    if (App_CardIsValid(i) != 0u && s_card_uids[i] == uid)
    {
      return i;
    }
  }
  return 0xFFu;//没找到
}

//上一个函数的简化版仅返回是否存在，如果不相等就返回1代表存在
static uint8_t App_FindCard(uint32_t uid)
{
  return (App_FindCardSlotByUid(uid) != 0xFFu) ? 1u : 0u;
}

//指定索引添加rfid卡
static uint8_t App_AddCardToSlot(uint8_t slot, uint32_t uid)
{
  if (slot >= APP_MAX_CARDS)//判断是否超出最大卡片位置
  {
    return 0u;
  }
  if (App_CardIsValid(slot) != 0u)//判断指定索引的卡片是否被占用，如果被占用了就返回0
  {
    return 0u;
  }
  uint8_t exist = App_FindCardSlotByUid(uid);//查找这个uid是否已经存在了，如果存在就返回0
  if (exist != 0xFFu)
  {
    return 0u;
  }
  s_card_uids[slot] = uid;//安全性判断成功之后就把这个uid存入指定的索引
  App_SetCardValid(slot, 1u);//把相应的有效值改为1，意味着这个位置存入了uid
  App_SaveConfig();//flash保存配置
  return 1u;//返回1.标识添加成功
}

//删除指定索引的卡片
static uint8_t App_RemoveCardSlot(uint8_t slot)
{
  if (slot >= APP_MAX_CARDS)//检查索引有没有超过最大卡片位置
  {
    return 0u;
  }
  if (App_CardIsValid(slot) == 0u)//检查指定索引的位置上是否存在卡片，如果不存在卡片那就说明不能删除，返回
  {
    return 0u;
  }
  App_SetCardValid(slot, 0u);//这个函数会将指定索引的卡片有效性清空，并且删除那个数组位置的卡片uid变为0
  App_SaveConfig();//保存到flash
  return 1u;//成功返回1
}

//调用PcdRequest读取卡片
static uint8_t App_ReadCard(uint32_t *uid)
{
  uint8_t tag_type[2];
  uint8_t snr[4];

  if (PcdRequest(PICC_REQALL, tag_type) != MI_OK)//读取
  {
    return 0u;
  }
  if (PcdAnticoll(snr) != MI_OK)
  {
    return 0u;
  }
  if (uid != NULL)
  {
    *uid = App_PackUid(snr);//读取成功之后这个snr是四个uint8_t的数组组成的调用这个函数来把他拼成一个uint32_t
  }
  return 1u;//成功返回1
}

//row OLED 显示行号
//text 要显示的原始文本
//存储上一次显示的内容，用于对比是否变化
static void App_ShowLine(uint8_t row, const char *text, char *cache)
{
  char line[17];// 存储格式化后的16字符显示行（+1位结束符）
  char blank[17];// 存储16个空格的清空行

  App_BuildLine(line, text);//将原始文本text格式化为 16 字符长度的标准行（超出截断、不足补空格）；
  if (strncmp(cache, line, 16) == 0)//如果没有变化那就不更新了，直接退出
  {
    return;
  }
  //如果有变换，先把blank清空，并且补最后一个结束符
  memset(blank, ' ', 16);
  blank[16] = '\0';
  OLED_ShowString(0, (u8)(row * 2u), (u8 *)blank, APP_OLED_FONT);//然后清空单行，因为如果清空整个屏幕刷新很慢，给一行全写空格最快
  OLED_ShowString(0, (u8)(row * 2u), (u8 *)line, APP_OLED_FONT);//写真正的内容，也就是刷新行
  strncpy(cache, line, 16);//将line复制到cache，作为上次现实的内容，用来和下次的内容进行比对
  cache[16] = '\0';
}

//显示整个屏幕四行只需要调用一次整个函数，把每一行的字符串都传入到这个，不用调用四次函数了
static void App_ShowLines(const char *line1, const char *line2, const char *line3, const char *line4)
{
  App_ShowLine(0u, line1, s_lines[0]);
  App_ShowLine(1u, line2, s_lines[1]);
  App_ShowLine(2u, line3, s_lines[2]);
  App_ShowLine(3u, line4, s_lines[3]);
}

//显示键盘界面
static void App_ShowKeypad(const char *line4)
{//这里如果传入的line4为空，则代表最下面一行不显示密码之类的，就是我们自己定义的 * * * _ _ _ 这样的。显示默认的C 0 R OK
  const char *l4 = (line4 != NULL) ? line4 : "C   0   R  OK";
  App_ShowLines("1   2   3   U", "4   5   6   D", "7   8   9   A", l4);//然后显示默认的界面
}

static void App_ShowPwdEntry(uint8_t len)
{
  char line4[17];//创建一个数组，17个字符用来存放等会要刷新到屏幕上面的内容
  App_BuildPwdLine(len, line4);//构建密码显示第四行显示多少个*和多少个_
  App_ShowKeypad(line4);//刷新按键页面的屏幕，用的行刷新，所以只在第四行进行刷新
}

//显示管理员输入密码的时候的界面
static void App_ShowAdminPwd(uint8_t len)
{
  char line4[17];//同样先创建一个数组来存储等会要写到屏幕上的东西
  App_BuildPwdLine(len, line4);//在最后一行同样写len个*和6-len个_
  App_ShowLines("ADMIN PWD", "", "", line4);
}

//和上面一样，不过内容改为了new pwm在设置新密码的时候用的
static void App_ShowPwdSet(uint8_t len)
{
  char line4[17];
  App_BuildPwdLine(len, line4);
  App_ShowLines("NEW PWD", "", "", line4);
}

//显示管理员菜单
static void App_ShowAdminMenu(void)
{//创建一个s_menu_sel标识符，用来判断当前是显示在第几行，第一行就把">"这个符号显示在第一行
  App_ShowLines(
    (s_menu_sel == 0u) ? ">1.CARD" : " 1.CARD",
    (s_menu_sel == 1u) ? ">2.FINGER" : " 2.FINGER",
    (s_menu_sel == 2u) ? ">3.PWD" : " 3.PWD",
    (s_menu_sel == 3u) ? ">4.VOL" : " 4.VOL");
}

//这个是显示
static void App_ShowCardMenu(void)
{
  char line2[17];//存储第二行的卡片状态文本
  const char *state = "--";// 默认状态
  uint16_t id = s_card_sel_id;// 当前选中的卡片ID
  if (id >= 1u && id <= APP_MAX_CARDS)//判断是否合法
  {
    uint8_t bit = App_CardIsValid((uint8_t)(id - 1u));//获取这个位的卡片有效性，减去1是因为我们之前所有的获取有效性都是0-49，现在显示是要1-50
    state = (bit != 0u) ? "YES" : "NO";//三目运算符判断yes还是no
  }
  (void)snprintf(line2, sizeof(line2), "ID:%03u %s", id, state);//把ID：几 yes/no 封装到line2
  App_ShowLines(//一起显示出来
    "CARD",
    line2,
    "",
    "");
}

//指纹菜单和上面也是一样首先创建一个数组用来显示行，然后判断有效性，然后刷新到屏幕上面
static void App_ShowFingerMenu(void)
{
  char line2[17];
  const char *state = "--";//如果s_finger_index_valid和指纹模组通信失败，一直读取不到信息，那么就无法
  if (s_finger_index_valid != 0u)//指纹有效性索引表s_finger_index是否有效，当成功读取到指纹的索引表的时候会将其变为1
  {
    uint16_t id = s_finger_sel_id;//获取用户当前选中的指纹
    if (id <= APP_FINGER_DB_SIZE)//如果指纹小于等于50才继续运行，不在这个范围说明错了，最大只能寸50枚指纹
    {
      uint8_t bit = (uint8_t)((s_finger_index >> id) & 0x1u);//这个是64个字节的0或者1判断是否有效
      state = (bit != 0u) ? "YES" : "NO";
    }
  }
  (void)snprintf(line2, sizeof(line2), "ID:%03u %s", s_finger_sel_id, state);//把信息打包放进line2
  App_ShowLines("FINGER", line2, "", "");//刷新line2
}

//显示音量菜单
static void App_ShowVolumeMenu(void)
{
  char line2[17];
  uint8_t level = (s_volume_level < APP_VOLUME_LEVELS) ? s_volume_level : 0u;
  (void)snprintf(line2, sizeof(line2), "LEVEL %u/%u", (uint16_t)(level + 1u), (uint16_t)APP_VOLUME_LEVELS);
  App_ShowLines("VOLUME", line2, "", "");
}
//音效播放
static void App_PlayVoice(uint16_t index)
{
  JQ8900_PlayIndex(&s_voice, index);
}

//解锁逻辑
static void App_UnlockOk(void)
{
  App_PlayVoice(1u);//先播放音效
  Servo_Forward(&htim4, TIM_CHANNEL_3);//正转500ms
  vTaskDelay(pdMS_TO_TICKS(APP_SERVO_PULSE_MS));
  Servo_Stop(&htim4, TIM_CHANNEL_3);//停止1000ms
  vTaskDelay(pdMS_TO_TICKS(APP_UNLOCK_MS));
  Servo_Reverse(&htim4, TIM_CHANNEL_3);//反转500ms
  vTaskDelay(pdMS_TO_TICKS(APP_SERVO_PULSE_MS));
  Servo_Stop(&htim4, TIM_CHANNEL_3);//停止
  s_pwd_len = 0u;//刷新密码长度为0
  s_fail_count = 0u;//连续输入密码次数为0
  if (s_mode == APP_MODE_NORMAL)//如果现在的界面是normal界面也就是一开始的显示键盘值和字母的。输入密码之后将屏幕刷新为之前的样子
  {
    App_ShowKeypad(NULL);
  }
}

static void App_Deny(const char *reason, uint16_t voice_idx)
{
  (void)reason;
  App_PlayVoice(voice_idx);//输出对应的音效
  s_pwd_len = 0u;//把密码长度刷新
  if (s_mode == APP_MODE_NORMAL)//刷新屏幕
  {
    App_ShowKeypad(NULL);
  }
}

static void App_SendEvent(AppEvtType type, uint32_t data)
{
  AppEvent evt = { type, data };
  (void)xQueueSend(s_evt_q, &evt, 0);
}

//锁定30s，30s后自动显示默认按键界面
static uint8_t App_IsLocked(void)
{
  if (s_lock_until == 0u)
  {
    return 0u;
  }

  TickType_t now = xTaskGetTickCount();
  if (now < s_lock_until)
  {
    if (s_mode == APP_MODE_NORMAL)
    {
      uint32_t remain_ms = (uint32_t)(s_lock_until - now);
      uint8_t remain_sec = (uint8_t)((remain_ms + 999u) / 1000u);
      if (remain_sec != s_lock_last_sec)
      {
        char line4[17];
        (void)snprintf(line4, sizeof(line4), "LOCK %02uS", remain_sec);
        App_ShowKeypad(line4);
        s_lock_last_sec = remain_sec;
      }
    }
    return 1u;
  }

  s_lock_until = 0u;
  s_lock_last_sec = 0u;
  if (s_mode == APP_MODE_NORMAL)
  {
    App_ShowKeypad(NULL);
  }
  return 0u;
}

static void App_TriggerLockout(void)
{//获取当前的时刻+30s
  s_lock_until = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
  s_fail_count = 0u;
  s_lock_last_sec = 0xFFu;
  App_PlayVoice(12u);
}

static void App_HandlePwdFail(void)
{
  s_fail_count++;
  App_Deny("PWD ERR", 2u);
  if (s_fail_count >= 3u)
  {
    App_TriggerLockout();
  }
}

void FINGER_PromptSecondPress(void)
{
  AppEvent evt = { APP_EVT_FINGER, (APP_FINGER_RESULT_PROMPT << 16) };
  (void)xQueueSend(s_evt_q, &evt, pdMS_TO_TICKS(50));
}

static uint8_t App_KeyToDigit(uint8_t key, uint8_t *digit)
{
  switch (key)
  {
    case 1u: *digit = 1u; return 1u;
    case 2u: *digit = 2u; return 1u;
    case 3u: *digit = 3u; return 1u;
    case 5u: *digit = 4u; return 1u;
    case 6u: *digit = 5u; return 1u;
    case 7u: *digit = 6u; return 1u;
    case 9u: *digit = 7u; return 1u;
    case 10u: *digit = 8u; return 1u;
    case 11u: *digit = 9u; return 1u;
    case KEY_ZERO: *digit = 0u; return 1u;
    default:
      break;
  }
  return 0u;
}

static void App_HandleCard(uint32_t uid)
{
  if (App_IsLocked() != 0u)
  {
    return;
  }

  if (s_mode == APP_MODE_ADMIN_CARD_ADD)
  {
    if (s_card_sel_id < 1u || s_card_sel_id > APP_MAX_CARDS)
    {
      App_PlayVoice(6u);
      s_mode = APP_MODE_ADMIN_CARD_MENU;
      App_ShowCardMenu();
      return;
    }
    if (App_AddCardToSlot((uint8_t)(s_card_sel_id - 1u), uid) != 0u)
    {
      App_PlayVoice(10u);
    }
    else
    {
      App_PlayVoice(6u);
    }
    s_mode = APP_MODE_ADMIN_CARD_MENU;
    App_ShowCardMenu();
    return;
  }

  if (s_mode != APP_MODE_NORMAL)
  {
    return;
  }

  if (App_FindCard(uid) != 0u)
  {
    App_UnlockOk();
  }
  else
  {
    App_Deny("CARD ERR", 2u);
  }
}

static void App_HandleKey(uint8_t key)
{
  uint8_t digit = 0u;

  App_PlayVoice(13u);

  if (App_IsLocked() != 0u)
  {
    return;
  }

  if (s_mode == APP_MODE_ADMIN_FINGER_WAIT)
  {
    return;
  }

  if (s_mode == APP_MODE_ADMIN_PWD)
  {
    if (App_KeyToDigit(key, &digit) != 0u)
    {
      if (s_pwd_len < APP_PWD_LEN)
      {
        s_pwd_buf[s_pwd_len++] = digit;
        App_ShowAdminPwd(s_pwd_len);
        if (s_pwd_len == APP_PWD_LEN)
        {
          if (memcmp(s_pwd_buf, s_password, APP_PWD_LEN) == 0)
          {
            s_mode = APP_MODE_ADMIN_MENU;
            s_menu_sel = 0u;
            App_ShowAdminMenu();
          }
          else
          {
            App_Deny("PWD ERR", 2u);
            s_mode = APP_MODE_NORMAL;
            App_ShowKeypad(NULL);
          }
          s_pwd_len = 0u;
        }
      }
      return;
    }

    if (key == KEY_C)
    {
      if (s_pwd_len > 0u)
      {
        s_pwd_len--;
      }
      App_ShowAdminPwd(s_pwd_len);
      return;
    }

    if (key == KEY_R)
    {
      s_pwd_len = 0u;
      s_mode = APP_MODE_NORMAL;
      App_ShowKeypad(NULL);
      return;
    }
  }

  if (s_mode == APP_MODE_ADMIN_MENU)
  {
    if (key == KEY_U)
    {
      s_menu_sel = (s_menu_sel == 0u) ? 3u : (uint8_t)(s_menu_sel - 1u);
      App_ShowAdminMenu();
      return;
    }
    if (key == KEY_D)
    {
      s_menu_sel = (uint8_t)((s_menu_sel + 1u) % 4u);
      App_ShowAdminMenu();
      return;
    }
    if (key == KEY_OK)
    {
      if (s_menu_sel == 0u)
      {
        s_mode = APP_MODE_ADMIN_CARD_MENU;
        s_card_sel_id = 1u;
        App_ShowCardMenu();
      }
      else if (s_menu_sel == 1u)
      {
        s_mode = APP_MODE_ADMIN_FINGER_MENU;
        s_finger_sel_id = 1u;
        s_finger_index_valid = 0u;
        FingerCmd cmd = { FINGER_CMD_READ_INDEX, 0u };
        (void)xQueueSend(s_finger_q, &cmd, 0);
        App_ShowFingerMenu();
      }
      else if (s_menu_sel == 2u)
      {
        s_mode = APP_MODE_PWD_SET;
        s_pwd_len = 0u;
        App_ShowPwdSet(s_pwd_len);
      }
      else
      {
        s_mode = APP_MODE_ADMIN_VOLUME;
        App_ShowVolumeMenu();
      }
      return;
    }
    if (key == KEY_R)
    {
      s_mode = APP_MODE_NORMAL;
      App_ShowKeypad(NULL);
      return;
    }
  }

  if (s_mode == APP_MODE_ADMIN_CARD_MENU)
  {
    if (key == KEY_U)
    {
      s_card_sel_id = (s_card_sel_id <= 1u) ? APP_MAX_CARDS : (uint16_t)(s_card_sel_id - 1u);
      App_ShowCardMenu();
      return;
    }
    if (key == KEY_D)
    {
      s_card_sel_id = (s_card_sel_id >= APP_MAX_CARDS) ? 1u : (uint16_t)(s_card_sel_id + 1u);
      App_ShowCardMenu();
      return;
    }
    if (key == KEY_OK)
    {
      if (App_RemoveCardSlot((uint8_t)(s_card_sel_id - 1u)) != 0u)
      {
        App_PlayVoice(9u);
      }
      else
      {
        App_PlayVoice(6u);
      }
      App_ShowCardMenu();
      return;
    }
    if (key == KEY_A)
    {
      s_mode = APP_MODE_ADMIN_CARD_ADD;
      return;
    }
    if (key == KEY_R)
    {
      s_mode = APP_MODE_ADMIN_MENU;
      App_ShowAdminMenu();
      return;
    }
  }

  if (s_mode == APP_MODE_ADMIN_CARD_ADD)
  {
    if (key == KEY_R)
    {
      s_mode = APP_MODE_ADMIN_CARD_MENU;
      App_ShowCardMenu();
      return;
    }
  }

  if (s_mode == APP_MODE_ADMIN_FINGER_MENU)
  {
    if (key == KEY_U)
    {
      s_finger_sel_id = (s_finger_sel_id <= 1u) ? APP_FINGER_DB_SIZE : (uint16_t)(s_finger_sel_id - 1u);
      App_ShowFingerMenu();
      return;
    }
    if (key == KEY_D)
    {
      s_finger_sel_id = (s_finger_sel_id >= APP_FINGER_DB_SIZE) ? 1u : (uint16_t)(s_finger_sel_id + 1u);
      App_ShowFingerMenu();
      return;
    }
    if (key == KEY_A)
    {
      FingerCmd cmd = { FINGER_CMD_ENROLL, s_finger_sel_id };
      HAL_NVIC_DisableIRQ(FINGER_IRQ_EXTI_IRQn);
      (void)xQueueSend(s_finger_q, &cmd, 0);
      s_mode = APP_MODE_ADMIN_FINGER_WAIT;
      App_PlayVoice(3u);
      return;
    }
    if (key == KEY_OK)
    {
      FingerCmd cmd = { FINGER_CMD_DELETE, s_finger_sel_id };
      (void)xQueueSend(s_finger_q, &cmd, 0);
      s_mode = APP_MODE_ADMIN_FINGER_WAIT;
      return;
    }
    if (key == KEY_R)
    {
      s_mode = APP_MODE_ADMIN_MENU;
      App_ShowAdminMenu();
      return;
    }
  }

  if (s_mode == APP_MODE_PWD_SET)
  {
    if (App_KeyToDigit(key, &digit) != 0u)
    {
      if (s_pwd_len < APP_PWD_LEN)
      {
        s_pwd_buf[s_pwd_len++] = digit;
        App_ShowPwdSet(s_pwd_len);
      }
      return;
    }
    if (key == KEY_C)
    {
      if (s_pwd_len > 0u)
      {
        s_pwd_len--;
      }
      App_ShowPwdSet(s_pwd_len);
      return;
    }
    if (key == KEY_OK)
    {
      if (s_pwd_len == APP_PWD_LEN)
      {
        memcpy(s_password, s_pwd_buf, APP_PWD_LEN);
        App_SaveConfig();
        App_PlayVoice(11u);
        s_mode = APP_MODE_ADMIN_MENU;
        App_ShowAdminMenu();
      }
      else
      {
        App_Deny("PWD LEN", 6u);
        App_ShowPwdSet(s_pwd_len);
      }
      s_pwd_len = 0u;
      return;
    }
    if (key == KEY_R)
    {
      s_pwd_len = 0u;
      s_mode = APP_MODE_ADMIN_MENU;
      App_ShowAdminMenu();
      return;
    }
    return;
  }

  if (s_mode == APP_MODE_ADMIN_VOLUME)
  {
    if (key == KEY_U)
    {
      if (s_volume_level + 1u >= APP_VOLUME_LEVELS)
      {
        s_volume_level = 0u;
      }
      else
      {
        s_volume_level++;
      }
      JQ8900_SetVolume(&s_voice, s_volume_table[s_volume_level]);
      App_SaveConfig();
      App_ShowVolumeMenu();
      return;
    }
    if (key == KEY_D)
    {
      if (s_volume_level == 0u)
      {
        s_volume_level = (uint8_t)(APP_VOLUME_LEVELS - 1u);
      }
      else
      {
        s_volume_level--;
      }
      JQ8900_SetVolume(&s_voice, s_volume_table[s_volume_level]);
      App_SaveConfig();
      App_ShowVolumeMenu();
      return;
    }
    if (key == KEY_OK || key == KEY_R)
    {
      s_mode = APP_MODE_ADMIN_MENU;
      App_ShowAdminMenu();
      return;
    }
  }

  if (s_mode == APP_MODE_NORMAL)
  {
    if (key == KEY_A)
    {
      s_pwd_len = 0u;
      s_mode = APP_MODE_ADMIN_PWD;
      App_ShowAdminPwd(s_pwd_len);
      return;
    }

    if (App_KeyToDigit(key, &digit) != 0u)
    {
      if (s_pwd_len < APP_PWD_LEN)
      {
        s_pwd_buf[s_pwd_len++] = digit;
        App_ShowPwdEntry(s_pwd_len);
        if (s_pwd_len == APP_PWD_LEN)
        {
          if (memcmp(s_pwd_buf, s_password, APP_PWD_LEN) == 0)
          {
            App_UnlockOk();
          }
          else
          {
            App_HandlePwdFail();
          }
          s_pwd_len = 0u;
        }
      }
      return;
    }

    if (key == KEY_C)
    {
      if (s_pwd_len > 0u)
      {
        s_pwd_len--;
        App_ShowPwdEntry(s_pwd_len);
      }
      else
      {
        App_ShowKeypad(NULL);
      }
      return;
    }
  }
}

static void App_InputTask(void *argument)
{
  (void)argument;
  uint8_t last_key = 0u;
  uint32_t last_uid = 0u;
  TickType_t last_uid_tick = 0u;
  uint8_t last_shake = 0u;

  KEY_Init();
  RC522_Init();
  RC522_Rese();
  RC522_Config_Type('A');
  BT_Init();

  for (;;)
  {
    uint8_t key = KEY_Scan();
    if (key != 0u && key != last_key)
    {
      App_SendEvent(APP_EVT_KEY, key);
      last_key = key;
    }
    else if (key == 0u)
    {
      last_key = 0u;
    }

    uint32_t uid = 0u;
    if (App_ReadCard(&uid) != 0u)
    {
      TickType_t now = xTaskGetTickCount();
      if (uid != last_uid || (now - last_uid_tick) > pdMS_TO_TICKS(1500))
      {
        App_SendEvent(APP_EVT_CARD, uid);
        last_uid = uid;
        last_uid_tick = now;
      }
    }

    uint8_t shake = SHAKE_IsActive();
    if (shake != 0u && last_shake == 0u)
    {
      App_SendEvent(APP_EVT_SHAKE, 0u);
    }
    last_shake = shake;

    while (BT_Available() != 0u)
    {
      int16_t ch = BT_Read();
      if (ch >= 0)
      {
        App_SendEvent(APP_EVT_BT, (uint32_t)(uint8_t)ch);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void App_FingerTask(void *argument)
{
  (void)argument;
  FingerCmd cmd;

  FINGER_Init(&s_finger, &huart3);
  FINGER_SetTimeout(&s_finger, 1000);

  for (;;)
  {
    (void)xQueueReceive(s_finger_q, &cmd, portMAX_DELAY);

    if (cmd.cmd == FINGER_CMD_IDENTIFY)
    {
      uint16_t page_id = 0;
      uint16_t score = 0;
      FINGER_Result ret = FINGER_Identify(&s_finger, 0, APP_FINGER_DB_SIZE, &page_id, &score, 5000);
      uint32_t ok = (ret == FINGER_OK) ? 1u : 0u;
      App_SendEvent(APP_EVT_FINGER, (APP_FINGER_RESULT_IDENTIFY << 16) | ok);
      App_FingerIrqRelease();
    }
    else if (cmd.cmd == FINGER_CMD_ENROLL)
    {
      FINGER_Result ret = FINGER_Enroll(&s_finger, cmd.param, 15000);
      uint32_t ok = (ret == FINGER_OK) ? 1u : 0u;
      App_SendEvent(APP_EVT_FINGER, (APP_FINGER_RESULT_ENROLL << 16) | ok);
      App_FingerIrqRelease();
    }
    else if (cmd.cmd == FINGER_CMD_DELETE)
    {
      FINGER_Result ret = FINGER_Delete(&s_finger, cmd.param, 1);
      uint32_t ok = (ret == FINGER_OK) ? 1u : 0u;
      App_SendEvent(APP_EVT_FINGER, (APP_FINGER_RESULT_DELETE << 16) | ok);
    }
    else if (cmd.cmd == FINGER_CMD_READ_INDEX)
    {
      uint8_t table[32];
      uint64_t mask = 0u;
      FINGER_Result ret = FINGER_ReadIndexTable(&s_finger, 0u, table, sizeof(table));
      if (ret == FINGER_OK)
      {
        uint16_t max_bits = (APP_FINGER_DB_SIZE < 64u) ? APP_FINGER_DB_SIZE : 64u;
        for (uint16_t i = 0; i < max_bits; i++)
        {
          uint8_t bit = (uint8_t)(table[i / 8u] >> (i % 8u)) & 0x1u;
          if (bit != 0u)
          {
            mask |= (uint64_t)1u << i;
          }
        }
        s_finger_index = mask;
        s_finger_index_valid = 1u;
      }
      else
      {
        s_finger_index_valid = 0u;
      }
      App_SendEvent(APP_EVT_FINGER, (APP_FINGER_RESULT_INDEX << 16));
    }
  }
}

static void App_UiTask(void *argument)
{
  (void)argument;
  AppEvent evt;

  OLED_Init();
  OLED_Clear();
  for (uint8_t i = 0; i < 4u; i++)
  {
    s_lines[i][0] = '\0';
  }
  App_ShowKeypad(NULL);
  s_mode = APP_MODE_NORMAL;
  s_menu_sel = 0u;
  s_card_sel_id = 1u;
  s_finger_sel_id = 1u;
  s_pwd_len = 0u;
  s_fail_count = 0u;
  s_lock_until = 0u;
  s_lock_last_sec = 0u;
  s_finger_index = 0u;
  s_finger_index_valid = 0u;

  JQ8900_Init(&s_voice, &huart2);
  App_LoadConfig();
  JQ8900_SetVolume(&s_voice, s_volume_table[s_volume_level]);

  for (;;)
  {
    if (xQueueReceive(s_evt_q, &evt, pdMS_TO_TICKS(200)) != pdTRUE)
    {
      if (s_lock_until != 0u)
      {
        (void)App_IsLocked();
      }
      continue;
    }

    if (evt.type == APP_EVT_SHAKE)
    {
      if (App_IsLocked() == 0u)
      {
        App_Deny("SHAKE", 6u);
      }
      continue;
    }

    if (evt.type == APP_EVT_BT)
    {
      if ((uint8_t)evt.data == 'O')
      {
        if (App_IsLocked() == 0u)
        {
          App_UnlockOk();
        }
      }
      continue;
    }

    if (evt.type == APP_EVT_CARD)
    {
      App_HandleCard(evt.data);
      continue;
    }

    if (evt.type == APP_EVT_KEY)
    {
      App_HandleKey((uint8_t)evt.data);
      continue;
    }

    if (evt.type == APP_EVT_FINGER)
    {
      uint16_t kind = (uint16_t)(evt.data >> 16);
      uint16_t ok = (uint16_t)(evt.data & 0xFFFFu);
      if (kind == APP_FINGER_RESULT_IDENTIFY)
      {
        if (App_IsLocked() == 0u && s_mode == APP_MODE_NORMAL)
        {
          if (ok != 0u)
          {
            App_UnlockOk();
          }
          else
          {
            App_Deny("FINGER ERR", 2u);
          }
        }
      }
      else if (kind == APP_FINGER_RESULT_ENROLL)
      {
        if (ok != 0u)
        {
          App_PlayVoice(7u);
        }
        else
        {
          App_PlayVoice(6u);
        }
        s_mode = APP_MODE_ADMIN_FINGER_MENU;
        {
          FingerCmd cmd = { FINGER_CMD_READ_INDEX, 0u };
          (void)xQueueSend(s_finger_q, &cmd, 0);
        }
        App_ShowFingerMenu();
      }
      else if (kind == APP_FINGER_RESULT_PROMPT)
      {
        App_PlayVoice(4u);
      }
      else if (kind == APP_FINGER_RESULT_DELETE)
      {
        if (ok != 0u)
        {
          App_PlayVoice(8u);
        }
        else
        {
          App_PlayVoice(6u);
        }
        s_mode = APP_MODE_ADMIN_FINGER_MENU;
        {
          FingerCmd cmd = { FINGER_CMD_READ_INDEX, 0u };
          (void)xQueueSend(s_finger_q, &cmd, 0);
        }
        App_ShowFingerMenu();
      }
      else if (kind == APP_FINGER_RESULT_INDEX)
      {
        if (s_mode == APP_MODE_ADMIN_FINGER_MENU)
        {
          App_ShowFingerMenu();
        }
      }
      continue;
    }
  }
}

//不同优先级抢占式调度
//同级优先级时间片轮转
void App_CreateTasks(void)
{
  s_evt_q = xQueueCreate(12, sizeof(AppEvent));//能存放12个appevent类型的事件
  s_finger_q = xQueueCreate(4, sizeof(FingerCmd));//4个fingercmd类型事件
  //任务函数，任务名称，任务栈大小（单位是字，1字=4字节），任务参数，任务优先级
  xTaskCreate(App_InputTask, "input", 256, NULL, tskIDLE_PRIORITY + 2U, NULL);
  xTaskCreate(App_FingerTask, "finger", 256, NULL, tskIDLE_PRIORITY + 2U, NULL);
  xTaskCreate(App_UiTask, "ui", 256, NULL, tskIDLE_PRIORITY + 1U, NULL);
}

void FINGER_DelayMs(uint32_t ms)
{
  vTaskDelay(pdMS_TO_TICKS(ms));
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == FINGER_IRQ_Pin)
  {
    BaseType_t higher = pdFALSE;
    FingerCmd cmd = { FINGER_CMD_IDENTIFY, 0u };
    HAL_NVIC_DisableIRQ(FINGER_IRQ_EXTI_IRQn);
    (void)xQueueSendFromISR(s_finger_q, &cmd, &higher);
    portYIELD_FROM_ISR(higher);
  }
}
