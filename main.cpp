/*
//Erittäin toimiva koodi, als-anturit säätävät moottoria vain jos skulma (pot.arvo) on tietyllä alueella

 * Copyright (c) 2006-2020 Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 ***********************************
 *UDP send receive example
 * A microcontroller board and MOD WIFI ESP8266
 * https://os.mbed.com/docs/mbed-os/v6.15/apis/connectivity.html
 * https://os.mbed.com/docs/mbed-os/v6.15/apis/ethernet.html
 * https://os.mbed.com/docs/mbed-os/v6.15/apis/wi-fi.html
 * https://os.mbed.com/teams/ESP8266/code/esp8266-driver/
 * https://www.olimex.com/Products/IoT/ESP8266/MOD-WIFI-ESP8266/open-source-hardware
 * https://os.mbed.com/docs/mbed-os/v6.15/apis/udpsocket.html
 *
 * L432KC --- MOD WIFI ESP8266 from OLIMEX
 * L432KC D5=PB6=UART1TX --- 3 RXD
 * L432KC D4=PB7=UART1RX --- 4 TXD
 * or
 * L432KC D1=PA9=UART1TX --- 3 RXD
 * L432KC D0=PA10=UART1RX --- 4 TXD
 * L432KC 3V3 --- 1 3.3V
 * L432KC GND --- 2 GND
 *
 * UDP Send Receive App on smart phone needed for testing
 * https://play.google.com/store/search?q=UDP%20Sender%20Receiver
 *  or Windows computer with an UDP client app
 * Timo Karppinen 12.12.2021  Apache-2.0
 ***********************************/

#include "mbed.h"

// MOD WIFI ESP8266
#include "ESP8266Interface.h"   // included in the OS6

#include <MQTTClientMbedOs.h>
#include <cstdio>
#define REMOTE_PORT 5000
#define LOCAL_PORT 5001
#define BUFF_SIZE 128


//Valistuksen säätöön oma silmukka
//potentiiometrille oma silmukka


// Network interface 

    

//Threads
Thread recv_thread;
Thread send_thread; 

Thread moottoriAuki;
Thread potenttiArvo;

//Functions
void udpReceive( void );
void udpSend( void );

void ValonVertailu();
void DatanKerays();
void Auki();
void PotentiometrinSaato();
void MQTTJulkaisu();
//skulma määritetty 0.0-2.0 välille
float sKulma;
float tilaus;

// WLAN security
const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

void scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\r\n");

    int count = wifi->scan(NULL, 0);

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];

    count = wifi->scan(ap, count);
    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\r\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\r\n", count);

    delete[] ap;
}

//Fields
char in_data[BUFF_SIZE];
char out_data1[BUFF_SIZE] = "sensordata";
char buffer[BUFF_SIZE];


//SPI communication interfaces

//ALS valo anturi
SPI ALS_spi(D11,D12, D13); //mosi miso clck  + 2 muu CS pinnit

DigitalOut sisaCS(A1);
DigitalOut ulkoCS(A3);

int sisaSensorData;
int ulkoSensorData;



DigitalOut moottorinSuunta(D10);
//DigitalOut usrNappi(D8, PullNone);
PwmOut moottorinTeho(D3);

//WiFi UDP
SocketAddress clientUDP;  // Client on remote device
UDPSocket serverUDP;   // UDP server in this device
//Store device IP
SocketAddress deviceIP;
//Store broker IP
SocketAddress MQTTBroker;

TCPSocket socket;
MQTTClient client(&socket);

int main() {
    ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);



// Setting up WLAN
 
    printf("WiFi example\r\n\r\n");
     ThisThread::sleep_for(500ms); // waiting for the ESP8266 to wake up. 
    
    scan_demo(&esp);

    printf("\r\nConnecting...\r\n");
    int ret = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);

    if(ret != 0)
    {
        printf("\nConnection error\n");
    }
    else
    {
        printf("\nConnection success\n");
    }

     esp.get_ip_address(&deviceIP);
    printf("IP via DHCP: %s\n", deviceIP.get_ip_address());

    // Tästä alkaa aiempi koodi
    esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");

    MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    
    //data.clientID.cstring = MBED_CONF_APP_MQTT_CLIENT_ID;
    //data.username.cstring = MBED_CONF_APP_MQTT_AUTH_METHOD;
    //data.password.cstring = MBED_CONF_APP_MQTT_AUTH_TOKEN;
    char *id = MBED_CONF_APP_MQTT_ID;
    data.clientID.cstring = id;

    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)buffer;
    msg.payloadlen = strlen(buffer);
    ThisThread::sleep_for(5s);

    // Connecting mqtt broker
    printf("Connecting %s ...\n", MBED_CONF_APP_MQTT_BROKER_HOSTNAME);
    socket.open(&esp);
    socket.connect(MQTTBroker);
    client.connect(data);
    

    // Show network address
    SocketAddress espAddress;
    printf("Success\n\n");
    printf("MAC: %s\n", esp.get_mac_address());
    esp.get_ip_address(&espAddress);
    printf("IP: %s\n", espAddress.get_ip_address());
    printf("Netmask: %s\n", esp.get_netmask());
    printf("Gateway: %s\n", esp.get_gateway());
    printf("RSSI: %d\n\n", esp.get_rssi());
    
    ThisThread::sleep_for(50ms); 
    
    serverUDP.open(&esp);
    int err = serverUDP.bind(LOCAL_PORT);
    printf("Port status is: %d\n",err);
    
    //recv_thread.start(udpReceive);
    printf("Listening has been started at port number %d\n", LOCAL_PORT);
    
    //send_thread.start(udpSend);
    printf("Sending out demo data to port number %d", REMOTE_PORT);
    printf(" has been started.\n");
    printf("The IP will be taken from the incoming message\n");
    
    //moottoriAuki.start(Auki);

    AnalogIn potentiometri(A6); //pot. varten
    ALS_spi.format(16, 2);
    ALS_spi.frequency(2000000);
    

    

