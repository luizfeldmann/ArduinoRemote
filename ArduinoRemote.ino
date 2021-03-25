#include <IRremote.h>

int RECV_PIN = 11;

IRrecv irrecv(RECV_PIN);

decode_results results;


void setup() {
  Serial.begin(9600);
  irrecv.enableIRIn();
}

unsigned long lastcmd = 0;
unsigned long lasttime = 0;
void loop()
{
  if (irrecv.decode(&results)) 
  {
    unsigned long now = millis();
    
    if ((results.value != lastcmd || now - lasttime > 100) /*&& (results.decode_type == 3)*/ /*&& (results.value < 65535)*/)
    {
      //Serial.print(results.decode_type);
      Serial.print(results.value, HEX);
      Serial.print('\n');
      Serial.flush();
    }
    
    lasttime = now;  
    lastcmd = results.value;
    irrecv.resume();
  }

  if (Serial.available() > 0) { Serial.read(); }
}
