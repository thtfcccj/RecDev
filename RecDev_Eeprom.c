/******************************************************************************

              ��¼ר�õײ���������-��EEPROM�е�ʵ��
������дʱ����֤����һ����¼�Ŀռ�Ϊȫ1���Դ˱�֤�ػ����ܲ��ҵ���ʼλ��
�洢���Լ�¼����Ϊ��־����
******************************************************************************/

#include "RecDev.h"

#include  "Eeprom.h"  //�����˽ӿ��Բ���EEPROM
#include <string.h>

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
  Eeprom_Rd(Start, pDev->RdBuf, FrameSize);//�ȶ�������¼����������
  if(!_RdBufIsValid(pDev)){//����������Ч����Ϊ�ճ�ʼ������û�ù�
    pDev->LastWrPos = Start;
    return;
  }
  //������ֵ�����Ҽ�¼���һ��д��¼λ��
  unsigned short FindMin = 0;  
  unsigned short RecCount = (pDev->Desc.End - Start) / FrameSize;
  unsigned short FindMax = RecCount;
  unsigned short FindMid = RecCount / 2;
  while((FindMax - 1) > FindMin){
    Eeprom_Rd((Start + (FindMid * FrameSize)), pDev->RdBuf, FrameSize);
    if(!_RdBufIsValid(pDev))//ûʹ�ù�ʱ,���м�ֵǰ��
      FindMax = FindMid;
    else//ʹ�ù�ʱ,���м�ֵ����
      FindMin = FindMid;
    FindMid = (FindMax + FindMin) / 2;
  }
  //�õ��Ƿ�ػ�����ȡҳ���һ����¼,�������ݣ���ػ���
  RecDevAdr_t RdPos = pDev->Desc.End - ((pDev->Desc.End - Start) % FrameSize);
  RdPos -= FrameSize;//ҳ����¼λ��
  Eeprom_Rd(RdPos, pDev->RdBuf, FrameSize);
  if(_RdBufIsValid(pDev)){//����ʱ
    pDev->Looped = 1;//�ûػ���־
  }
  
  //��ʼ��дͷ
  if(FindMax == RecCount)//ȫ���쳣:�������һ����¼ʱʧ�ܣ��ݴ�ǰ��ʼ
    pDev->LastWrPos = Start;//�õ�дλ��
  else
    pDev->LastWrPos = (FindMax * FrameSize) + Start;//�õ�дλ��
}

//-----------------------��ʽ����¼������--------------------------------
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
  RecDev_Init(pDev, &pDev->Desc);//���³�ʼ��
}

//-----------------------�õ�д��λ�ú���-------------------------
static RecDevAdr_t _GetWrPos(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  // * 2Ϊ׼��д��Ľ���λ��
  RecDevAdr_t WrPos = pDev->LastWrPos + FrameSize * 2; 
  if(WrPos > pDev->Desc.End){//�ػ���
    pDev->Looped = 1;
    WrPos = pDev->Desc.Start;
  }
  else WrPos -= FrameSize; //����дλ����
  return WrPos;
}

//----------------------------��¼�����豸д����-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t WrPos = _GetWrPos(pDev);
  Eeprom_Wr(WrPos, pDev->WrBuf, FrameSize);
  //д��ɹ�
  pDev->LastWrPos = WrPos;
  //���ڻػ�״̬������ǰ�轫�¸���¼(�����һ��дλ��)ɾ�����������������ҵ��ػ�λ��
  if(pDev->Looped){
    memset(pDev->WrBuf, 0xff, FrameSize);
    Eeprom_Wr( _GetWrPos(pDev), pDev->WrBuf, FrameSize);
  }
}

//------------------------�õ��ѱ����¼����---------------------------
unsigned short RecDev_GetCount(struct _RecDev *pDev)
{
  if(pDev->Looped){//�ػ���(-1����Ϊ���һ��������ǰ���)
    return (pDev->Desc.End - pDev->Desc.Start) / pDev->Desc.FrameSize - 1;
  }
  //û�ػ�
  return (pDev->LastWrPos - pDev->Desc.Start) / pDev->Desc.FrameSize;
}

//-----------------------��¼�����豸������-----------------------
//��ָ�����ݶ�ȡ����������,������һ����ʷ��¼���ܵ��ô˺�������
//���ػ�������ַ��NULL��ʾ��¼���޻����
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId)
{
  if(RecId >= RecDev_GetCount(pDev)) return NULL; //��λ�ó���
  //���д��λ���ǵ�һ��
  RecDevAdr_t RdPos = RecId * pDev->Desc.FrameSize + pDev->LastWrPos; //�õ���ƫ��
  if(RdPos > pDev->LastWrPos){ //�ػ���
    RdPos -= pDev->LastWrPos; //�����ػ�����
    //���λ�����²��ܴ�ŵ�
    unsigned char Leave = (pDev->Desc.End - pDev->Desc.Start) % pDev->Desc.FrameSize;
    RdPos =  pDev->Desc.End - (RdPos + Leave);
  }
  else RdPos = pDev->LastWrPos - RdPos;
  Eeprom_Rd(RdPos, pDev->RdBuf, pDev->Desc.FrameSize);
  return pDev->RdBuf;
}



