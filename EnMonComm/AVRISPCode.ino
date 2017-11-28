
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
    logger.notice(F("[AVRISP] avrdude -c arduino -p <device> -P net:%s:%l -t # or -U ..." CR), WiFi.localIP().toString().c_str(), avrisp_port);
 
}

void handleAVRISP() {
    static unsigned int i = 0;
    static AVRISPState_t last_state = AVRISP_STATE_IDLE;
    AVRISPState_t new_state = avrprog.update();
    if (last_state != new_state) {
        switch (new_state) {
            case AVRISP_STATE_IDLE: {
                logger.notice(F("[AVRISP] now idle" CR));
                // Use the SPI bus for other purposes
                break;
            }
            case AVRISP_STATE_PENDING: {
                logger.notice(F("[AVRISP] connection pending" CR));
                // Clean up your other purposes and prepare for programming mode
                break;
            }
            case AVRISP_STATE_ACTIVE: {
                logger.notice(F("[AVRISP] programming mode" CR));
                // Stand by for completion
                break;
            }
        }
        last_state = new_state;
    }
    // Serve the client
    if (last_state != AVRISP_STATE_IDLE) {
        if ((i++ % 4096) == 0) logger.notice(".");
        avrprog.serve();
    }
}
