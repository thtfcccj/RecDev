/******************************************************************************

              ��¼ר�õײ���������-��Flash�е�ʵ��
������дʱ����֤����һ����¼�Ŀռ�Ϊȫ1���Դ˱�֤�ػ����ܲ��ҵ���ʼλ��
�洢���Լ�¼����Ϊ��־���룬ÿҳ(FLASH_PAGE_SIZE)���ݶ���
ע����¼��ַ��ʼ�����������ΪFlashһҳ����ʼ���������
******************************************************************************/

//-------------------------Flashʵ����Eepromʵ�ֵ���ͬ-------------------------
//��ͬ����
//   ���ǿռ������洢���ʳ�ʼ�����ȡ��¼������ͬ(�����ö�д������ͬ)
//��ͬ��:
//��Flashֻ��һ�������������ʸ�ʽ����ͬ
//flash����һ����¼��Խ�����������⣬������һ����¼����˸�ʽ���¸���������д�кܴ�ͬ

#include "RecDev.h"

#include  "Flash.h"  //�����˽ӿ��Բ���Flash
#include <string.h>

//-------------------------дflash����--------------------------------
void _WrFlash(unsigned long Adr,   //Flash��ַ
              const void *pVoid,    //Ҫд�������
              unsigned long Len)   //д���ݳ���
{
  Flash_Unlock();//����
  Flash_Write(Adr, pVoid, Len);
  Flash_Lock();//����
  //WDT_Week();//ι��һ��
}

//----------------------��ʽ��һҳ����--------------------------------
void _FormatFlash(unsigned long Adr)
{
  Flash_Unlock();//����
  Flash_ErasePage(Adr);//����ҳ
  Flash_Lock();//����
  //WDT_Week();//ι��һ��
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
    pDev->LastWrPos = Start;
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
  //�õ��Ƿ�ػ�����ȡҳ���һ����¼,�������ݣ���ػ���
  RecDevAdr_t RdPos = pDev->Desc.End - ((pDev->Desc.End - Start) % FrameSize);
  RdPos -= FrameSize;//ҳ����¼λ��
  Flash_Read(RdPos, pDev->RdBuf, FrameSize);
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
  for(; Start < End; Start += FLASH_PAGE_SIZE){
    unsigned short WrSize = End - Start;
    if(WrSize > FLASH_PAGE_SIZE) WrSize = FLASH_PAGE_SIZE;
    _FormatFlash(Start);

  }
  RecDev_Init(pDev, &pDev->Desc);//���³�ʼ��
}

//--------------------------�õ�д��λ�ú���----------------------------
static RecDevAdr_t _GetWrPos(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  // * 2Ϊ׼��д��Ľ���λ��
  RecDevAdr_t WrPos = pDev->LastWrPos + FrameSize * 2; 
  if(WrPos > pDev->Desc.End){//�ػ���
    pDev->Looped = 1;
    WrPos = pDev->Desc.Start;
    _FormatFlash(pDev->Desc.Start); //��ǰ��ʽ��
  }
  else WrPos -= FrameSize; //����дλ����
  return WrPos;
}

//----------------------------��¼�����豸д����-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  RecDevAdr_t WrPos = _GetWrPos(pDev);
  //�õ���������д���ֽ���
  unsigned short NextOffWrPos = (WrPos % FLASH_PAGE_SIZE) + FrameSize;//�´����д��λ��
  unsigned char CurWrCount = FrameSize;
  if(NextOffWrPos > FLASH_PAGE_SIZE){ //������
    CurWrCount -= (NextOffWrPos - FLASH_PAGE_SIZE); //���ο�д��
    _FormatFlash(WrPos + FrameSize);//��ǰ��ʽ��δ����д�����һ����
  };
  _WrFlash(WrPos, pDev->WrBuf, CurWrCount);
  if(CurWrCount < FrameSize){//��д�°벿��
    _WrFlash(WrPos + CurWrCount, //��ҳ��ʼ
             &pDev->WrBuf[CurWrCount], //δд�벿��
             FrameSize - CurWrCount);  //δд�벿������
    pDev->LastWrPos = WrPos; //д��ɹ�
  }
  //���°벿��ʱ�����ػ����������Ƿ�絽��һҳ,������ǰ��ʽ����׼��
  else if(pDev->Looped){
    pDev->LastWrPos = WrPos; //��ǰ������һҳ��_GetWrPos()
    NextOffWrPos = (_GetWrPos(pDev) % FLASH_PAGE_SIZE) + FrameSize;//���´����д��λ��
    if(NextOffWrPos > FLASH_PAGE_SIZE){ //������
      _FormatFlash(WrPos + FrameSize);//��ǰ��ʽ������д�����һ����
    }
  }
  //����FLASH_PAGE_SIZE = 1024, FrameSize = 5
  //��pDev->LastWrPos= 1015�� ����д��λ��1020->��ǰ��ʽ����һҳ->4+1д ��:1025 
  //��pDev->LastWrPos= 1104:  ����д��λ��1019->5д->��ǰ��ʽ����һҳ:1024
}

//------------------------�õ��ѱ����¼����---------------------------
unsigned short RecDev_GetCount(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  if(pDev->Looped){//�ػ���
    //���������д��ҳ�ѱ�ɾ���ļ�¼
    unsigned short MaxCount = (pDev->Desc.End - pDev->Desc.Start) / FrameSize;
    unsigned short OffPos = pDev->LastWrPos % FLASH_PAGE_SIZE;//�ѱ����ҳ�����λ��
    MaxCount -= (FLASH_PAGE_SIZE - OffPos) / FrameSize;
    return MaxCount - 1; //Ԥ��һ����λ��ֹ��ҳ���⡣ 
  }
  //û�ػ�
  return (pDev->LastWrPos - pDev->Desc.Start) / FrameSize;

  //����FLASH_PAGE_SIZE = 1024, FrameSize = 5, ��4ҳ
  //��pDev->LastWrPos= 1025 + �ػ� ����Ϊ: �����ܸ�����(4095 / 5) = 819;
  //819 - (1024 - (1025 % 1024)) / 5 -1 = 615(��Ϊ��С�洢����)
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
  Flash_Read(RdPos, pDev->RdBuf, pDev->Desc.FrameSize);
  return pDev->RdBuf;
}
