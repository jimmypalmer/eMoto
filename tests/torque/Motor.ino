#include "Motor.h"

void
Motor::setup( void ) 
{
  Serial.println("CAN Setup -- Started");
  CAN_cfg.speed = CAN_SPEED_250KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_17;
  CAN_cfg.rx_pin_id = GPIO_NUM_16;
  CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
  // Init CAN Module
  ESP32Can.CANInit();

  // throttle pin config
  ledcSetup(0, 10000, 12);              // Config PWM output (dunno, frequency, bits)
  ledcAttachPin(GPIO_throttleOUT, 0);   // Config throttle out pin

  // HV control config
  pinMode(GPIO_precharge, OUTPUT);
  pinMode(GPIO_contactor, OUTPUT);
  pinMode(GPIO_discharge, OUTPUT);
  digitalWrite(GPIO_precharge, LOW);
  digitalWrite(GPIO_contactor, LOW);
  digitalWrite(GPIO_discharge, LOW);
  HVcntrlTimer = millis();

  delay(400);
  
  Serial.println("CAN Setup -- Finished");
}

void
Motor::setHV( int cmdHV )
{
    HVstatus = cmdHV;
    if ( cmdHV == 1 ) { // turn-on
//      Serial.println("Confirmed -- turn-on");
      digitalWrite(GPIO_discharge, LOW);
      digitalWrite(GPIO_contactor, LOW);
      digitalWrite(GPIO_precharge, HIGH);
      HVcntrlTimer = millis();
      delay(200);
    } else if ( cmdHV == 3 ) { // turn-off
//      Serial.println("Confirmed -- turn-off");
      digitalWrite(GPIO_discharge, LOW);
      digitalWrite(GPIO_precharge, LOW);
      digitalWrite(GPIO_contactor, LOW);
      HVcntrlTimer = millis();
      delay(200);
      digitalWrite(GPIO_discharge, HIGH);      
    }
}

void
Motor::HVcontrol( void )
{
  if ( HVstatus == 0 ) {            // OFF
    // TODO
  } else if ( HVstatus == 2 ) {     // ON
    // TODO
  } else if ( (HVstatus == 1 ) && (millis() > HVcntrlTimer + HVonHoldoff) ) {    
    digitalWrite(GPIO_contactor, HIGH);
    digitalWrite(GPIO_precharge, LOW);
    Serial.println("HV on");
    HVstatus = 2;
    delay(100);
//  } else if ( (HVstatus == 3 ) && (millis() > HVcntrlTimer + 1000) ) {
//    digitalWrite(GPIO_discharge, HIGH);
//    Serial.println("Bleedin");
//    delay(10);
  } else if ( (HVstatus == 3 ) && (millis() > HVcntrlTimer + HVoffHoldoff) ) {
    digitalWrite(GPIO_discharge, LOW);
    HVstatus = 0;
    Serial.println("HV off");
    delay(100);
  }

}

void
Motor::restart( void )
{
    // TODO
}

void
Motor::service( void )
{
    // TODO
    // if (not working) restart;

    // TODO
    // if (error) writeThrottle(0);

    readThrottle();
    readCAN();
    writeThrottle();
    HVcontrol();
}

int
Motor::readThrottle( void )
{
  throttleIN = (analogRead(GPIO_throttleIN)-630) * 1000 / (4095-630);
  if (throttleIN<0) { throttleIN = 0; }
  throttleOUT = (throttleIN*4095)/1000;
//  Serial.println("Throttle GOT");

  return throttleOUT;
}

void
Motor::writeThrottle( void )
{
    ledcWrite(0, throttleOUT);
//    Serial.println("Throttle SET");
}

