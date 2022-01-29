
/** NimBLE_Server Demo:

    Demonstrates many of the available features of the NimBLE server library.

    Created: on March 22 2020
        Author: H2zero

*/

#include <NimBLEDevice.h>
#include <TFT_eSPI.h>        //LCD函式庫
#include "DHT.h"
#include "font.h"
#define DHTPIN 33     //這個腳位測試OK Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321


DHT dht(DHTPIN, DHTTYPE);
TFT_eSPI tft = TFT_eSPI();

String str1;
String str2;
String str3;

static NimBLEServer* pServer;
const int LED = 27;  // LED接腳
/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      Serial.println("GATT用戶端已連線");
      Serial.println("Multi-connect support: start advertising");
      NimBLEDevice::startAdvertising();
    };
    /** Alternative onConnect() method to extract details of the connection.
        See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
    */
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
      Serial.print("用戶端地址: ");
      Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
      /** We can use the connection handle here to ask for different connection parameters.
          Args: connection handle, min connection interval, max connection interval
          latency, supervision timeout.
          Units; Min/Max Intervals: 1.25 millisecond increments.
          Latency: number of intervals allowed to skip.
          Timeout: 10 millisecond increments, try for 5x interval time for best results.
      */
      pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
    };
    void onDisconnect(NimBLEServer* pServer) {
      Serial.println("用戶端離線 - 啟動廣播");
      NimBLEDevice::startAdvertising();
    };
    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) {
      Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
    };

    /********************* Security handled here **********************
    ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest() {
      Serial.println("Server Passkey Request");
      /** This should return a random 6 digit number for security
          or make your own static passkey as done here.
      */
      return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key) {
      Serial.print("The passkey YES/NO number: "); Serial.println(pass_key);
      /** Return false if passkeys don't match. */
      return true;
    };

    void onAuthenticationComplete(ble_gap_conn_desc* desc) {
      /** Check that encryption was successful, if not we disconnect the client */
      if (!desc->sec_state.encrypted) {
        NimBLEDevice::getServer()->disconnect(desc->conn_handle);
        Serial.println("Encrypt connection failed - disconnecting client");
        return;
      }
      Serial.println("Starting BLE work!");
    };
};

/** Handler class for characteristic actions */
class CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic) {
      Serial.println("用戶端 請求讀取資料！");

      Serial.print(pCharacteristic->getUUID().toString().c_str());    //特徵
      Serial.print(": onRead(), value: ");
      Serial.println(pCharacteristic->getValue().c_str());


    };

    void onWrite(NimBLECharacteristic* pCharacteristic) {
      Serial.println("用戶端 請求寫入資料！");

      Serial.print(pCharacteristic->getUUID().toString().c_str());    //特徵
      Serial.print(": onWrite(), value: ");
      Serial.println(pCharacteristic->getValue().c_str());


      std::string rxVal = pCharacteristic->getValue();
      if (rxVal == "on") {
        Serial.println("開燈！");
        digitalWrite(LED, HIGH);
      } else if (rxVal == "off") {
        Serial.println("關燈！");
        digitalWrite(LED, LOW);
      }


    };
    /** Called before notification or indication is sent,
        the value can be changed here before sending if desired.
    */
    void onNotify(NimBLECharacteristic* pCharacteristic) {
      Serial.println("Sending notification to clients");
    };


    /** The status returned in status is defined in NimBLECharacteristic.h.
        The value returned in code is the NimBLE host return code.
    */
    void onStatus(NimBLECharacteristic* pCharacteristic, Status status, int code) {
      String str = ("Notification/Indication status code: ");
      str += status;
      str += ", return code: ";
      str += code;
      str += ", ";
      str += NimBLEUtils::returnCodeToString(code);
      Serial.println(str);
    };

    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
      String str = "Client ID: ";
      str += desc->conn_handle;
      str += " Address: ";
      str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
      if (subValue == 0) {
        str += " Unsubscribed to ";
      } else if (subValue == 1) {
        str += " Subscribed to notfications for ";
      } else if (subValue == 2) {
        str += " Subscribed to indications for ";
      } else if (subValue == 3) {
        str += " Subscribed to notifications and indications for ";
      }
      str += std::string(pCharacteristic->getUUID()).c_str();

      Serial.println(str);
    };
};

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor) {
      std::string dscVal = pDescriptor->getValue();
      Serial.print("Descriptor witten value:");
      Serial.println(dscVal.c_str());
    };

    void onRead(NimBLEDescriptor* pDescriptor) {
      Serial.print(pDescriptor->getUUID().toString().c_str());
      Serial.println(" Descriptor read");
    };
};


