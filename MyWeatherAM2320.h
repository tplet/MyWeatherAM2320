//
// Created by Thibault PLET on 19/05/2020.
//

#ifndef COM_OSTERES_AUTOMATION_ARDUINO_COMPONENT_MYSENSOR_MYWEATHERAM2320_H
#define COM_OSTERES_AUTOMATION_ARDUINO_COMPONENT_MYSENSOR_MYWEATHERAM2320_H

#include <Wire.h>
#include <MySensors.h>
#include <AM2320.h>
#include <ArduinoProperty.h>

class MyWeatherAM2320 {
public:
    /**
     * Constructor
     *
     * @param delay             Delay for buffer validity, in milliseconds
     * @param firstDelay        Delay for first outdate, in milliseconds. Immediate by default.
     * @param startImmediately  Start buffer immediately
     */
    MyWeatherAM2320(
        unsigned int childTempID = 1, 
        unsigned int childHumID = 0, 
        unsigned long intervalSend = 60000U, // 1 min
        unsigned long intervalSendForce = 180000U // 3 min
    ) {
        this->childTempID = childTempID;
        this->childHumID = childHumID;
        this->intervalSend = new DataBuffer(intervalSend);
        this->intervalSendForce = new DataBuffer(intervalSendForce);
        this->enable = true;
    }

    /**
     * Destructor
     */
    ~MyWeatherAM2320()
    {
        delete this->intervalSend;
        delete this->intervalSendForce;
    }

    /**
     * Presentation (for MySensor)
     */
    void presentation()
    {
        if (this->isEnable()) {
            wait(100);
            present(this->childHumID, S_HUM, "Humidity", true);
            wait(100);
            present(this->childTempID, S_TEMP, "Temperature", true);
        }
    }

    /**
     * Setup weather
     */
    void setup()
    {
        if (this->isEnable()) {
            // DHT
            this->dht.begin();
            // Waiting for sampling
            sleep(this->getDHTSamplingPeriod());
            this->bufferMoveForward(this->getDHTSamplingPeriod());

        }
    }

    /**
     * Receive (for MySensor)
     */
    void receive(const MyMessage &message)
    {
        if (this->isEnable()) {
            // ECHO to confirm that probe value right received
            if (message.sensor == this->childTempID && message.isEcho()) {
                // Ok, remove flag, value has been successfully received by server
                this->trySendTemp = 0;
            }
            else if (message.sensor == this->childHumID && message.isEcho()) {
                // Ok, remove flag, value has been successfully received by server
                this->trySendHum = 0;
            }
        }
    }

    /**
     * Process
     */
    void loop()
    {
        if (this->isEnable()) {
            this->process();
        }
    }

    /**
     * Move increment forward to the future
     */
    void bufferMoveForward(unsigned long increment)
    {
        this->intervalSend->moveForward(increment);
        this->intervalSendForce->moveForward(increment);
    }

    /**
     * Send temperature to gateway
     */
    void sendTemperature()
    {
        this->trySendTemp++;
        
        #ifdef MY_DEBUG
        String logMsg = "Send humidity (try ";
        logMsg.concat(this->trySendTemp);
        logMsg.concat(")");
        this->sendLog(logMsg.c_str());
        delete &logMsg;
        #endif

        // Send
        MyMessage message(this->childTempID, V_TEMP);
        message.set(this->temperature, 1);
        send(message, true);

        // Waiting for ACK confirmation
        wait(MY_SMART_SLEEP_WAIT_DURATION_MS);
        
        this->lastTemperature = this->temperature;
    }

    /**
     * Flag to indicate if trying to send temperature value
     */
    bool isTryToSendTemp()
    {
        return this->trySendTemp > 0;
    }

    /**
     * Flag to indicate if last send success
     */
    bool isSuccessSendingTemp()
    {
        return this->trySendTemp == 0;
    }

    /**
     * Send humidity to gateway
     */
    void sendHumidity()
    {
        this->trySendHum++;
        
        #ifdef MY_DEBUG
        String logMsg = "Send humidity (try ";
        logMsg.concat(this->trySendHum);
        logMsg.concat(")");
        this->sendLog(logMsg.c_str());
        delete &logMsg;
        #endif

        // Send
        MyMessage message(this->childHumID, V_HUM);
        message.set(this->humidity, 1);
        send(message, true);
        
        // Waiting for ACK confirmation
        wait(MY_SMART_SLEEP_WAIT_DURATION_MS);

        this->lastHumidity = this->humidity;
    }

    /**
     * Flag to indicate if trying to send temperature value
     */
    bool isTryToSendHum()
    {
        return this->trySendHum > 0;
    }

    /**
     * Flag to indicate if last send success
     */
    bool isSuccessSendingHum()
    {
        return this->trySendHum == 0;
    }

