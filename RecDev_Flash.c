/******************************************************************************

              记录专用底层驱动程序-在Flash中的实现
驱动在写时，保证续还一条记录的空间为全1，以此保证回环后能查找到起始位置
每个存储区以记录长度对齐，若取模有余数，则丢弃!
注：记录地址起始与结束，必须为Flash一页的起始与结束！！
******************************************************************************/
#include "RecDev.h"

#include  "Flash.h"  //依赖此接口以操作Flash
#include <string.h>

//-------------------------写flash函数--------------------------------
void _WrFlash(unsigned long Adr,   //Flash地址
              const void *pVoid,    //要写入的数据
              unsigned long Len)   //写数据长度
{
  Flash_Unlock();//解锁
  Flash_Write(Adr, pVoid, Len);
  Flash_Lock();//上锁
  //WDT_Week();//喂狗一次
}

//----------------------格式化一页函数--------------------------------
void _FormatFlash(unsigned long Adr)
{
  Flash_Unlock();//解锁
  Flash_ErasePage(Adr);//擦除页
  Flash_Lock();//上锁
  //WDT_Week();//喂狗一次
}

//--------------------------判断读缓冲记录是否有效-----------------------
signed char _RdBufIsValid(const struct _RecDev *pDev)
{
  for(unsigned char i = 0; i < pDev->Desc.FrameSize; i++)
    if(pDev->RdBuf[i] != 0xff) return 1; //有效
  return 0; //全1无效
}

//----------------------------记录虚拟设备初始化函数-----------------------
void RecDev_Init(struct _RecDev *pDev, const struct _RecDevDesc *pDesc)
{
  //初始化结构(为防止pDesc地址相同冲掉，不初始化所有)
  memcpy(&pDev->Desc, pDesc, sizeof(struct _RecDevDesc));
  
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t Start = pDev->Desc.Start; 
  RecDevAdr_t End = pDev->Desc.End;   
  //先查找到每个扇区是否有第一条记录
  for(; Start < End; Start += FLASH_PAGE_SIZE){
    Flash_Read(Start, pDev->RdBuf, FrameSize);
    if(!_RdBufIsValid(pDev)) break; //首条有数据了
  }
  if(Start >= End) Start-= FLASH_PAGE_SIZE;//在最后一页了
  else{//前几页需检查回环:下一页有数据时，即认为回环了
    Flash_Read(Start + FLASH_PAGE_SIZE, pDev->RdBuf, FrameSize);
    pDev->Looped = 1;//置回环标志
  }
   
  //采用中值法查找当前页最后一条写记录位置
  unsigned short FindMin = 0;  
  unsigned short RecCount = FLASH_PAGE_SIZE / FrameSize;
  unsigned short FindMax = RecCount;
  unsigned short FindMid = RecCount / 2;
  while((FindMax - 1) > FindMin){
    Flash_Read((Start + (FindMid * FrameSize)), pDev->RdBuf, FrameSize);
    if(!_RdBufIsValid(pDev))//没使用过时,在中间值前面
      FindMax = FindMid;
    else//使用过时,在中间值后面
      FindMin = FindMid;
    FindMid = (FindMax + FindMin) / 2;
  }
  pDev->NextWrPos = Start + (FindMax * FrameSize);//得到写位置
}

//-----------------------格式化记录区函数--------------------------------
void RecDev_Format(struct _RecDev *pDev)
{
  RecDevAdr_t End = pDev->Desc.End;
  RecDevAdr_t Start = pDev->Desc.Start;
  for(; Start < End; Start += FLASH_PAGE_SIZE){
    unsigned short WrSize = End - Start;
    if(WrSize > FLASH_PAGE_SIZE) WrSize = FLASH_PAGE_SIZE;
    _FormatFlash(Start);

  }
  RecDev_Init(pDev, &pDev->Desc);//重新初始化
}

//--------------------------得到写入位置函数----------------------------
static RecDevAdr_t _GetWrPos(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  // * 1为准备写入的结束位置, 不够不写了，直接查回环
  RecDevAdr_t WrPos = pDev->NextWrPos; 
  if((WrPos + FrameSize) >= pDev->Desc.End){//下次或本次不够，回环了
    _FormatFlash(pDev->Desc.Start); //写入前，需提前格式化以留判断起始的空间
    if((WrPos + FrameSize) > pDev->Desc.End){//最后记录不足一条了,丢弃
      pDev->Looped = 1;//写到最后回环了
      WrPos = pDev->Desc.Start;
    }
    //else 继续写最后一条记录，写完再处理
  }
  return WrPos;
}

