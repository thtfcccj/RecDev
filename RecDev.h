/******************************************************************************

                              记录专用底层驱动程序
// 此记录虚拟设备用于在EEPROM或FLASH上,管理固定长度记录的应用
// 每种记录实例化一个RecDev, 最大寻址空间65k
// 任何情况下，每个记录的数据应禁止全1,故推荐记录时间为压缩格式(4Byte)
//此驱动程序只允许单线程操作,且储存需要时间，故多线程等情况时，应使用队列等方式实现写
//当为Flash实现时，至少准备两个扇区
******************************************************************************/
#ifndef __REC_DEV_H
#define __REC_DEV_H
#ifdef SUPPORT_EX_PREINCLUDE//不支持Preinlude時
  #include "Preinclude.h"
#endif

/******************************************************************************
                              相关配置
******************************************************************************/

//定义设备记录缓冲区大小,>系统最占内存记录
#ifndef REC_DEV_BUF_SIZE
  #define REC_DEV_BUF_SIZE    8
#endif

//定义基址范围
#ifndef RecDevAdr_t
  #define RecDevAdr_t  unsigned long
#endif

/******************************************************************************
                              相关结构
******************************************************************************/
//保存结构静态描述
struct _RecDevDesc{
  unsigned char Identifier;      //分配的记录设备唯一标识
  unsigned char FrameSize;       //每帧数据大小
  RecDevAdr_t Start;              //在设备中的起始地址
  RecDevAdr_t End;                //在设备中的结束地址(不含,即下一个起始)
};

struct _RecDev{
  struct _RecDevDesc Desc;           //保存结构描述(直接缓冲以加快程序执行)]
  unsigned char WrBuf[REC_DEV_BUF_SIZE]; //写专用缓冲
  unsigned char RdBuf[REC_DEV_BUF_SIZE]; //读专用缓冲 
  unsigned char Looped;                  //回环标志  
  RecDevAdr_t NextWrPos;                  //下次写入位置(不用最后写入以方便写前判断)  
};

/******************************************************************************
                              相关函数
******************************************************************************/
//----------------------------记录虚拟设备初始化函数-----------------------
void RecDev_Init(struct _RecDev *pDev, const struct _RecDevDesc *pDesc);

//---------------------------格式化记录区函数-------------------------------
//将整个记录区置为0xff,即未使用状态
void RecDev_Format(struct _RecDev *pDev);

//---------------------记录虚拟设备得到写数据缓冲区-----------------------
#define RecDev_pGetWrBuf(pDev)    ((pDev)->WrBuf)

//----------------------------记录虚拟设备写函数-----------------------
void RecDev_Wr(struct _RecDev *pDev);

//------------------------得到已保存记录总数---------------------------
unsigned short RecDev_GetCount(const struct _RecDev *pDev);

//-----------------------记录虚拟设备读函数---------------------------
//将指定数据读取到缓冲区中,到少有一个历史记录才能调用此函数读数
//返回缓冲区地址：NULL表示记录超限或错误
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId);

//---------------------------清除某个记录函数-------------------------------
//这里仅将该记录时间位全部清除, 以标识被清除
void RecDev_Clr(struct _RecDev *pDev, unsigned short RecId);

//---------------------记录虚拟设备得到读缓冲区-----------------------
#define   RecDev_pGetRdBuf(pDev) ((pDev)->RdBuf)

#endif