//metodi potenttiometrin arvon ja Skulman vertailuun
    while (1) {
        //sKulma = potentiometri.read_voltage(); //tai read() antaa 0-1 arvot
        sKulma = potentiometri.read();
        sKulma = sKulma*100.0;  //ANTAA ARVOA 0-100(%)
        //ALS_spi.lock();
        //Auki();
        DatanKerays();
        if (300 > sisaSensorData || sisaSensorData > 1500){
            ValonVertailu();
            PotentiometrinSaato();
        }   
        MQTTJulkaisu();
        
        
        ThisThread::sleep_for(5s);
        
        
    }
    /*while(1) { 
        
        
        sprintf(out_data1, "Input A1 value: %d\n", ain5.read_u16());
        printf("Input A1 value: %d\n", ain5.read_u16());
        ThisThread::sleep_for(1s);  //It doesn't matter how long the main takes

        firstChar = in_data[0];
        lastChart = in_data[10];
        
        if (firstChar == 83 && lastChart == 49){
            printf("ONNISTUI!!!!! First Char on %c ja Last Chart on %c\n ",firstChar,lastChart);
        }
    }*/

}
// Tämä funktio vastaanottaa dataa
void udpReceive()
{
    int i = 0;
    int bytes;
    while(1) {
        bytes = serverUDP.recvfrom(&clientUDP, &in_data, BUFF_SIZE);
        printf("\n");
        printf("bytes received: %d\n",bytes);
        printf("string: %s\n",in_data);
        printf("client address: %s\n", clientUDP.get_ip_address());
        printf("\n");
       
        
    }

}
//tämä funktio siirtää dataa
void udpSend()
{
    while(1){
        //char out_data[BUFF_SIZE] = "demodata";
        clientUDP.set_port(REMOTE_PORT);
        serverUDP.sendto(clientUDP, out_data1, sizeof(out_data1));
        printf("Sending out: %s\n", out_data1);
        printf("with %d" , sizeof(out_data1));
        printf(" data bytes in UDP datagram\n");
        ThisThread::sleep_for(2s);
    }
}



void DatanKerays(){

    sisaCS = 0;
    sisaSensorData = ALS_spi.write(0x01);
    //float sensorDataF = (float)(sensorData/255.0) * 100;
    printf("SisaData: %d\n",sisaSensorData);
    sisaCS = 1;
    //ALS_spi.unlock();
    ThisThread::sleep_for(200ms);


    ulkoCS = 0;
    ulkoSensorData = ALS_spi.write(0x01);
    printf("UlkoData: %d\n",ulkoSensorData);
    ulkoCS = 1;

    sprintf(buffer, "{\"d\":{\"Sensor\":\"Datat \",\"Sisävaloisuus\":%d,\"ulkovaloisuus\":%d}}", sisaSensorData, ulkoSensorData);
    ThisThread::sleep_for(2s);
}

void ValonVertailu(){
     
     
        
        if (300 > sisaSensorData && sKulma < 50){
            //muutetaan säleen kulmaa
            printf("avataan verhoja\n");
            
            tilaus = sKulma+20;
            
            moottorinSuunta.write(1);
            moottorinTeho.write(1);
            
        }
        if (1500 < sisaSensorData && sKulma > 50){
            printf("suljetaan verhot\n");
             
            tilaus = sKulma-20;
            moottorinSuunta.write(0);
            moottorinTeho.write(1);
        }
        
    
}


void Auki(){
    /*while (1) {
    if (usrNappi) {
        moottorinTeho.write(0);
        moottorinSuunta = 1;
    }
    else{
        moottorinTeho.write(0);
        moottorinSuunta = 0;
    }*/

    moottorinTeho.write(1);
    }
void PotentiometrinSaato(){
    printf("tilausarvo: %f\n",tilaus);
    printf("sKulma: %f\n",sKulma);
    if (tilaus > sKulma){
        //pyöritetään moottoria myötäpäivään???
        //nopeus hitaaksi
        
        moottorinSuunta.write(1);
        moottorinTeho.period(0.001f);  
        moottorinTeho.write(0.50f); 
        //moottorinTeho.write(1);
    }
    if (tilaus < sKulma){
        //pyöritetään moottoria vastapäivään???
        //nopeus hitaaksi
        moottorinSuunta.write(0);
        moottorinTeho.period(0.001f);  // 1ms second period
        moottorinTeho.write(0.50f);  //50% duty cycle
        
        //moottorinTeho.write(1);
    }
    
        //pysäytetään moottori
    moottorinTeho.write(0);
    
}
void MQTTJulkaisu(){
    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)buffer;
    msg.payloadlen = strlen(buffer);
    ThisThread::sleep_for(5s);


    client.publish(MBED_CONF_APP_MQTT_TOPIC, msg);
    printf("Published to %s\n",MBED_CONF_APP_MQTT_TOPIC);
}

