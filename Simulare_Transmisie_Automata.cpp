#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

#define JOY_Y=A1
#define POT_ACCELERATIE=A0
#define BUTON=D2
#define JOY_CENTER=512
#define JOY_DEADZONE=150
#define JOY_SUS (JOY_CENTER+JOY_DEADZONE)
#define JOY_JOS (JOY_CENTER-JOY_DEADZONE)
LiquidCrystal_I2C lcd(0x27,16,2);
const float RAPORT_CUTIE[]={0,4.377,2.859,1.921,1.368,1.000,0.820,0.728};
const float RAPORT_REV1=3.416;
const float RAPORT_FINAL=3.07;
const float CIRC_ROATA=1.94;
const int MAX_TREAPTA=7;
const int RPM_RELANTI=750;
const int RPM_MAX=5000;
const int RPM_TORQUE_SF=2800;
const int RPM_PUTERE_SF=4200;
const int SCHIMB_SUS_COMFORT[]={1400,1500,1600,1500,1400,1350};
const int SCHIMB_JOS_COMFORT[]={800,850,900,850,800,780};
const int SCHIMB_SUS_SPORT[]={2500,2700,2800,2700,2600,2500};
const int SCHIMB_JOS_SPORT[]={1000,1100,1200,1100,1000,950};
const int SCHIMB_SUS_KICK[]={4200,4300,4400,4300,4200,4100};
const int SCHIMB_JOS_KICK[]={1500,1600,1700,1600,1500,1400};
enum ModTransmisie {P,R,N,D};
ModTransmisie modCurent=N;
ModTransmisie modAnterior=N;
int accel=0;
int treaptaCurenta=1;
float rpmSmooth=750.0;
float vitezaSmooth=0.0;
int rpm=750;
int viteza=0;
int modCondus=0;
unsigned long timpApasare=0;
unsigned long ultimDebounce=0;
unsigned long ultimaLCD=0;
unsigned long ultimulCalcul=0;
unsigned long ultimaSchimbare=0;
unsigned long ultimaManeta=0;
bool butonApasat=false;
int butonDebounce=HIGH;
float calcViteza(float r,float raport){
  if(raport<=0)return 0;
  return (r*60.0*CIRC_ROATA)/(raport*RAPORT_FINAL*1000.0);
}
void citesteButon(){
  unsigned long acum=millis();
  int citire=digitalRead(BUTON);
  if(citire!=butonDebounce)ultimDebounce=acum;
  if(acum-ultimDebounce>50){
    if(citire==LOW&&!butonApasat){
      butonApasat=true;
      timpApasare=acum;
    }else if(citire==HIGH&&butonApasat){
      butonApasat=false;
      modCurent=(acum-timpApasare>=1000)?P:N;
    }
  }
  butonDebounce=citire;
}
void citesteJoystick(){
  unsigned long acum=millis();
  if(acum-ultimaManeta<200)return;
  int joy=analogRead(JOY_Y);
  if(joy>JOY_SUS&&modCurent!=R){
    modCurent=R;
    ultimaManeta=acum;
  }else if(joy<JOY_JOS&&modCurent!=D){
    modCurent=D;
    ultimaManeta=acum;
  }
}
void calculeaza(){
  unsigned long acum=millis();
  if(acum-ultimulCalcul<80)return;
  ultimulCalcul=acum;
  accel=map(analogRead(POT_ACCELERATIE),0,1023,0,100);
  if(accel<35)modCondus=0;
  else if(accel<70)modCondus=1;
  else modCondus=2;
  float rpmTinta=RPM_RELANTI;
  float vitezaTinta=0;
  switch(modCurent){
    case P:
    case N:
      rpmTinta=RPM_RELANTI;
      vitezaTinta=0;
      treaptaCurenta=1;
      break;
    case R:
      if(vitezaSmooth>0.5){
        rpmTinta=RPM_RELANTI;
        vitezaTinta=0;
      }else{
        rpmTinta=map(accel,0,100,RPM_RELANTI,2500);
        vitezaTinta=calcViteza(rpmTinta,RAPORT_REV1);
        if(vitezaTinta>25)vitezaTinta=25;
      }
      treaptaCurenta=1;
      break;
    case D:{
      if(modAnterior==R&&vitezaSmooth>0.5){
        rpmTinta=RPM_RELANTI;
        vitezaTinta=0;
        break;
      }
      int rpmMaxT=(treaptaCurenta<=3)?RPM_TORQUE_SF:RPM_PUTERE_SF;
      rpmTinta=map(accel,0,100,RPM_RELANTI,min(rpmMaxT,RPM_MAX));
      vitezaTinta=calcViteza(rpmTinta,RAPORT_CUTIE[treaptaCurenta]);
      if(acum-ultimaSchimbare>=900){
        const int* pSus=(modCondus==2)?SCHIMB_SUS_KICK:(modCondus==1)?SCHIMB_SUS_SPORT:SCHIMB_SUS_COMFORT;
        const int* pJos=(modCondus==2)?SCHIMB_JOS_KICK:(modCondus==1)?SCHIMB_JOS_SPORT:SCHIMB_JOS_COMFORT;
        int idx=treaptaCurenta-1;
        if((int)rpmTinta>pSus[idx]&&treaptaCurenta<MAX_TREAPTA){
          treaptaCurenta++;
          ultimaSchimbare=acum;
          rpmSmooth*=RAPORT_CUTIE[treaptaCurenta]/RAPORT_CUTIE[treaptaCurenta-1];
        }else if((int)rpmTinta<pJos[idx]&&treaptaCurenta>1){
          treaptaCurenta--;
          ultimaSchimbare=acum;
          rpmSmooth*=RAPORT_CUTIE[treaptaCurenta]/RAPORT_CUTIE[treaptaCurenta+1];
        }
      }
      break;
    }
  }
  float factorRPM=(rpmTinta>rpmSmooth)?0.10:(modCurent==P||modCurent==N)?0.25:0.18;
  rpmSmooth+=(rpmTinta-rpmSmooth)*factorRPM;
  rpmSmooth=constrain(rpmSmooth,0,RPM_MAX);
  float factorV=(vitezaSmooth<8.0)?0.03:(vitezaTinta>vitezaSmooth)?0.09:0.15;
  vitezaSmooth+=(vitezaTinta-vitezaSmooth)*factorV;
  vitezaSmooth=constrain(vitezaSmooth,0,250);
  rpm=(int)rpmSmooth;
  viteza=(int)vitezaSmooth;
}
void actualizeazaLCD(){
  unsigned long acum=millis();
  if(acum-ultimaLCD<250)return;
  ultimaLCD=acum;
  lcd.setCursor(0,0);
  char buf[17];
  char modC=(modCurent==P)?'P':(modCurent==R)?'R':(modCurent==N)?'N':'D';
  char modS=(modCurent==D)?((modCondus==0)?'C':(modCondus==1)?'S':'K'):' ';
  sprintf(buf,"%c%c %4dRPM     ",modC,modS,rpm);
  lcd.print(buf);
  lcd.setCursor(0,1);
  char buf2[17];
  if(modCurent==D)sprintf(buf2,"%3dkm/h T%d %3d%%  ",viteza,treaptaCurenta,accel);
  else if(modCurent==R)sprintf(buf2,"%3dkm/h R  %3d%%  ",viteza,accel);
  else sprintf(buf2,"  --km/h   %3d%%  ",accel);
  lcd.print(buf2);
}
void setup(){
  lcd.init();
  lcd.backlight();
  pinMode(BUTON,INPUT_PULLUP);
  rpmSmooth=750.0;
  vitezaSmooth=0.0;
  treaptaCurenta=1;
  lcd.setCursor(0,0);lcd.print("7G-Tronic W212  ");
  lcd.setCursor(0,1);lcd.print("E220 CDI 170PS  ");
  delay(2000);
  lcd.clear();
}
void loop(){
  citesteButon();
  citesteJoystick();
  calculeaza();
  actualizeazaLCD();
  if(modCurent!=modAnterior){
    modAnterior=modCurent;
    treaptaCurenta=1;
  }
}
