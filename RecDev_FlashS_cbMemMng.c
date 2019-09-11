/******************************************************************************

              ��¼ר�õײ���������ص�����-ʹ�öѿռ�ʱ��ʵ��
//��ģ��Ҫ��MemMngģ��Ϊ����ʱ�����������ͷ�ģʽ
//���ļ�Ӧ���޹�
******************************************************************************/

#include "RecDev.h"
#include "MemMng.h"
#include <string.h>

//---------------���豸����¼�������������У������ػ�ַ�뻺�峤��--------------------
//�˺����ڲ���ǰ���ã��ɻص�������ȡ�����õļ�¼(һ��Ϊ�������¼�¼�������μ�¼
//�ȣ�Ҫ�����ε��豸����Ҫ�м�¼���������ϣ�Ҳ���赥����ȡ����)
const unsigned char *RecDev_pcbRecToBuf(const struct _RecDev *pDev,
                                          unsigned short *pBufSize)
{
  //�ȼ�����ÿռ�
  unsigned char FrameSize = pDev->Desc.FrameSize;
  unsigned short RecCount = (pDev->Desc.End - pDev->Desc.Start) / FrameSize;   
  unsigned short WrRecCount = RecCount >> 1; //����дһ���¼
  unsigned short FreeRecCount = (MEM_MNG_HEAP_SIZE - xNextFreeByte) / FrameSize;
  if(FreeRecCount < WrRecCount) WrRecCount = FreeRecCount;
  unsigned short BufSize = WrRecCount * FrameSize;
  *pBufSize = BufSize;
  //�õ��ڴ�λ��
  const unsigned char *pFlashPos = (unsigned char *)pDev->Desc.Start;
  pFlashPos += (RecCount - WrRecCount) * FrameSize;//���ļ�¼
  unsigned char *pSteal = (unsigned char *)MemMng_pvSteal();
  //�����¼�����μ�¼��������(����),����ֱ�ӽ����ռ�copy��
  memcpy(pSteal, pFlashPos, BufSize);
  return pSteal;
}




