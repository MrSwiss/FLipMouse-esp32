/*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
* 
*   http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
* 
* Copyright Neil Kolban
 */

/** @file
 * @brief This file is the implementation for Neil Kolbans CPP utils
 * 
 * It initializes 3 queues for sending mouse, keyboard and joystick data
 * from within the C-side. C++ classes are instantiated here.
 * If you want to have a different BLE HID device, you need to adapt this file.
 * 
 * @note Thank you very much Neil Kolban for this impressive work!
*/

#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "SampleKeyboardTypes.h"
#include <esp_log.h>
#include <string>
#include <Task.h>
#include "hal_ble.h"

#include "sdkconfig.h"

#define LOG_LEVEL_BLE ESP_LOG_DEBUG

static char LOG_TAG[] = "halBLE";

/// @brief Is the BLE currently connected?
uint8_t isConnected = 0;

///@brief Is Keyboard interface active?
uint8_t activateKeyboard = 0;
///@brief Is Mouse interface active?
uint8_t activateMouse = 0;
///@brief Is Joystick interface active?
uint8_t activateJoystick = 0;

//static BLEHIDDevice class instance for communication (sending reports)
static BLEHIDDevice* hid;
//BLE server handle
BLEServer *pServer;
//characteristic for sending keyboard reports to the host
BLECharacteristic* inputKbd;
//characteristic for sending mouse reports to the host
BLECharacteristic* inputMouse;
//characteristic for sending mouse reports to the host
BLECharacteristic* inputJoystick;
//characteristic for receiving keyboard reports from the host (status LEDs)
BLECharacteristic* outputKbd;

/** @brief Constant report map for keyboard
 * 
 * This report map will be used on init do build a report map according
 * to init functions (with activated interfaces).
 * 
 * @note Report id is on all reports in offset 7.
 * */
const uint8_t reportMapKeyboard[] = {
  USAGE_PAGE(1),      0x01,       // Generic Desktop Ctrls
  USAGE(1),           0x06,       // Keyboard
  COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,
    //report equal to usb_bridge (https://github.com/benjaminaigner/usb_bridge
    REPORT_SIZE(1),     0x01,
    REPORT_COUNT(1),    0x08,
    USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
    USAGE_MINIMUM(1),   0xE0,
    USAGE_MAXIMUM(1),   0xE7,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    INPUT(1),           0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x01,
    REPORT_SIZE(1),     0x08,
    INPUT(1),           0x03,
    REPORT_COUNT(1),    0x05,
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    OUTPUT(1),          0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    OUTPUT(1),          0x03,
    REPORT_COUNT(1),    0x06,       //   6 bytes (Keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 104,       //   104 keys
    USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
    USAGE_MINIMUM(1),   0x00,       //   Num Lock
    USAGE_MAXIMUM(1),   104,       //   Kana
    INPUT(1),           0x00,
  END_COLLECTION(0)
};

/** @brief Constant report map for mouse
 * 
 * This report map will be used on init do build a report map according
 * to init functions (with activated interfaces).
 * 
 * @note Report id is on all reports in offset 7.
 * */
const uint8_t reportMapMouse[] = {
  USAGE_PAGE(1), 			0x01,
  USAGE(1), 				  0x02,
  COLLECTION(1),			0x01,
    REPORT_ID(1),       0x02,
    USAGE(1),				    0x01,
    COLLECTION(1),			0x00,
      USAGE_PAGE(1),			0x09,
      USAGE_MINIMUM(1),		0x1,
      USAGE_MAXIMUM(1),		0x3,
      LOGICAL_MINIMUM(1),	0x0,
      LOGICAL_MAXIMUM(1),	0x1,
      REPORT_COUNT(1),		0x3,
      REPORT_SIZE(1),	  	0x1,
      INPUT(1), 				  0x2,		// (Data, Variable, Absolute), ;8 button bits
      REPORT_COUNT(1),		0x1,
      REPORT_SIZE(1),		  0x5,
      INPUT(1), 				  0x1,		//(Constant), ;5 bit padding
      USAGE_PAGE(1), 		  0x1,		//(Generic Desktop),
      USAGE(1),				    0x30,   //X
      USAGE(1),				    0x31,   //Y
      USAGE(1),				    0x38,   //wheel
      LOGICAL_MINIMUM(1),	0x81,
      LOGICAL_MAXIMUM(1),	0x7f,
      REPORT_SIZE(1),	  	0x8,
      REPORT_COUNT(1),		0x03,   //3 bytes: X/Y/wheel
      INPUT(1), 				  0x6,		//(Data, Variable, Relative), ;3 position bytes (X & Y & wheel)
    END_COLLECTION(0),
  END_COLLECTION(0)
};


