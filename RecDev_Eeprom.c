/******************************************************************************

              记录专用底层驱动程序-在EEPROM中的实现
驱动在写时，保证续还一条记录的空间为全1，以此保证回环后能查找到起始位置
存储区以记录长度为标志对齐
******************************************************************************/

#include "RecDev.h"

#include  "Eeprom.h"  //依赖此接口以操作EEPROM
#include <string.h>

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
  Eeprom_Rd(Start, pDev->RdBuf, FrameSize);//先读首条记录到读缓冲区
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
    Eeprom_Rd((Start + (FindMid * FrameSize)), pDev->RdBuf, FrameSize);
    if(!_RdBufIsValid(pDev))//没使用过时,在中间值前面
      FindMax = FindMid;
    else//使用过时,在中间值后面
      FindMin = FindMid;
    FindMid = (FindMax + FindMin) / 2;
  }
  //得到是否回环：读取页最后一个记录,若有数据，则回环了
  RecDevAdr_t RdPos = pDev->Desc.End - ((pDev->Desc.End - Start) % FrameSize);
  RdPos -= FrameSize;//页最后记录位置
  Eeprom_Rd(RdPos, pDev->RdBuf, FrameSize);
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
  memset(pDev->WrBuf, 0xff, REC_DEV_BUF_SIZE);
  for(; Start < End; Start += REC_DEV_BUF_SIZE){
    unsigned char WrSize = End - Start;
    if(WrSize > REC_DEV_BUF_SIZE) WrSize = REC_DEV_BUF_SIZE;
    Eeprom_Wr(Start, pDev->WrBuf, WrSize);
  }
  RecDev_Init(pDev, &pDev->Desc);//重新初始化
}

//-----------------------得到写入位置函数-------------------------
static RecDevAdr_t _GetWrPos(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  // * 2为准备写入的结束位置
  RecDevAdr_t WrPos = pDev->LastWrPos + FrameSize * 2; 
  if(WrPos > pDev->Desc.End){//回环了
    pDev->Looped = 1;
    WrPos = pDev->Desc.Start;
  }
  else WrPos -= FrameSize; //本次写位置了
  return WrPos;
}

//----------------------------记录虚拟设备写函数-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t WrPos = _GetWrPos(pDev);
  Eeprom_Wr(WrPos, pDev->WrBuf, FrameSize);
  //写入成功
  pDev->LastWrPos = WrPos;
  //若在回环状态，则提前需将下个记录(即最后一次写位置)删除，才能在重启后找到回环位置
  if(pDev->Looped){
    memset(pDev->WrBuf, 0xff, FrameSize);
    Eeprom_Wr( _GetWrPos(pDev), pDev->WrBuf, FrameSize);
  }
}

//------------------------得到已保存记录总数---------------------------
unsigned short RecDev_GetCount(struct _RecDev *pDev)
{
  if(pDev->Looped){//回环了(-1是因为最后一个数被提前清掉)
    return (pDev->Desc.End - pDev->Desc.Start) / pDev->Desc.FrameSize - 1;
  }
  //没回环
  return (pDev->LastWrPos - pDev->Desc.Start) / pDev->Desc.FrameSize;
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
  Eeprom_Rd(RdPos, pDev->RdBuf, pDev->Desc.FrameSize);
  return pDev->RdBuf;
}