//----------------------------记录虚拟设备写函数-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  //提前得到下次写入结束位置
  RecDevAdr_t NextEnd = pDev->NextWrPos + FrameSize * 2;  
  //本页最后一条记录时，提前格式化下一页，以防止期间关机
  if((NextEnd % FLASH_PAGE_SIZE) >= FLASH_PAGE_SIZE){
    if(NextEnd >= pDev->Desc.End){//写到最后回环了
      NextEnd = pDev->Desc.Start;
      pDev->Looped = 1;
    }
    else //重新修正到下页起始
      NextEnd = pDev->NextWrPos + FrameSize + FLASH_PAGE_SIZE % FrameSize; 
    _FormatFlash(NextEnd);
  }
  else NextEnd -= FrameSize;//本次结束位置
  //写入本次记录
  _WrFlash(pDev->NextWrPos, pDev->WrBuf, FrameSize);
  pDev->NextWrPos = NextEnd;
}

//------------------------得到记录页数-------------------------------
static unsigned char _GetPageCount(const struct _RecDev *pDev)
{
  return (pDev->Desc.End - pDev->Desc.Start) / FLASH_PAGE_SIZE;
}

//------------------------得到记录容量-------------------------------
unsigned short RecDev_GetCapability(const struct _RecDev *pDev)
{
  //每页可记录总数
  unsigned short PageRecCount = FLASH_PAGE_SIZE / pDev->Desc.FrameSize;
  return PageRecCount * _GetPageCount(pDev);
}

//------------------------得到已保存记录总数---------------------------
unsigned short RecDev_GetCount(const struct _RecDev *pDev)
{
  unsigned char FullPageCount; //已写满页总数
  if(pDev->Looped){//回环了
    FullPageCount = _GetPageCount(pDev) - 1;//有一页未写满
  }
  else{//没回环
    FullPageCount = (pDev->NextWrPos - pDev->Desc.Start) / 
                     FLASH_PAGE_SIZE;
  }
  //每页可记录总数
  unsigned char FrameSize = pDev->Desc.FrameSize;   
  return FullPageCount * (FLASH_PAGE_SIZE / FrameSize) + //已写入整页的
         (pDev->NextWrPos % FLASH_PAGE_SIZE) / FrameSize;//未写满的
}

//-----------------------记录虚拟设备读函数-----------------------
//将指定数据读取到缓冲区中,到少有一个历史记录才能调用此函数读数
//返回缓冲区地址：NULL表示记录超限或错误
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId)
{
  if(RecId >= RecDev_GetCount(pDev)) return NULL; //读位置超限
  
  unsigned char FrameSize = pDev->Desc.FrameSize; 
  unsigned short CurPageRecCount = //当前正在写入页的记录总数
                 (pDev->NextWrPos & FLASH_PAGE_SIZE) / FrameSize;
  if(RecId < CurPageRecCount){//在写入页了
    Flash_Read(pDev->NextWrPos - RecId * FrameSize, 
               pDev->RdBuf, FrameSize);
    return pDev->RdBuf;    
  }
  
  //跨页了,先计算页位置
  RecId -= CurPageRecCount;//整页对齐了
  unsigned short PageRecCount = FLASH_PAGE_SIZE / FrameSize;//每页可记录总数
  unsigned char WrPagePos = //写入位置页位，从此开始往前算
                (pDev->NextWrPos - pDev->Desc.Start) / FLASH_PAGE_SIZE;
  
  //需要跨的页数转换到具体页数
  unsigned char PagePos = RecId / PageRecCount;//需要跨的页数  
  if(WrPagePos >= PagePos){//在pDev->Desc.Start后页，没有回环
    PagePos = WrPagePos - PagePos;
  }
  else{//回环到最后了
    PagePos = PagePos - WrPagePos;//需回环的页数
    PagePos = _GetPageCount(pDev) - PagePos; //反向回环位置
  }
  //再计算记录位置
  RecDevAdr_t RdPos = pDev->Desc.Start + PagePos * FLASH_PAGE_SIZE;//至某页了
  RdPos += PageRecCount - (RecId % PageRecCount);//具体位置,倒着算
  
  Flash_Read(RdPos, pDev->RdBuf, pDev->Desc.FrameSize);
  return pDev->RdBuf;
}




