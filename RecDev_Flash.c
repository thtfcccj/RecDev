/******************************************************************************

              ��¼ר�õײ���������-��Flash�е�ʵ��
������дʱ����֤����һ����¼�Ŀռ�Ϊȫ1���Դ˱�֤�ػ����ܲ��ҵ���ʼλ��
ÿ���洢���Լ�¼���ȶ��룬��ȡģ������������!
ע����¼��ַ��ʼ�����������ΪFlashһҳ����ʼ���������
******************************************************************************/
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
  RecDevAdr_t End = pDev->Desc.End - FLASH_PAGE_SIZE; //�������ҳ
  //�Ȳ��ҵ�ÿ�������Ƿ������һ����¼,û�����ʾ�ڴ�ҳ
  RecDevAdr_t PageEndPos = (FLASH_PAGE_SIZE - FrameSize - FLASH_PAGE_SIZE % FrameSize);
  for(; Start < End; Start += FLASH_PAGE_SIZE){
    Flash_Read(Start + PageEndPos, pDev->RdBuf, FrameSize);
    if(!_RdBufIsValid(pDev)) break; //��ҳδд��
  }
  if(Start < End){//���ҳ��ǰ�ػ�: ��һҳ�����ݼ���Ϊ�й��ػ�
    Flash_Read(Start + FLASH_PAGE_SIZE, pDev->RdBuf, FrameSize);
    if(_RdBufIsValid(pDev)) pDev->Looped = 1;
  }
  
  //��0����¼(��ֵ��0��1����¼�޷�����,���޳���0��)��������
  Flash_Read(Start, pDev->RdBuf, FrameSize);
  if(!_RdBufIsValid(pDev)){//ûʹ�ù�ʱ,���м�ֵǰ�� 
    pDev->NextWrPos = Start;
    return;
  }    
  
  //������ֵ�����Ҽ�¼λ��
  unsigned short FindMin = 0;  
  unsigned short RecCount = FLASH_PAGE_SIZE / FrameSize;
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
  if(FindMax == RecCount) Start = (RecCount - 1) * FrameSize;//���һ����
  else Start += (FindMax * FrameSize);//�м�λ��
  pDev->NextWrPos = Start;
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

//----------------------------��¼�����豸д����-----------------------
void RecDev_Wr(struct _RecDev *pDev)
{
  unsigned char FrameSize = pDev->Desc.FrameSize;
  //��ǰ�õ��´�д�����λ��
  RecDevAdr_t NextPos = pDev->NextWrPos + FrameSize;  
  //��һ����¼д����ʱ������ǰ��ʽ����һҳ���Է�ֹ�ڼ�ػ�����ʶ��ͷ
  if(((NextPos + (FrameSize - 1)) % FLASH_PAGE_SIZE) <= FrameSize){
    if(NextPos >= pDev->Desc.End){//д�����ػ�����ʼ��
      NextPos = pDev->Desc.Start;
      pDev->Looped = 1;
    }
    else{ //���һ����¼д���£�������������ҳ��ʼ
      NextPos += FLASH_PAGE_SIZE % FrameSize; 
      //���һҳ����ʽ���ˣ�����Ҫ�ػ�������
      if(NextPos >= (pDev->Desc.End - FLASH_PAGE_SIZE))
        pDev->Looped = 0;
    }
    _FormatFlash(NextPos);
  }

  //д�뱾�μ�¼
  _WrFlash(pDev->NextWrPos, pDev->WrBuf, FrameSize);
  pDev->NextWrPos = NextPos;
}

//------------------------�õ���¼ҳ��-------------------------------
static unsigned char _GetPageCount(const struct _RecDev *pDev)
{
  return (pDev->Desc.End - pDev->Desc.Start) / FLASH_PAGE_SIZE;
}

//------------------------�õ���¼����-------------------------------
unsigned short RecDev_GetCapability(const struct _RecDev *pDev)
{
  //ÿҳ�ɼ�¼����
  unsigned short PageRecCount = FLASH_PAGE_SIZE / pDev->Desc.FrameSize;
  return PageRecCount * _GetPageCount(pDev);
}

//------------------------�õ��ѱ����¼����---------------------------
unsigned short RecDev_GetCount(const struct _RecDev *pDev)
{
  unsigned char FullPageCount; //��д��ҳ����
  if(pDev->Looped){//�ػ���
    FullPageCount = _GetPageCount(pDev) - 1;//��һҳδд��
  }
  else{//û�ػ�
    FullPageCount = (pDev->NextWrPos - pDev->Desc.Start) / 
                     FLASH_PAGE_SIZE;
  }
  //ÿҳ�ɼ�¼����
  unsigned char FrameSize = pDev->Desc.FrameSize;   
  return FullPageCount * (FLASH_PAGE_SIZE / FrameSize) + //��д����ҳ��
         (pDev->NextWrPos % FLASH_PAGE_SIZE) / FrameSize;//δд����
}

//-----------------------��¼�����豸������-----------------------
//��ָ�����ݶ�ȡ����������,������һ����ʷ��¼���ܵ��ô˺�������
//���ػ�������ַ��NULL��ʾ��¼���޻����
const unsigned char *RecDev_Rd(struct _RecDev *pDev,
                               unsigned short RecId)
{
  if(RecId >= RecDev_GetCount(pDev)) return NULL; //��λ�ó���
  
  unsigned char FrameSize = pDev->Desc.FrameSize; 
  unsigned short CurPageRecCount = //��ǰ����д��ҳ�ļ�¼����
                 (pDev->NextWrPos % FLASH_PAGE_SIZE) / FrameSize;
  if(RecId < CurPageRecCount){//��д��ҳ��
    Flash_Read((pDev->NextWrPos - FrameSize) - RecId * FrameSize, 
               pDev->RdBuf, FrameSize);
    return pDev->RdBuf;    
  }
  
  //��ҳ��,�ȼ���ҳλ��
  RecId -= CurPageRecCount;//��ҳ������
  unsigned short PageRecCount = FLASH_PAGE_SIZE / FrameSize;//ÿҳ�ɼ�¼����
  unsigned char WrPagePos = //д��λ��ҳλ���Ӵ˿�ʼ��ǰ��
                (pDev->NextWrPos - pDev->Desc.Start) / FLASH_PAGE_SIZE;
  
  //��Ҫ���ҳ��ת��������ҳ��
  unsigned char PagePos = RecId / PageRecCount;//��Ҫ���ҳ��  
  if(PagePos >= WrPagePos){//�ػ��������
    PagePos = PagePos - WrPagePos;//��ػ���ҳ��
    PagePos = (_GetPageCount(pDev)  - 1) - PagePos; //����ػ�λ��
  }
  else{//��pDev->Desc.Start��ҳ��û�лػ�
    PagePos = (WrPagePos - 1) - PagePos;
  }
  //�ټ����¼λ��
  RecDevAdr_t RdPos = pDev->Desc.Start + PagePos * FLASH_PAGE_SIZE;//��ĳҳ��
  RdPos += ((PageRecCount - 1) - (RecId % PageRecCount)) * FrameSize;//����λ��,������
  
  Flash_Read(RdPos, pDev->RdBuf, FrameSize);
  return pDev->RdBuf;
}




