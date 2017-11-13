
/*


Connections are as follows:

+-----------+-------------+
| ESP8266   | AVR / SPI   |
+===========+=============+
| GPIO12    | MISO        | voltage devider needed if AVR runs on 5v
+-----------+-------------+
| GPIO13    | MOSI        |
+-----------+-------------+
| GPIO14    | SCK         |
+-----------+-------------+
| any\*     | RESET       |
+-----------+-------------+

*/

void setupAVRISP(){
    avrprog.setReset(false); // let the AVR run
    avrprog.begin();  
  
    MDNS.addService("avrisp", "tcp", avrisp_port);  
    
    IPAddress local_ip = WiFi.localIP();
    Serial.print(F("[AVRISP] avrdude -c arduino -p <device> -P net:"));
    Serial.print(local_ip);
    Serial.print(":");
    Serial.print(avrisp_port);
    Serial.println(F(" -t # or -U ..."));

  
}

void handleAVRISP() {
    static unsigned int i = 0;
    static AVRISPState_t last_state = AVRISP_STATE_IDLE;
    AVRISPState_t new_state = avrprog.update();
    if (last_state != new_state) {
        switch (new_state) {
            case AVRISP_STATE_IDLE: {
                Serial.println();
                Serial.println(F("[AVRISP] now idle"));
                // Use the SPI bus for other purposes
                break;
            }
            case AVRISP_STATE_PENDING: {
                Serial.println(F("[AVRISP] connection pending"));
                // Clean up your other purposes and prepare for programming mode
                break;
            }
            case AVRISP_STATE_ACTIVE: {
                Serial.println();
                Serial.println(F("[AVRISP] programming mode"));
                // Stand by for completion
                break;
            }
        }
        last_state = new_state;
    }
    // Serve the client
    if (last_state != AVRISP_STATE_IDLE) {
        if ((i++ % 4096) == 0) Serial.print(".");
        avrprog.serve();
    }
}