/** @brief Constant report map for joystick
 * 
 * This report map will be used on init do build a report map according
 * to init functions (with activated interfaces).
 * 
 * @note Report id is on all reports in offset 7.
 * */
const uint8_t reportMapJoystick[] = {
  USAGE_PAGE(1), 			0x01,
  USAGE(1), 				  0x04,
  COLLECTION(1),			0x01,
    REPORT_ID(1),       0x03,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1),		0x20, /* 32 */
    REPORT_SIZE(1),	  	0x01,
    USAGE_PAGE(1),      0x09,
    USAGE_MINIMUM(1),   0x01,
    USAGE_MAXIMUM(1),   0x20, /* 32 */
    INPUT(1),           0x02, // variable | absolute
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x07,
    PHYSICAL_MINIMUM(1),0x01,
    PHYSICAL_MAXIMUM(2),(315 & 0xFF), ((315>>8) & 0xFF),
    REPORT_SIZE(1),	  	0x04,
    REPORT_COUNT(1),	  0x01,
    UNIT(1),            20,
    USAGE_PAGE(1), 			0x01,
    USAGE(1), 				  0x39,  //hat switch
    INPUT(1),           0x42, //variable | absolute | null state
    USAGE_PAGE(1), 			0x01,
    USAGE(1), 				  0x01,  //generic pointer
    COLLECTION(1),      0x00,
      LOGICAL_MINIMUM(1), 0x00,
      //LOGICAL_MAXIMUM(4), (1023 & 0xFF), ((1023>>8) & 0xFF), ((1023>>16) & 0xFF), ((1023>>24) & 0xFF),
      LOGICAL_MAXIMUM(2), (1023 & 0xFF), ((1023>>8) & 0xFF),
      REPORT_COUNT(1),		0x04,
      REPORT_SIZE(1),	  	0x0A, /* 10 */
      USAGE(1), 				  0x30,  // X axis
      USAGE(1), 				  0x31,  // Y axis
      USAGE(1), 				  0x32,  // Z axis
      USAGE(1), 				  0x35,  // Z-rotator axis
      INPUT(1),           0x02, // variable | absolute
    END_COLLECTION(0),
    LOGICAL_MINIMUM(1), 0x00,
    //LOGICAL_MAXIMUM(4), (1023 & 0xFF), ((1023>>8) & 0xFF), ((1023>>16) & 0xFF), ((1023>>24) & 0xFF),
    LOGICAL_MAXIMUM(2), (1023 & 0xFF), ((1023>>8) & 0xFF),
    REPORT_COUNT(1),		0x02,
    REPORT_SIZE(1),	  	0x0A, /* 10 */
    USAGE(1), 				  0x36,  // generic slider
    USAGE(1), 				  0x36,  // generic slider
    INPUT(1),           0x02, // variable | absolute
  END_COLLECTION(0)
};

class kbdOutputCB : public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic* me){
		uint8_t* value = (uint8_t*)(me->getValue().c_str());
		ESP_LOGW(LOG_TAG, "Not implemented: special keys: %d", *value);
	}
};