/** Define callback instances globally to use for multiple Charateristics \ Descriptors */
static DescriptorCallbacks dscCallbacks;
static CharacteristicCallbacks chrCallbacks;


void setup() {
  Serial.begin(115200);
  Serial.println("Starting NimBLE Server");
  pinMode(LED, OUTPUT);
  /** sets device name */
  NimBLEDevice::init("NimBLE-Arduino");

  /** Optional: set the transmit power, default is 3db */
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

  /** Set the IO capabilities of the device, each option will trigger a different pairing method.
      BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
      BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
      BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
  */
  //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
  //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

  /** 2 different ways to set security - both calls achieve the same result.
      no bonding, no man in the middle protection, secure connections.

      These are the default values, only shown here for demonstration.
  */
  //NimBLEDevice::setSecurityAuth(false, false, true);
  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pDeadService = pServer->createService("0001");

  //0001服務 特徵 電燈
  NimBLECharacteristic* a1Characteristic = pDeadService->createCharacteristic(
        "00a1",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
        /** Require a secure connection for read and write access */
        //NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired / encrypted
        //NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired / encrypted
      );

  a1Characteristic->setValue("off");
  a1Characteristic->setCallbacks(&chrCallbacks);

  /** 2904 descriptors are a special case, when createDescriptor is called with
      0x2904 a NimBLE2904 class is created with the correct properties and sizes.
      However we must cast the returned reference to the correct type as the method
      only returns a pointer to the base NimBLEDescriptor class.
  */
  NimBLE2904* pBeef2904 = (NimBLE2904*)a1Characteristic->createDescriptor("2904");
  pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
  pBeef2904->setCallbacks(&dscCallbacks);

  //0001服務 特徵 濕度
  NimBLECharacteristic* a2Characteristic = pDeadService->createCharacteristic(
        "00a2",
        NIMBLE_PROPERTY::READ |
        //NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
        /** Require a secure connection for read and write access */
        //NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired / encrypted
        //NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired / encrypted
      );

  a2Characteristic->setValue("");
  a2Characteristic->setCallbacks(&chrCallbacks);

  //   BLEDescriptor *pDesc = new BLEDescriptor((uint16_t)0x2901);      //描述 可以不要
  //  pDesc->setValue("控制板內建LED的開關");
  //  pCharact_RX->addDescriptor(pDesc);

  //0001服務 特徵 溫度
  NimBLECharacteristic* a3Characteristic = pDeadService->createCharacteristic(
        "00a3",
        NIMBLE_PROPERTY::READ |
        // NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
        /** Require a secure connection for read and write access */
        //NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired / encrypted
        //NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired / encrypted
      );

  a3Characteristic->setValue("");
  a3Characteristic->setCallbacks(&chrCallbacks);

  //***************************************************//
  NimBLEService* pBaadService = pServer->createService("2001");
  NimBLECharacteristic* pFoodCharacteristic = pBaadService->createCharacteristic(
        "2002",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
      );

  pFoodCharacteristic->setValue("");
  pFoodCharacteristic->setCallbacks(&chrCallbacks);

  /** Note a 0x2902 descriptor MUST NOT be created as NimBLE will create one automatically
      if notification or indication properties are assigned to a characteristic.
  */

  /** Custom descriptor: Arguments are UUID, Properties, max length in bytes of the value */
  NimBLEDescriptor* pC01Ddsc = pFoodCharacteristic->createDescriptor(
                                 "C01D",
                                 NIMBLE_PROPERTY::READ |
                                 NIMBLE_PROPERTY::WRITE |
                                 NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                 20
                               );
  pC01Ddsc->setValue("Send it back!");
  pC01Ddsc->setCallbacks(&dscCallbacks);

  /** Start the services when finished creating all Characteristics and Descriptors */
  pDeadService->start();
  pBaadService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  /** Add the services to the advertisment data **/
  pAdvertising->addServiceUUID(pDeadService->getUUID());
  pAdvertising->addServiceUUID(pBaadService->getUUID());
  /** If your device is battery powered you may consider setting scan response
      to false as it will extend battery life at the expense of less data sent.
  */
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  tft.begin();               // 初始化LCD
  tft.setRotation(1);  // landscape
  tft.fillScreen(TFT_BLACK); // 用全黑清除螢幕
  tft.setSwapBytes(true);

  //TFT上顯示溫度
  //str1 = String(t, 2);
  tft.fillScreen(TFT_BLACK); // 用全黑清除螢幕
  tft.setTextColor(TFT_RED);
  tft.loadFont(font48); //指定tft屏幕对象载入font字库
  tft.drawString("溫度:", 0, 0);

  tft.setTextColor(TFT_RED);
  tft.drawString("濕度:", 0, 60);

  tft.unloadFont(); //释放字库文件,节省资源

  dht.begin();


  Serial.println("Advertising Started");
}