    /**
     * Set feature enable or not
     */
    void setEnable(bool enable)
    {
        this->enable = enable;
    }

    /**
     * Flag for enable feature
     */
    bool isEnable()
    {
        return this->enable;
    }

    /**
     * Set dhtSamplingPeriod
     */
    void setDHTSamplingPeriod(unsigned long duration)
    {
        this->dhtSamplingPeriod = duration;
    }

    /**
     * Get dhtSamplingPeriod
     */
    unsigned long getDHTSamplingPeriod()
    {
        return this->dhtSamplingPeriod;
    }

    /**
     * Get interval send
     */
    DataBuffer * getIntervalSend()
    {
        return this->intervalSend;
    }

    /**
     * Get interval send force
     */
    DataBuffer * getIntervalSendForce()
    {
        return this->intervalSendForce;
    }

protected:

    /**
     * Read probes value
     */
    bool readProbe()
    {
        // Try to read sensor
        if (!this->dht.measure()) {
            #ifdef MY_DEBUG
            this->sendLog(String("Failed read from AMT2320").c_str());
            int errorCode = this->dht.getErrorCode();
            switch (errorCode) {
                case 1: this->sendLog(String("E:Sensor offline").c_str()); break;
                case 2: this->sendLog(String("E:CRC valid failed").c_str()); break;
            }
            #endif

            return false;
        }
        // Success
        else {
            // Read from sensor
            this->temperature = this->dht.getTemperature();
            this->humidity = this->dht.getHumidity();

            #ifdef MY_DEBUG
            this->sendLog(String("T: " + String(this->temperature) + "°C").c_str());
            this->sendLog(String("H: " + String(this->humidity) + "%").c_str());
            #endif

            return true;
        }
    }

    /**
     * Business process
     */
    void process()
    {
        bool trigger = this->intervalSend->isOutdated();
        bool force = this->intervalSendForce->isOutdated();

        #ifdef MY_DEBUG
        String logMsg = F("Weather process");
        logMsg.concat(trigger || force ? F("go") : F("wait"));
        this->sendLog(logMsg.c_str());
        delete &logMsg;
        #endif
        
        if (( trigger || force ) && this->readProbe()) {
            unsigned int i;
            
            // Send temperature
            i = 0;
            if (force || 
                this->temperature != this->lastTemperature
            ) {
                do {
                    this->sendTemperature();
                } while (!this->isSuccessSendingTemp() && ++i < this->maxTry);
            }

            // Send humidity
            i = 0;
            if (force || 
                this->humidity != this->lastHumidity
            ) {
                do {
                    this->sendHumidity();
                } while (!this->isSuccessSendingHum() && ++i < this->maxTry);
            }

            // Reset intervals
            if (trigger) {
                this->intervalSend->reset();
            }
            if (force) {
                this->intervalSendForce->reset();
            }
        }
    }


    /**
     * Send log
     *
     * @param char * message Log message (max 25 bytes). To confirm: 10 char max
     */
    void sendLog(const char * message)
    {
      MyMessage msg;
      msg.sender = getNodeId();
      msg.destination = GATEWAY_ADDRESS;
      msg.sensor = NODE_SENSOR_ID;
      msg.type = I_LOG_MESSAGE;
      mSetCommand(msg, C_INTERNAL);
      mSetRequestEcho(msg, true);
      mSetEcho(msg, false);

      msg.set(message);

      _sendRoute(msg);
    }

    /*
     * Enable feature or not
     */
    bool enable = true;
    
    /**
     * Child temperature ID
     */
    unsigned int childTempID;

    /**
     * Child humidity ID
     */
    unsigned int childHumID;

    /**
     * Interval to send data to gateway if data changed
     */
    DataBuffer * intervalSend;

    /*
     * Interval to send data to gateway, even if data not changed
     */
    DataBuffer * intervalSendForce;

    /**
     * Temperature value
     */
    float temperature;

    /**
     * Humidity value
     */
    float humidity;

    /**
     * Last temperature value
     */
    float lastTemperature;

    /**
     * Last humidity value
     */
    float lastHumidity;

    /**
     * Number of attempt ton send temperature value (0 for success at the first try)
     */
    unsigned int trySendTemp = 0;

    /**
     * Number of attempt ton send humidity value (0 for success at the first try)
     */
    unsigned int trySendHum = 0;

    /**
     * Max try to send probe value
     */
    unsigned int maxTry = 3;

    /**
     * Weather probe
     */
    AM2320 dht;

    /**
     * Period for waiting DHT probe on init
     */
    unsigned long dhtSamplingPeriod = 2000;
};

#endif //COM_OSTERES_AUTOMATION_ARDUINO_COMPONENT_MYSENSOR_MYWEATHERAM2320_H