class BLETask : public Task {  
	void run(void*){
    hid_command_t rx;
  
    //Empty queue if initialized (there might be something left from last connection)
    if(hid_ble != nullptr) xQueueReset(hid_ble);
    
    //check if queue is initialized
    if(hid_ble != NULL)
    {
      while(1)
      {
        //pend on MQ, if timeout triggers, just wait again.
        if(xQueueReceive(hid_ble,&rx,portMAX_DELAY))
        {
          //debug output
          #if LOG_LEVEL_BLE >= ESP_LOG_DEBUG
          switch(rx.data[0])
          {
            case 'M': ESP_LOGD(LOG_TAG,"BLE Mouse: B: %d, X/Y: %d/%d, wheel: %d", \
              rx.data[1],(int8_t)rx.data[2],(int8_t)rx.data[3],(int8_t)rx.data[4]);
              break;
            case 'K': ESP_LOGD(LOG_TAG,"BLE Kbd: Mod: %d, keys: %d/%d/%d/%d/%d/%d", \
              rx.data[1],rx.data[2],rx.data[3],rx.data[4],rx.data[5],rx.data[6],rx.data[7]);
              break;
            case 'J': ESP_LOGD(LOG_TAG,"BLE joystick:");
              ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,&rx.data[1], 12 ,ESP_LOG_DEBUG);
              break;
            default: break;
          }
          #endif
          
          //test if it is a supported report
          if(rx.data[0] != 'M' && rx.data[0] != 'K' && rx.data[0] != 'J') {
            ESP_LOGE(LOG_TAG,"Unknown USB report");
            continue;
          }
          
          //according to interface, notify via characteristics
          switch(rx.data[0])
          {
            case 'M':
              if(activateMouse)
              {
                inputMouse->setValue(&rx.data[1],4);
                inputMouse->notify();
              } else {
                ESP_LOGW(LOG_TAG,"Mouse disabled, but report received");
              }
            break;
            case 'K':
              if(activateKeyboard)
              {
                uint8_t a[] = {rx.data[1],0,rx.data[2],rx.data[3],rx.data[4], \
                  rx.data[5],rx.data[6],rx.data[7] };
                inputKbd->setValue(a,sizeof(a));
                inputKbd->notify();
              } else {
                ESP_LOGW(LOG_TAG,"Kbd disabled, but report received");
              }
            break;
            case 'J':
              if(activateJoystick)
              {
                inputJoystick->setValue(&rx.data[1],12);
                inputJoystick->notify();
              } else {
                ESP_LOGW(LOG_TAG,"Joystick disabled, but report received");
              }
            break;
          }
        }
      }
    } else {
      ESP_LOGE(LOG_TAG,"ble hid queue not initialized, retry in 1s");
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
	}
};
BLETask *bletask; //instance for this task

class CBs: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer){
    BLE2902* desc;
    
    isConnected = 1;
    
    //enable notifications for input service, suggested by chegewara to support iOS/Win10
    //https://github.com/asterics/esp32_mouse_keyboard/issues/4#issuecomment-386558158
    if(activateKeyboard)
    {
      desc = (BLE2902*) inputKbd->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(true);
    }
    
    if(activateMouse)
    {
      desc = (BLE2902*) inputMouse->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(true);
    }

    if(activateJoystick)
    {
      desc = (BLE2902*) inputJoystick->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(true);
    }
    bletask->start();
  }

  void onDisconnect(BLEServer* pServer){
    BLE2902* desc;
    
    isConnected = 0;
    
    //disable notifications for input service, suggested by chegewara to
    //reduce power & memory usage
    //https://github.com/asterics/esp32_mouse_keyboard/commit/a1796ce91155ec7db62af4a53dbdef32bc4adf08#commitcomment-28888676
    if(activateKeyboard)
    {
      desc = (BLE2902*) inputKbd->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(false);
    }
    
    if(activateMouse)
    {
      desc = (BLE2902*) inputMouse->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(false);
    }
    
    if(activateJoystick)
    {
      desc = (BLE2902*) inputJoystick->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(false);
    }
    //stop HID receiving task
    bletask->stop();
    
    //restart advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
    ESP_LOGI(LOG_TAG,"Client disconnected, restarting advertising");
  }
};

