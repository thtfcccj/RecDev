## 用于作历史记录的记录存储实现，内含
   ### RecDev.h 不依赖于硬件的标准接口

   ### RecDev_xxxx .c 标准接口在各硬件上的实现，含：
   #### RecDev_Eeprom.c 在EEPROM上的实现(依赖Eeprom模块)
   #### RecDev_Flashs.c 在Falsh单页上的实现(依赖Flash模块)

   ### RecDev_FlashS_cbMemMng.c
          RecDev_Flashs.c 中，内部擦洗时，所需缓存在MemMng上的实现(依赖MemMng模块)