void loop() {

  //  // Wait a few seconds between measurements.
  //

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("DHT sensor讀取資料失敗!請檢查DHT傳感器??"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("濕度: "));
  Serial.print(h);
  Serial.print(F("%  溫度: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  熱指數: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  //
  //TFT上顯示溫度
  str1 = String(t, 2);
  str2 = String(h, 2);
  //str3 = "溫度:"+str1+",濕度:"+str2;
  str3 = "" + str2 + " ," + str1 + " ";
  //文字 T-Display
  tft.fillScreen(TFT_BLACK); // 用全黑清除螢幕
  tft.setTextColor(TFT_BLUE);

  tft.loadFont(font48); //指定tft屏幕对象载入font字库

  tft.setTextColor(TFT_RED);
  tft.drawString("溫度:", 0, 0);
  tft.setTextColor(TFT_BLUE);
  tft.drawString(str1, 110, 0);


  tft.setTextColor(TFT_RED);
  tft.drawString("濕度:", 0, 60);
  tft.setTextColor(TFT_BLUE);
  tft.drawString(str2, 110, 60);



  tft.unloadFont(); //释放字库文件,节省资源

  //
  //
  //
  if (pServer->getConnectedCount()) {
    NimBLEService* pSvc = pServer->getServiceByUUID("0001");
    if (pSvc) {
      NimBLECharacteristic* a1Chr = pSvc->getCharacteristic("00a1");
      if (a1Chr) {
        //a1Chr->setValue(str3);
        a1Chr->notify(true);

        Serial.println("a1 notify送出");
      }

      NimBLECharacteristic* a2Chr = pSvc->getCharacteristic("00a2");
      if (a2Chr) {
        a2Chr->setValue(str2);
        a2Chr->notify(true);

        Serial.println("a2 notify送出");
      }

      NimBLECharacteristic* a3Chr = pSvc->getCharacteristic("00a3");
      if (a3Chr) {

        str3 = a1Chr->getValue().c_str();
        a3Chr->setValue(str1 + "," + str2 + "," + str3);
        a3Chr->notify(true);

        Serial.println("a3 notify送出");


        //Serial.println(str3);
      }
    }
  }



  delay(2000);
}