uint32_t passKey = 1307;
/** @brief security callback
 * 
 * This class is passed to the BLEServer as callbacks for security
 * related actions. Depending on IO_CAP configuration & host, different
 * types of security actions are required for bonding this device to a
 * host. */
class CB_Security: public BLESecurityCallbacks {
  
  // Request a pass key to be typed in on the host
  uint32_t onPassKeyRequest(){
    ESP_LOGE(LOG_TAG, "The passkey request %d", passKey);
    vTaskDelay(25000);
    return passKey;
  }
  
  // The host sends a pass key to the ESP32 which needs to be displayed
  //and typed into the host PC
  void onPassKeyNotify(uint32_t pass_key){
    ESP_LOGE(LOG_TAG, "The passkey Notify number:%d", pass_key);
    passKey = pass_key;
  }
  
  // CB for testing if a host is allowed to connect, in our case always yes.
  bool onSecurityRequest(){
    return true;
  }

  // CB on a completed authentication (not depending on status)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl){
    if(auth_cmpl.success){
      ESP_LOGI(LOG_TAG, "remote BD_ADDR:");
      esp_log_buffer_hex(LOG_TAG, auth_cmpl.bd_addr, sizeof(auth_cmpl.bd_addr));
      ESP_LOGI(LOG_TAG, "address type = %d", auth_cmpl.addr_type);
    }
      ESP_LOGI(LOG_TAG, "pair status = %s", auth_cmpl.success ? "success" : "fail");
  }
  
  // You need to confirm the given pin
  bool onConfirmPIN(uint32_t pin)
  {
    ESP_LOGE(LOG_TAG, "Confirm pin: %d", pin);
    return true;
  }
  
};

/** @brief Main BLE HID-over-GATT task
 * 
 * This task is used to initialize 3 tasks for keyboard, mouse and joystick
 * as well as intializing the BLE device:
 * * Init device
 * * Create a new server
 * * Attach a HID over GATT implementation to the server
 * * Create the input & output reports for the different HID devices
 * * Create & add the HID report map
 * * Finally start the server & services and start advertising
 * */