void
Motor::readCAN( void ) 
{
  
  CAN_frame_t rx_frame;

  unsigned long currentMillis = millis();

  // Receive next CAN frame from queue
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {

    // Motor Message 1
    if ( rx_frame.MsgID == 0x0CF11E05 ) {

      // Bits
      int bit0 = rx_frame.data.u8[0];
      int bit1 = rx_frame.data.u8[1];
      int bit2 = rx_frame.data.u8[2];
      int bit3 = rx_frame.data.u8[3];
      int bit4 = rx_frame.data.u8[4];
      int bit5 = rx_frame.data.u8[5];
      uint8_t bit6 = rx_frame.data.u8[6];
      uint8_t bit7 = rx_frame.data.u8[7];

      // Speed, Current, Voltage
      RPM = ( (bit1*256) + bit0 );
      currentMtr = ( (bit3*256) + bit2 ) / 10;
      voltageDC = ( (bit5*256) + bit4 ) / 10;

      uint8_t bitsCount = 8;
//      char ERR[ bitsCount*2 + 1 ];
      
      uint8_t i = 0;
//      uint8_t j = 0;
      while ( i < bitsCount ) {
        ERR[ i ] = bitRead( bit6, i ) + '0';
        i+=1;
      } 
      while ( (i < bitsCount*2) && (i >= bitsCount) ) {
        ERR[ i ] = bitRead( bit7, i - bitsCount ) + '0';
        i+=1;
      }
      ERR[ i ] = '\0';
    }

    // Motor Message 2
    if ( rx_frame.MsgID == 0x0CF11F05 ) {

      // Bits
      int bit0 = rx_frame.data.u8[0];
      int bit1 = rx_frame.data.u8[1];
      int bit2 = rx_frame.data.u8[2];
      int bit3 = rx_frame.data.u8[3];
      uint8_t bit4 = rx_frame.data.u8[4];
      uint8_t bit5 = rx_frame.data.u8[5];
      uint8_t bit6 = rx_frame.data.u8[6];
      uint8_t bit7 = rx_frame.data.u8[7];

      // Speed, Current, Voltage
      float temp = ( (float)bit0 * (100.0/255.0) );
      throttleRead = (int)temp;
      tempInv = bit1 - 40;
      tempMtr = bit2 - 30;

      uint8_t bitsCount = 8;
//      char statusInv[ bitsCount*2 + 1 ];
      uint8_t i = 0;
      while ( i < bitsCount ) {
        statusInv[ i ] = bitRead( bit4, i ) + '0';
        i+=1;
      } 
      while ( (i < bitsCount*2) && (i >= bitsCount) ) {
        statusInv[ i ] = bitRead( bit5, i - bitsCount ) + '0';
        i+=1;
      }
      statusInv[ i ] = '\0';

      statusCmd = 'N';     
      if ( (statusInv[0] == '0') & (statusInv[1] == '0') ) {
        statusCmd = 'N';
      } else if ( (statusInv[0] == '1') & (statusInv[1] == '0') ) {
        statusCmd = 'F';
      } else if ( (statusInv[0] == '0') & (statusInv[1] == '1') ) {
        statusCmd = 'R';
      }

      statusFb = 'N';     
      if ( (statusInv[2] == '0') & (statusInv[3] == '0') ) {
        statusFb = 'N';
      } else if ( (statusInv[2] == '1') & (statusInv[3] == '0') ) {
        statusFb = 'F';
      } else if ( (statusInv[2] == '0') & (statusInv[3] == '1') ) {
        statusFb = 'R';
      }

    }

    if ( print_mtr_CAN && ((rx_frame.MsgID == 0x0CF11E05) || (rx_frame.MsgID == 0x0CF11E05)) ) {
      Serial.print("RPM: ");
      Serial.print(RPM);
      Serial.print("  I: ");
      Serial.print(currentMtr);
      Serial.print("  VDC: ");
      Serial.print(voltageDC);
      Serial.print("  ERRORS: ");
      Serial.print(ERR);
      Serial.print("  ThIN: ");
      Serial.print(throttleIN);
      Serial.print("  ThR: ");
      Serial.print(throttleRead);
      Serial.print("  Ti: ");
      Serial.print(tempInv);
      Serial.print("  Tm: ");
      Serial.print(tempMtr);
      Serial.print("  CMD: ");
      Serial.print(statusCmd);
      Serial.print("  FB: ");
      Serial.print(statusFb);
      Serial.println();
    }
    
  }
}
