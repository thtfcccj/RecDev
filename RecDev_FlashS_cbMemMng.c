/******************************************************************************

              记录专用底层驱动程序回调函数-使用堆空间时的实现
//此模块要求MemMng模块为开机时分配连续不释放模式
//此文件应用无关
******************************************************************************/

#include "RecDev.h"
#include "MemMng.h"
#include <string.h>

//---------------由设备将记录区移至缓冲区中，并返回基址与缓冲长度--------------------
//此函数在擦除前调用，由回调函数提取出有用的记录(一般为最后的最新记录，如屏蔽记录
//等，要求被屏蔽的设备必须要有记录，故其再老，也得需单独提取出来)
const unsigned char *RecDev_pcbRecToBuf(const struct _RecDev *pDev,
                                          unsigned short *pBufSize)
{
  //先计算可用空间
  unsigned char FrameSize = pDev->Desc.FrameSize;
  unsigned short RecCount = (pDev->Desc.End - pDev->Desc.Start) / FrameSize;   
  unsigned short WrRecCount = RecCount >> 1; //最多回写一半记录
  unsigned short FreeRecCount = (MEM_MNG_HEAP_SIZE - xNextFreeByte) / FrameSize;
  if(FreeRecCount < WrRecCount) WrRecCount = FreeRecCount;
  unsigned short BufSize = WrRecCount * FrameSize;
  *pBufSize = BufSize;
  //得到内存位置
  const unsigned char *pFlashPos = (unsigned char *)pDev->Desc.Start;
  pFlashPos += (RecCount - WrRecCount) * FrameSize;//最后的记录
  unsigned char *pSteal = (unsigned char *)MemMng_pvSteal();
  //缓冲记录：屏蔽记录单独处理(暂略),其它直接将最后空间copy走
  memcpy(pSteal, pFlashPos, BufSize);
  return pSteal;
}