class BLE_HOG: public Task {
	void run(void *data) {
		ESP_LOGD(LOG_TAG, "Initialising BLE HID device.");

    /*
     * Create new task instances, if necessary
     */
    #ifdef DEVICE_FLIPMOUSE
      BLEDevice::init("FLipMouse");
    #endif
    #ifdef DEVICE_FABI
      BLEDevice::init("FABI");
    #endif
		pServer = BLEDevice::createServer();
		pServer->setCallbacks(new CBs());
		//BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
		BLEDevice::setSecurityCallbacks(new CB_Security());
    
    bletask = new BLETask();
    bletask->setStackSize(8096);
    
		/*
		 * Instantiate hid device
		 */
		hid = new BLEHIDDevice(pServer);
    
		/*
		 * Set manufacturer name
		 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.manufacturer_name_string.xml
		 */
		std::string name = "AsTeRICS Foundation";
		hid->manufacturer()->setValue(name);

		/*
		 * Set pnp parameters
		 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.pnp_id.xml
		 */
    //hid->pnp(0x01,0xE502,0xA111,0x0210); //BT SIG assigned VID
    hid->pnp(0x02,0xE502,0xA111,0x0210); //USB assigned VID

		/*
		 * Set hid informations
		 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.hid_information.xml
		 */
		//const uint8_t val1[] = {0x01,0x11,0x00,0x03};
		//hid->hidInfo()->setValue((uint8_t*)val1, 4);
    hid->hidInfo(0x00,0x01);

    /*
     * Build a report map, depending on init function.
     * For each enabled interface (keyboard, mouse, joystick) the
     * corresponding report map is copied to a new map which is 
     * used for initializing the BLEHID class.
     * Report IDs are changed accordingly.
     */
    size_t reportMapSize = 0;
    if(activateKeyboard) reportMapSize += sizeof(reportMapKeyboard);
    if(activateMouse) reportMapSize += sizeof(reportMapMouse);
    if(activateJoystick) reportMapSize += sizeof(reportMapJoystick);
    
    uint8_t *reportMap = (uint8_t *)malloc(reportMapSize);
    uint8_t *reportMapCurrent = reportMap;
    uint8_t reportID = 1;
    
    if(reportMap == nullptr)
    {
      ESP_LOGE(LOG_TAG,"Cannot allocate memory for the report map, cannot start HID");
    } else {
      //copy report map for keyboard to allocated full report map, if activated
      if(activateKeyboard)
      {
        //create report map for keyboard
        memcpy(reportMapCurrent,reportMapKeyboard,sizeof(reportMapKeyboard));
        reportMap[7] = reportID;
        reportMapCurrent += sizeof(reportMapKeyboard);
        
        //create in&out characteristics/reports for keyboard
        inputKbd = hid->inputReport(reportID);
        outputKbd = hid->outputReport(reportID);
        outputKbd->setCallbacks(new kbdOutputCB());
        
        ESP_LOGD(LOG_TAG,"Keyboard added @report ID %d, current report Map:", reportID);
        ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,reportMap,(uint16_t)(reportMapCurrent-reportMap),ESP_LOG_DEBUG);
        
        reportID++; //increase report id for next interface
      }
      //copy report map for joystick to allocated full report map, if activated
      if(activateJoystick)
      {
        memcpy(reportMapCurrent,reportMapJoystick,sizeof(reportMapJoystick));
        reportMapCurrent[7] = reportID;
        reportMapCurrent += sizeof(reportMapJoystick);
        
        //create in characteristics/reports for joystick
        inputJoystick = hid->inputReport(reportID);
        
        ESP_LOGD(LOG_TAG,"Joystick added @report ID %d, current report Map:", reportID);
        ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,reportMap,(uint16_t)(reportMapCurrent-reportMap),ESP_LOG_DEBUG);
        
        reportID++; //increase report id for next interface
      }
      //copy report map for mouse to allocated full report map, if activated
      if(activateMouse)
      {
        memcpy(reportMapCurrent,reportMapMouse,sizeof(reportMapMouse));
        reportMapCurrent[7] = reportID;
        reportMapCurrent += sizeof(reportMapMouse);
        
        //create in characteristics/reports for mouse
        inputMouse = hid->inputReport(reportID);
        
        ESP_LOGD(LOG_TAG,"Mouse added @report ID %d, current report Map:", reportID);
        ESP_LOG_BUFFER_HEXDUMP(LOG_TAG,reportMap,(uint16_t)(reportMapCurrent-reportMap),ESP_LOG_DEBUG);
        
        reportID++; //increase report id for next interface
      }
      
      ESP_LOGI(LOG_TAG,"Final HID report map size: %d B",reportMapSize);
          
      /*
       * Set report map (here is initialized device driver on client side)
       * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.report_map.xml
       */
      hid->reportMap((uint8_t*)reportMap, reportMapSize);
  
      /*
       * We are prepared to start hid device services. Before this point we can change all values and/or set parameters we need.
       * Also before we start, if we want to provide battery info, we need to prepare battery service.
       * We can setup characteristics authorization
       */
      hid->startServices();
    }

		/*
		 * Its good to setup advertising by providing appearance and advertised service. This will let clients find our device by type
		 */
		BLEAdvertising *pAdvertising = pServer->getAdvertising();
		//pAdvertising->setAppearance(HID_KEYBOARD);
		pAdvertising->setAppearance(GENERIC_HID);
    pAdvertising->setMinInterval(400); //250ms minimum
    pAdvertising->setMaxInterval(800); //500ms maximum
		pAdvertising->addServiceUUID(hid->hidService()->getUUID());
		pAdvertising->start();
    


		BLESecurity *pSecurity = new BLESecurity();
		pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
		pSecurity->setCapability(ESP_IO_CAP_NONE);

		ESP_LOGI(LOG_TAG, "Advertising started!");
    vTaskDelete(NULL);
	}
};


