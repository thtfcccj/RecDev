/******************************************************************************

              记录专用底层驱动程序-在单Flash个Sector中的实现
  单Sector采用回写最新记录方式进行，在此Sector满时，将存储的最新数据移至临时RAM中，
将Flash区擦除后再回写到起始，接着写接下来的记录。

  单Sector不存在回环问题，但存在回写多少个字节，以及回写字节对齐问题
  回写字节建议为容量的一半。
******************************************************************************/

#include "RecDev.h"

#include "RecDev_FlashS.h"
#include  "Flash.h"  //依赖此接口以操作Flash
#include <string.h>

//-------------------------Flash实现与Eeprom实现的异同-------------------------
//相同处：
//   均是空间连续存储，故初始化与读取记录操作略同(除调用读写函数不同外，去掉回环部分)
//不同点:
//   因Flash只有一个扇区，故不存在回环问题,，写数据函数等也不同

//-------------------------写flash函数--------------------------------
void _WrFlash(unsigned long Adr,   //Flash地址
              const void *pVoid,    //要写入的数据
              unsigned long Len)   //写数据长度
{
  Flash_Write(Adr, pVoid, Len);
}

//----------------------格式化一页函数--------------------------------
void _FormatFlash(unsigned long Adr)
{
  Flash_ErasePage(Adr);//擦除页
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
    pDev->NextWrPos = Start;
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
  
  //初始化写头
  if(FindMax == RecCount)//全满时
    pDev->NextWrPos = pDev->Desc.End;//至结束，再次写时将处理
  else
    pDev->NextWrPos = (FindMax * FrameSize) + Start;//得到最后写位置
}

//-----------------------格式化记录区函数--------------------------------
void RecDev_Format(struct _RecDev *pDev)
{
  if(RecDev_GetCount(pDev) == 0) return; //防止重复初始化
 _FormatFlash(pDev->Desc.Start);
  RecDev_Init(pDev, &pDev->Desc);//重新初始化
}

//----------------------------记录虚拟设备写函数-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t CurWrPos = pDev->NextWrPos;
  if((CurWrPos + FrameSize) > pDev->Desc.End){ //记录满了，需处理
    unsigned short BufSize;
    const unsigned char *pBufData = RecDev_pcbRecToBuf(pDev, &BufSize);//缓存入缓冲
    _FormatFlash(pDev->Desc.Start); //擦除整片区域(祈祷此期间不要断电)
    _WrFlash(pDev->Desc.Start, //回写(祈祷此期间不要断电)
             pBufData, BufSize);
    CurWrPos = pDev->Desc.Start + BufSize;
  }
  _WrFlash(CurWrPos, pDev->WrBuf, FrameSize);
  pDev->NextWrPos = CurWrPos + FrameSize; //更新最后写入位置
}

//------------------------得到已保存记录总数---------------------------
unsigned short RecDev_GetCount(const struct _RecDev *pDev)
{
  return (pDev->NextWrPos - pDev->Desc.Start) / pDev->Desc.FrameSize;
}

//-----------------------记录虚拟设备读函数-----------------------
//将指定数据读取到缓冲区中,到少有一个历史记录才能调用此函数读数
//返回缓冲区地址：NULL表示记录超限或错误
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId)
{
  if(RecId >= RecDev_GetCount(pDev)) return NULL; //读位置超限
  //准备写入位置前，是第一条
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t RdPos = (pDev->NextWrPos - FrameSize) - RecId * FrameSize;
  //Flash_Read(RdPos, pDev->RdBuf, FrameSize);
  //return pDev->RdBuf;
  //为加快速度，直接返回flash指针
  return (const unsigned char *)RdPos;
}

//---------------------------清除某个记录函数-------------------------------
//这里仅将该记录时间位全部清除, 以标识被清除
void RecDev_Clr(struct _RecDev *pDev, unsigned short RecId)
{
  const unsigned char *pRec = RecDev_Rd(pDev, RecId); //这里返回的是准备写的flash指针
  if(pRec == NULL) return; //未找到
  memset(pDev->WrBuf, 0, 4);
  _WrFlash((unsigned long)pRec - pDev->Desc.Start, pDev->WrBuf, 4);
}

