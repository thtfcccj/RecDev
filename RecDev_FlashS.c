/******************************************************************************

              ��¼ר�õײ���������-�ڵ�Flash��Sector�е�ʵ��
  ��Sector���û�д���¼�¼��ʽ���У��ڴ�Sector��ʱ�����洢����������������ʱRAM�У�
��Flash���������ٻ�д����ʼ������д�������ļ�¼��

  ��Sector�����ڻػ����⣬�����ڻ�д���ٸ��ֽڣ��Լ���д�ֽڶ�������
  ��д�ֽڽ���Ϊ������һ�롣
******************************************************************************/

#include "RecDev.h"

#include "RecDev_FlashS.h"
#include  "Flash.h"  //�����˽ӿ��Բ���Flash
#include <string.h>

//-------------------------Flashʵ����Eepromʵ�ֵ���ͬ-------------------------
//��ͬ����
//   ���ǿռ������洢���ʳ�ʼ�����ȡ��¼������ͬ(�����ö�д������ͬ�⣬ȥ���ػ�����)
//��ͬ��:
//   ��Flashֻ��һ���������ʲ����ڻػ�����,��д���ݺ�����Ҳ��ͬ

//-------------------------дflash����--------------------------------
void _WrFlash(unsigned long Adr,   //Flash��ַ
              const void *pVoid,    //Ҫд�������
              unsigned long Len)   //д���ݳ���
{
  Flash_Write(Adr, pVoid, Len);
}

//----------------------��ʽ��һҳ����--------------------------------
void _FormatFlash(unsigned long Adr)
{
  Flash_ErasePage(Adr);//����ҳ
}

//--------------------------�ж϶������¼�Ƿ���Ч-----------------------
signed char _RdBufIsValid(const struct _RecDev *pDev)
{
  for(unsigned char i = 0; i < pDev->Desc.FrameSize; i++)
    if(pDev->RdBuf[i] != 0xff) return 1; //��Ч
  return 0; //ȫ1��Ч
}

//----------------------------��¼�����豸��ʼ������-----------------------
void RecDev_Init(struct _RecDev *pDev, const struct _RecDevDesc *pDesc)
{
  //��ʼ���ṹ(Ϊ��ֹpDesc��ַ��ͬ���������ʼ������)
  memcpy(&pDev->Desc, pDesc, sizeof(struct _RecDevDesc));
  
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t Start = pDev->Desc.Start; 
  Flash_Read(Start, pDev->RdBuf, FrameSize);//�ȶ�������¼����������
  if(!_RdBufIsValid(pDev)){//����������Ч����Ϊ�ճ�ʼ������û�ù�
    pDev->NextWrPos = Start;
    return;
  }
  //������ֵ�����Ҽ�¼���һ��д��¼λ��
  unsigned short FindMin = 0;  
  unsigned short RecCount = (pDev->Desc.End - Start) / FrameSize;
  unsigned short FindMax = RecCount;
  unsigned short FindMid = RecCount / 2;
  while((FindMax - 1) > FindMin){
    Flash_Read((Start + (FindMid * FrameSize)), pDev->RdBuf, FrameSize);
    if(!_RdBufIsValid(pDev))//ûʹ�ù�ʱ,���м�ֵǰ��
      FindMax = FindMid;
    else//ʹ�ù�ʱ,���м�ֵ����
      FindMin = FindMid;
    FindMid = (FindMax + FindMin) / 2;
  }
  
  //��ʼ��дͷ
  if(FindMax == RecCount)//ȫ��ʱ
    pDev->NextWrPos = pDev->Desc.End;//���������ٴ�дʱ������
  else
    pDev->NextWrPos = (FindMax * FrameSize) + Start;//�õ����дλ��
}

//-----------------------��ʽ����¼������--------------------------------
void RecDev_Format(struct _RecDev *pDev)
{
  if(RecDev_GetCount(pDev) == 0) return; //��ֹ�ظ���ʼ��
 _FormatFlash(pDev->Desc.Start);
  RecDev_Init(pDev, &pDev->Desc);//���³�ʼ��
}

//----------------------------��¼�����豸д����-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t CurWrPos = pDev->NextWrPos;
  if((CurWrPos + FrameSize) > pDev->Desc.End){ //��¼���ˣ��账��
    unsigned short BufSize;
    const unsigned char *pBufData = RecDev_pcbRecToBuf(pDev, &BufSize);//�����뻺��
    _FormatFlash(pDev->Desc.Start); //������Ƭ����(�����ڼ䲻Ҫ�ϵ�)
    _WrFlash(pDev->Desc.Start, //��д(�����ڼ䲻Ҫ�ϵ�)
             pBufData, BufSize);
    CurWrPos = pDev->Desc.Start + BufSize;
  }
  _WrFlash(CurWrPos, pDev->WrBuf, FrameSize);
  pDev->NextWrPos = CurWrPos + FrameSize; //�������д��λ��
}

//------------------------�õ��ѱ����¼����---------------------------
unsigned short RecDev_GetCount(const struct _RecDev *pDev)
{
  return (pDev->NextWrPos - pDev->Desc.Start) / pDev->Desc.FrameSize;
}

//-----------------------��¼�����豸������-----------------------
//��ָ�����ݶ�ȡ����������,������һ����ʷ��¼���ܵ��ô˺�������
//���ػ�������ַ��NULL��ʾ��¼���޻����
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId)
{
  if(RecId >= RecDev_GetCount(pDev)) return NULL; //��λ�ó���
  //׼��д��λ��ǰ���ǵ�һ��
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t RdPos = (pDev->NextWrPos - FrameSize) - RecId * FrameSize;
  //Flash_Read(RdPos, pDev->RdBuf, FrameSize);
  //return pDev->RdBuf;
  //Ϊ�ӿ��ٶȣ�ֱ�ӷ���flashָ��
  return (const unsigned char *)RdPos;
}

//---------------------------���ĳ����¼����-------------------------------
//��������ü�¼ʱ��λȫ�����, �Ա�ʶ�����
void RecDev_Clr(struct _RecDev *pDev, unsigned short RecId)
{
  const unsigned char *pRec = RecDev_Rd(pDev, RecId); //���ﷵ�ص���׼��д��flashָ��
  if(pRec == NULL) return; //δ�ҵ�
  memset(pDev->WrBuf, 0, 4);
  _WrFlash((unsigned long)pRec - pDev->Desc.Start, pDev->WrBuf, 4);
}