extern "C" {
  /** @brief Activate/deactivate pairing mode
   * @param enable If set to != 0, pairing will be enabled. Disabled if == 0
   * @return ESP_OK on success, ESP_FAIL otherwise*/
  esp_err_t halBLESetPairing(uint8_t enable)
  {
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
		//pAdvertising->setAppearance(HID_KEYBOARD);
		pAdvertising->setAppearance(GENERIC_HID);
		pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    if(enable != 0)	pAdvertising->start();
    else pAdvertising->stop();
    return ESP_OK;
  }
  
  ///@todo Add interface to disable BLE & free memory! (see long button handler in main)
  
  /** @brief Get connection status
   * @return 0 if not connected, != 0 if connected */
  uint8_t halBLEIsConnected(void)
  {
    return isConnected;
  }
  
  /** @brief Main init function to start HID interface (C interface)
   * @see hid_ble */
  esp_err_t halBLEInit(uint8_t enableKeyboard, uint8_t enableMouse, uint8_t enableJoystick)
  {
    //save enabled interfaces
    activateMouse = enableMouse;
    activateKeyboard = enableKeyboard;
    activateJoystick = enableJoystick;
    
    //init Neil Kolban's HOG task
    BLE_HOG* blehid = new BLE_HOG();
    blehid->setStackSize(8096);
    blehid->start();
    return ESP_OK;
  }
  
  /** @brief En- or Disable BLE interface.
   * 
   * This method is used to enable or disable the BLE interface. Currently, the ESP32
   * cannot use WiFi and BLE simultaneously. Therefore, when enabling wifi, it is
   * necessary to disable BLE prior calling taskWebGUIEnDisable.
   * 
   * @note Calling this method prior to initializing BLE via halBLEInit will
   * result in an error!
   * @return ESP_OK on success, ESP_FAIL otherwise
   * @param onoff If != 0, switch on BLE, switch off if 0.
   * */
  esp_err_t halBLEEnDisable(int onoff)
  {
    if(onoff)
    {
      ESP_LOGE(LOG_TAG,"Re-enabling BLE is TBD");
    } else {
      //disable
      BLEDevice::deinit();
      ESP_LOGI(LOG_TAG,"Disable BLE device");
    }
    return ESP_OK;
  }
  
  
  /** @brief Reset the BLE data
   * 
   * Used for slot/config switchers.
   * It resets the keycode array and sets all HID reports to 0
   * (release all keys, avoiding sticky keys on a config change) 
   * @param exceptDevice if you want to reset only a part of the devices, set flags
   * accordingly:
   * 
   * (1<<0) excepts keyboard
   * (1<<1) excepts joystick
   * (1<<2) excepts mouse
   * If nothing is set (exceptDevice = 0) all are reset
   * */
  void halBLEReset(uint8_t exceptDevice)
  {
    if(isConnected)
    {
      if(activateMouse && !(exceptDevice & (1<<2)))
      {
        uint8_t a[] = {0,0,0,0};
        inputMouse->setValue(a,sizeof(a));
        inputMouse->notify();
      }
      if(activateKeyboard && !(exceptDevice & (1<<0)))
      {
        uint8_t a[] = {0,0,0,0,0,0,0,0};
        inputKbd->setValue(a,sizeof(a));
        inputKbd->notify();
      }
      if(activateJoystick && !(exceptDevice & (1<<1)))
      {
        uint8_t a[] = {0,0,0,0,0,0,0,0,0,0,0,0};
        inputJoystick->setValue(a,sizeof(a));
        inputJoystick->notify();
      }
    } else {
      ESP_LOGW(LOG_TAG,"Not connected, cannot reset");
    }
  }
  
}
