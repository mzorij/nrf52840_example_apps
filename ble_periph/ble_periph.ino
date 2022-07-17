#if defined(USE_TINYUSB)
#include <Adafruit_TinyUSB.h> // for Serial
#endif

#include <Arduino.h>

#include <bluefruit.h>
#include <utility/bonding.h>

// TODO: can't figure out how to include this directly #include <BLEAdafruitSensor.h>

// https://www.bluetooth.com/specifications/assigned-numbers/
BLEService alert_service = BLEService(UUID16_SVC_IMMEDIATE_ALERT);

uint8_t first_conn = 0;

//BLEUart bleuart; // uart over ble
// TODO: examine BLEAdafruitSensor.h, refer to BLEAdafruitButton
BLEDis  bledis;  // device information

void setup() 
{
  // put your setup code here, to run once:

  Serial.begin(115200);

  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Bluefruit.begin();

  bond_init();
  bond_print_list(BLE_GAP_ROLE_PERIPH);
#if 0
  //Serial.println("Initializing bond");
  // Note: this creates the bond directories in the file system
  //bond_init();
  // Note: this throws an assertion if the list is empty it seems
  //Serial.println("Printing current bond list");
  bond_print_list(BLE_GAP_ROLE_PERIPH);

  Serial.println("clearing bond list");
  Bluefruit.Periph.clearBonds();
  bond_print_list(BLE_GAP_ROLE_PERIPH);
#endif

  // HID Device can have a min connection interval of 9*1.25 = 11.25 ms
  Bluefruit.Periph.setConnInterval(9, 16); // min = 9*1.25=11.25 ms, max = 16*1.25=20ms
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
    
  // prevent man-in-the-middle attacks
  Bluefruit.Security.setMITM( true );

  // Set connection secured callback, invoked when connection is encrypted
  Bluefruit.Security.setSecuredCallback(connection_secured_callback);

  // set IO capabilities...will be we get a security prompt?
  Bluefruit.Security.setIOCaps( true /*display*/ , true /*yes_no*/, true/*keyboard*/ );

  Bluefruit.Security.setPairPasskeyCallback( pairPasskeyCb );
  //Bluefruit.Security.setPairPasskeyRequestCallback( pairPasskeyRequestCb );
  Bluefruit.Security.setPairCompleteCallback( pairCompleteCb );

  // TODO: do I need this?
  Bluefruit.Security.begin();

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.begin();

  // Note: this is displayed in the "device information" service
  // Configure and Start Device Information Service
  bledis.setManufacturer("Z Industries");
  bledis.setModel("crazy_mofo");
  bledis.begin();

  // must call begin on any service before beginning any characteristic
  // Note: refer to custom_hrm
  alert_service.begin();

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{
  Serial.println("Starting adveristing");
  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  
  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(alert_service);
  
  // Include CTS client UUID
  //Bluefruit.Advertising.addService(bleCTime);

  // Includes name
  Bluefruit.Advertising.addName();

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}


void connect_callback(uint16_t conn_handle)
{
  BLEConnection* p_conn = Bluefruit.Connection(conn_handle);
  char buf[100];
  
  Serial.println("Connected");

  // Display some basic info on the connection
  Serial.print("role is ");
  Serial.println( p_conn->getRole() );

  Serial.print("peer name is ");
  p_conn->getPeerName( buf, 100 );
  Serial.println( buf );

  // saveCccd() - TODO? CCCD: client configuration descriptor 

  // NOTE: this seems to work to request a connection that isn't bonded
  // request Pairing if not bonded
  if( p_conn->bonded() == false/*p_conn->secured()*/ )
  {
    //Serial.println("Attempting to PAIR with the device, please press PAIR on your phone ... ");
    //p_conn->requestPairing();
#if 1
    // connection must be secured otherwise disconnect it
    Serial.println("Connect is NOT secure, disconnecting");
    for( int i=0; i<10; i++ )
    {
      delay( 500 );
      digitalToggle(LED_RED);  
    }
    if( p_conn->bonded() == false )
    {
      p_conn->disconnect();
      digitalWrite( LED_RED, 0 );
    }
    else
    {
      digitalWrite( LED_RED, 1 );
    }
#else
    p_conn->requestPairing();
#endif    
  }
  else
  {
    // it's bonded, make sure it's secure
    if( p_conn->secured() )
    {
      Serial.println("Connection already secured");
      digitalWrite( LED_RED, 1 );
    }
  }
  /*
  Serial.print("Discovering CTS ... ");
  if ( bleCTime.discover(conn_handle) )
  {
    Serial.println("Discovered");
    
    // Current Time Service requires pairing to work
    // request Pairing if not bonded
    Serial.println("Attempting to PAIR with the iOS device, please press PAIR on your phone ... ");
    conn->requestPairing();
  }*/
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);

  digitalWrite( LED_RED, 0 );
}

void connection_secured_callback(uint16_t conn_handle)
{
  BLEConnection* p_conn = Bluefruit.Connection(conn_handle);

  if ( !p_conn->secured() )
  {
    Serial.println("In secure CB but not yet secured");
    // It is possible that connection is still not secured by this time.
    // This happens (central only) when we try to encrypt connection using stored bond keys
    // but peer reject it (probably it remove its stored key).
    // Therefore we will request an pairing again --> callback again when encrypted
    p_conn->requestPairing();
  }
  else
  {
    Serial.println("Secured");

  }
}

bool pairPasskeyCb(uint16_t conn_hdl, uint8_t const passkey[6], bool match_request)
{
  Serial.print("PairPasskeyCb ");  
  Serial.println( match_request );
  for( int i=0; i<6; i++ )
  {
    Serial.print( passkey[i] );
  }
  Serial.println( "cb done");
  
  return (true);
}
void pairPasskeyRequestCb(uint16_t conn_hdl, uint8_t passkey[6])
{
  Serial.println( "pairPasskeyRequestCb" );

  for( int i=0; i<6; i++ )
  {
    Serial.print( passkey[i] );
  }
}

void pairCompleteCb(uint16_t conn_hdl, uint8_t auth_status)
{
  Serial.print("Pair complete");
  Serial.println(auth_status);
}

void loop() 
{
  //digitalToggle(LED_RED);
  
  // put your main code here, to run repeatedly:
  delay( 1000 );
  if ( Bluefruit.connected() ) 
  {
    if( first_conn == 0 )
    {
      Serial.println("BLE connected");
      first_conn = 1;
    }
  }
  else
  {
    //Serial.println("BLE NOT connected");
  }
}
