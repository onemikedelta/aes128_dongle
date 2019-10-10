#include <12F683.h>
#device adc=8
#ZERO_RAM
#FUSES WDT                      //Watch Dog Timer
#FUSES HS                       //High speed Osc (> 4mhz)
#FUSES CPD                      //Data EEPROM Code Protected
#FUSES PROTECT                //Code not protected from reading
#FUSES MCLR                   //Master Clear pin used for I/O
#FUSES NOPUT                    //No Power Up Timer
#FUSES BROWNOUT                 //Reset when brownout detected
#FUSES NOIESO                   //Internal External Switch Over mode disabled
#FUSES NOFCMEN                  //Fail-safe clock monitor disabled

#use delay(clock=20000000)
#use rs232(baud=57600,parity=N,xmit=PIN_A1,rcv=PIN_A0,bits=8)

//ECB (Electronic Code Book), CBC (Cipher Block Chaining), CFB (Cipher FeedBack), OFB (Output FeedBack) and CTR (Counter).
#define AES128_ECB_enc 1
//#define AES128_ECB_dec 2   //dec needed
//#define AES128_CBC 3       //dec needed
#define AES128_CFB_enc 4
#define AES128_CFB_dec 5
#define AES128_OFB 6    //  never reuse iv 
#define AES128_CTR 7    //  never reuse iv  

#include <aes128.c>
#rom 0x2100 = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x7d}     //initial eeprom key

int mode = AES128_ECB_enc;
//int buffer[16]={0x32,0x88,0x31,0xe0,0x43,0x5a,0x31,0x37,0xf6,0x30,0x98,0x07,0xa8,0x8d,0xa2,0x34};
int IV[16]={0x32,0x88,0x31,0xe0,0x43,0x5a,0x31,0x37,0xf6,0x30,0x98,0x07,0xa8,0x8d,0xa2,0x34};

void main()
{
   int8 i,t,k;
   char rxd;
   setup_adc_ports(NO_ANALOGS|VSS_VDD);
   setup_adc(ADC_OFF);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_1);
   //setup_wdt(WDT_2304MS|WDT_DIV_16);
   setup_wdt(WDT_2304MS|WDT_TIMES_8);
   setup_timer_1(T1_DISABLED);
   setup_timer_2(T2_DISABLED,0,1);
   setup_comparator(NC_NC);
   setup_vref(FALSE);
   
   delay_ms(300);
   
   
   k=1;
   while(k)       //loop for getting correct key, without key, this dongle is meaningless
   {  rxd=getc();
   
      if(rxd == 'K')          //load incoming
       { t=0;
         putc('K');
         for(i=0;i<16;i++) 
            {key[i]=getc();
             t+=key[i];}
            
      if(t+5 == getc())         //get (checksum+5)
       {  printf("key.ok\n");    //key ok
          k=0;}
       else
         printf("key_failed\n");  //key corrupted
        
        }
      else if(rxd == 'k')       //load from eeprom
       {t=0;
        putc('k');
        for(i=0;i<16;i++)
           {key[i]=read_eeprom(i);
            t+=key[i];}
       
         if(t+5 == read_eeprom(16))
            {printf("key.ok\n");    //key ok
             k=0;}
         else
             printf("key_failed\n");  //key corrupted
        }
      }  //while(k)
      
      
   rxd=getc();   
   switch(rxd)
   {
      case 'E': mode = AES128_ECB_enc;       //ECB (Electronic Code Book)
                break;
      case 'F': mode = AES128_CFB_enc;      //CFB (Cipher FeedBack)
                for(i=0;i<16;i++)           //get IV
                  text[i] = getc();         
                break;     
      case 'G': mode = AES128_CFB_dec;      //CFB (Cipher FeedBack)
                for(i=0;i<16;i++)           //get IV
                  text[i] = getc();         
                break;  
      case 'O': mode = AES128_OFB;      //OFB (Output FeedBack)
                for(i=0;i<16;i++)      //get IV
                  text[i] = getc();
                break;
                
      case 'C': mode = AES128_CTR;      //CTR (Counter)
                for(i=8;i<16;i++)       //get nonce 8 bytes and concat with counter
                  IV[i]=getc();         
                for(i=0;i<8;i++)
                  IV[i]=0;
                break;
   
   
      case 'W':t=0;
               for(i=0;i<16;i++)           //get new key
               {restart_wdt();
                key[i] = getc();
                //write_eeprom(i,key[i]);   //use for slower communication rates
                t+=key[i];}
                
                restart_wdt();
                for(i=0;i<16;i++)           //write key
                  {write_eeprom(i,key[i]);}
                write_eeprom(17,t+5);
                reset_cpu();
                while(1);     //wait for wdt reset in case reset not works
                
      
      
   }

   
   while(mode == AES128_ECB_enc)
   {
      restart_wdt();
      for(i=0;i<16;i++) text[i]=getc();           //copy data into encryption variable
      encrypt();
      
      for(i=0;i<16;i++)
         //printf("%02x,",text[i]);
           putc(text[i]);
      putc('\n');
   }
   
                       
    while(mode == AES128_CFB_enc) //enc
    {
        restart_wdt();
        encrypt();
        for(i=0;i<16;i++)
            {text[i]^=getc();
               //printf("%02x,",text[i]);
               putc(text[i]);
               }
    }
    
    while(mode == AES128_CFB_dec) //dec
    {
        restart_wdt();
        encrypt();
        for(i=0;i<16;i++)
            {rxd =getc();
             //printf("%02x,",text[i]^rxd);
             putc(text[i]^rxd);
             text[i]=rxd;}
    }
    
    
    while(mode == AES128_OFB)    //enc dec same
    { 
         restart_wdt();
         encrypt();
         for(i=0;i<16;i++)
            { //printf("%02x,",text[i]^getc());
              putc(text[i]^getc());
            }
         
    }    
         
    while(mode == AES128_CTR)    //enc dec same
    {
      restart_wdt();
      for(i=0;i<16;i++)
         text[i]=IV[i];
      encrypt();
      
      for(i=0;i<16;i++)
         //printf("%02x,",text[i]^getc());
         putc(text[i]^getc());
        
      for(i=0;i<8;i++)     //increase_IV();
      {IV[i]++;
       if(IV[i]!=0)
         break;
      }
        
    }
   
}
