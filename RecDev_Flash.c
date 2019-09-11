/******************************************************************************

              记录专用底层驱动程序-在Flash中的实现
驱动在写时，保证续还一条记录的空间为全1，以此保证回环后能查找到起始位置
存储区以记录长度为标志对齐，每页(FLASH_PAGE_SIZE)数据对齐
注：记录地址起始与结束，必须为Flash一页的起始与结束！！
******************************************************************************/

//-------------------------Flash实现与Eeprom实现的异同-------------------------
//相同处：
//   均是空间连续存储，故初始化与读取记录操作相同(仅调用读写函数不同)
//不同点:
//因Flash只能一个扇区擦除，故格式化不同
//flash存在一条记录跨越两个扇区问题，且清下一条记录变成了格式化下个扇区，故写有很大不同

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
  Flash_Read(Start, pDev->RdBuf, FrameSize);//先读首条记录到读缓冲区
  if(!_RdBufIsValid(pDev)){//首条数据无效，认为刚初始化从来没用过
    pDev->LastWrPos = Start;
    return;
  }
  //采用中值法查找记录最后一条写记录位置
  unsigned short FindMin = 0;  
  unsigned short RecCount = (pDev->Desc.End - Start) / FrameSize;
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
  //得到是否回环：读取页最后一个记录,若有数据，则回环了
  RecDevAdr_t RdPos = pDev->Desc.End - ((pDev->Desc.End - Start) % FrameSize);
  RdPos -= FrameSize;//页最后记录位置
  Flash_Read(RdPos, pDev->RdBuf, FrameSize);
  if(_RdBufIsValid(pDev)){//有数时
    pDev->Looped = 1;//置回环标志
  }
  
  //初始化写头
  if(FindMax == RecCount)//全满异常:在清最后一条记录时失败，暂从前开始
    pDev->LastWrPos = Start;//得到写位置
  else
    pDev->LastWrPos = (FindMax * FrameSize) + Start;//得到写位置
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
  // * 2为准备写入的结束位置
  RecDevAdr_t WrPos = pDev->LastWrPos + FrameSize * 2; 
  if(WrPos > pDev->Desc.End){//回环了
    pDev->Looped = 1;
    WrPos = pDev->Desc.Start;
    _FormatFlash(pDev->Desc.Start); //提前格式化
  }
  else WrPos -= FrameSize; //本次写位置了
  return WrPos;
}

//----------------------------记录虚拟设备写函数-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t WrPos = _GetWrPos(pDev);
  //得到本扇区可写入字节数
  unsigned short NextOffWrPos = (WrPos % FLASH_PAGE_SIZE) + FrameSize;//下次相对写入位置
  unsigned char CurWrCount = FrameSize;
  if(NextOffWrPos > FLASH_PAGE_SIZE){ //跨区了
    CurWrCount -= (NextOffWrPos - FLASH_PAGE_SIZE); //本次可写入
    _FormatFlash(WrPos + FrameSize);//提前格式化未整体写入的下一扇区
  };
  _WrFlash(WrPos, pDev->WrBuf, CurWrCount);
  if(CurWrCount < FrameSize){//需写下半部分
    _WrFlash(WrPos + CurWrCount, //下页起始
             &pDev->WrBuf[CurWrCount], //未写入部分
             FrameSize - CurWrCount);  //未写入部分总数
    pDev->LastWrPos = WrPos; //写入成功
  }
  //无下半部分时，若回环，还需检查是否跨到下一页,是则提前格式化以准备
  else if(pDev->Looped){
    pDev->LastWrPos = WrPos; //提前更新下一页以_GetWrPos()
    NextOffWrPos = (_GetWrPos(pDev) % FLASH_PAGE_SIZE) + FrameSize;//下下次相对写入位置
    if(NextOffWrPos > FLASH_PAGE_SIZE){ //跨区了
      _FormatFlash(WrPos + FrameSize);//提前格式化整体写入的下一扇区
    }
  }
  //例：FLASH_PAGE_SIZE = 1024, FrameSize = 5
  //令pDev->LastWrPos= 1015： 本次写入位置1020->提前格式化下一页->4+1写 下:1025 
  //令pDev->LastWrPos= 1104:  本次写入位置1019->5写->提前格式化下一页:1024
}

//------------------------得到已保存记录总数---------------------------
unsigned short RecDev_GetCount(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  if(pDev->Looped){//回环了
    //需减掉正在写入页已被删除的记录
    unsigned short MaxCount = (pDev->Desc.End - pDev->Desc.Start) / FrameSize;
    unsigned short OffPos = pDev->LastWrPos % FLASH_PAGE_SIZE;//已保存的页内相对位置
    MaxCount -= (FLASH_PAGE_SIZE - OffPos) / FrameSize;
    return MaxCount - 1; //预留一个空位防止跨页问题。 
  }
  //没回环
  return (pDev->LastWrPos - pDev->Desc.Start) / FrameSize;

  //例：FLASH_PAGE_SIZE = 1024, FrameSize = 5, 共4页
  //令pDev->LastWrPos= 1025 + 回环 个数为: 最大可能个数：(4095 / 5) = 819;
  //819 - (1024 - (1025 % 1024)) / 5 -1 = 615(此为最小存储个数)
}

//-----------------------记录虚拟设备读函数-----------------------
//将指定数据读取到缓冲区中,到少有一个历史记录才能调用此函数读数
//返回缓冲区地址：NULL表示记录超限或错误
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId)
{
  if(RecId >= RecDev_GetCount(pDev)) return NULL; //读位置超限
  //最后写入位置是第一条
  RecDevAdr_t RdPos = RecId * pDev->Desc.FrameSize + pDev->LastWrPos; //得到读偏移
  if(RdPos > pDev->LastWrPos){ //回环了
    RdPos -= pDev->LastWrPos; //获得需回环数量
    //最后位置留下不能存放的
    unsigned char Leave = (pDev->Desc.End - pDev->Desc.Start) % pDev->Desc.FrameSize;
    RdPos =  pDev->Desc.End - (RdPos + Leave);
  }
  else RdPos = pDev->LastWrPos - RdPos;
  Flash_Read(RdPos, pDev->RdBuf, pDev->Desc.FrameSize);
  return pDev->RdBuf;
}
