#include <mbed.h>
#include <USBSerial.h>

#define n_samples 3000

uint8_t check_status(char);
void read_pressure(void);
void check_reduction_rate(void);
void push_counter(void);
void myTicker(void);
void calculate_stats(void);

USBSerial serial;

I2C i2c(I2C_SDA, I2C_SCL);
// button interrupt
InterruptIn buttonInterrupt(USER_BUTTON);
DigitalOut led1(LED3);
DigitalOut led2(LED4);
DigitalOut led3(LED5);
DigitalOut led4(LED6);

volatile uint32_t output_min = (uint32_t)((2.4/100)*16777216);
volatile uint32_t output_max = (uint32_t)((22.5/100)*16777216);
volatile uint32_t P_max = 300;
volatile uint32_t P_min = 0;
volatile float pressure = 0;
volatile float lastPressure = 0;
volatile uint32_t myTick = 0;
float buf[n_samples]; 
volatile uint32_t counter = 0;
volatile uint8_t state = 0;
const int I2CADD = 0x18 << 1;
volatile bool top_reached = false;
volatile float t1 = 0;
volatile float t2 = 0;
volatile float nSeconds = 0;
char command[10];
Timer timer;


int main() {
  Ticker t;
  buttonInterrupt.rise(&push_counter);
  t.attach_us(&myTicker, 1000);
  while(1){
    read_pressure();
    
    if(state == 3){
      led2.write(1);
      calculate_stats();
      led2.write(0);
      state = 0;
    }
    if(state==1){
      serial.printf("DS: %.2f DE\n",pressure);
    }
    if(state == 2){
      if (strcmp(command,"") != 0){
        serial.printf("%s",command);
      }
    }
    wait_ms(10);
    }
}

// system tick ISR
void myTicker(void) {

  if ((myTick%100)==0){
     if(state==2 and top_reached and (lastPressure-pressure)>0){
       volatile float pumping_speed = (lastPressure-pressure)/(t1-t2);
        if(pumping_speed>5){
          strcpy(command, "CD: F CE\n");
        }
        else if(pumping_speed<3){
          strcpy(command, "CD: S CE\n");
        }
        else{
          strcpy(command, "CD: N CE\n");
        }
     }
     lastPressure = pressure;
     t2 = timer.read();
  }
  myTick++;
}

void push_counter(void) {
  volatile static uint32_t lastTick = 0;

  if (myTick - lastTick > 300) {

    state += 1;
    if(state!=1){
      led1.write(0);
      led2.write(0);
      led4.write(0);
    }
    if (state == 4){
      state = 0;
    }
    if(state==0){
      counter = 0;
      top_reached = false;
      nSeconds = 0;
      timer.stop();
      strcpy(command, "");
      led1.write(0);
      led2.write(0);
      led4.write(0);
    }
    if(state==1){
      led1.write(1);
      led2.write(1);
      led4.write(1);
    }
    lastTick = myTick;
  }
  
}

void read_pressure(void){
  volatile uint32_t Output;
  char in_data[4];
  char cmd[3];
  char status[1];
  cmd[0] = 0xAA;
  cmd[1] = 0x00;
  cmd[2] = 0x00;
  volatile int count = 0;
  count = i2c.write(I2CADD, cmd, 3);
  count = i2c.read(I2CADD, status, 1);
  while(check_status(status[0])==2){
    wait_ms(5);
    count = i2c.read(I2CADD, status, 1);
  }
  if(check_status(status[0])==0){
    count=i2c.read(I2CADD, in_data, 4);
    Output = ((uint8_t)in_data[3])|((uint8_t)in_data[2]<<8)|((uint8_t)in_data[1]<<16);
    pressure = (float)((Output-output_min)*(300/((float)(output_max-output_min))));
  }
  else{
    pressure = lastPressure;
  }
  t1 = timer.read();
  if( pressure >= 150){
    top_reached = true;
    timer.start();
    led3.write(1);
  }
  else{
      led3.write(0);
  }
  if(state==2){
     led1.write(1);
     if ((counter <= n_samples) and top_reached){
         buf[counter] = pressure;
         counter += 1;
     }
     if (top_reached and (pressure <= 30)){
         timer.stop();
         nSeconds = timer.read();
         led1.write(0);
     }
  }
}

uint8_t check_status(char status){
  if(!(status & 1<<6)){
    //serial.printf("Device is powered off!");
    return 1;
  }
  if(status & 1<<5){
       //serial.printf("Device is busy!");
       return 2;
  }
  if(status & 1<<2){
    //serial.printf("Device memory test failed!");
    return 3;
  }    
  if(status & 1<<0){
    //serial.printf("Internal math saturation has occurred!");
    return 4;
  }
  return 0;
}

void calculate_stats(void){
  volatile int posmax = 0;
  volatile int posmax_k = 0;
  volatile int k = 0;
  volatile float max_pressure = 0;
  volatile float pressure_diff[n_samples];
  volatile float filtered[n_samples];
  volatile int heart_beat = 0;
  volatile int x_peak[1000];
  volatile int num_sam=2;
  for (int i = (num_sam/2); i < n_samples-(num_sam/2);i++){
    double sum = 0;
    for(int j=-1*(num_sam/2);j<(num_sam/2);j++){
        sum += buf[i-j];
    }
    filtered[k] = sum/num_sam;
    k += 1;
  }
  k = 0;
  for(int i=0;i<n_samples-2;i++){
    if (filtered[i]>=30){
      pressure_diff[k] = filtered[i+1] - filtered[i];
      if (k==0){
        max_pressure = pressure_diff[k];
      }
      else{
        if(pressure_diff[k]>max_pressure){
          max_pressure = pressure_diff[k];
          posmax = i;
          posmax_k = k;
        }
      }
      if((filtered[i] > filtered[i-1]) and (filtered[i] > filtered[i+1])){
        heart_beat += 1;
      }
      k += 1;
    }
  }
  for(int i = 0;i < n_samples;i++){
      pressure_diff[i] /= max_pressure;
  }
  k = 0;
  for(int i = posmax_k;i > 0 ;i--){
      if((pressure_diff[i] > pressure_diff[i-1]) and (pressure_diff[i] > pressure_diff[i+1]) and (pressure_diff[i]>0.3)){
            x_peak[k] = i;
            k += 1;
      }
  }
  for(int i = 0;i < 1000;i++){
      if((pressure_diff[x_peak[i]]>=0.5) and (pressure_diff[x_peak[i+1]]<0.5)){
          double SP = filtered[x_peak[i]];
          double DP = (3*filtered[posmax]-filtered[x_peak[i]])/2;
          double HR = (40.47/(4.14-log((filtered[posmax]-DP)/(0.01*(SP-DP)))));//This formula comes from this paper: Calculation of Mean Arterial Pressure During Exercise as a Function of Heart Rate
          //double HR = (float)(heart_beat/nSeconds);
          serial.printf("SP: %.2f CE\n",SP);
          wait_ms(1000);
          serial.printf("DP: %.2f CE\n",DP);
          wait_ms(1000);
          serial.printf("HR: %.2f CE\n",HR); // 
          //serial.printf("HR: %f CE\n", (float)(heart_beat/timer.read()));
          wait_ms(1000);
          return;
      }
  }
}