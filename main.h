#include <cstdint>
#include <cstdio>
#include <string>

#include "W5500.h"
#include "mbed.h"

#include <AM2315.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <time.h>
#include "calibril_10.h"
#include "calibril_5.h"
#include "bottom_payvand.h"
#include "pages.h"
#include "WIZnetInterface.h"
#include "TCPSocketConnection.h"
#include "TCPSocketServer.h"
#include "DNSClient.h"
// #include "I2CEeprom.h"


#define DARK_GREEN 0x02C1
#define ORANGE 0xFAA0

Watchdog &watchdog = Watchdog::get_instance();

TFT_ILI9163C tft(PB_15, PB_14, PB_13, PC_6, PC_4, PA_8);
WIZnetInterface eth(PA_7, PA_6, PA_5, PC_13, PC_1);
// I2CEeprom eeprom(PB_9, PB_8, 0xA0, 64, 0);
FlashIAP flash;

RawSerial pc(PA_2, PA_3, 115200);

AM2315 am(PC_12, PB_10);

DigitalIn door(PA_15, PullUp);
DigitalIn pir(PB_6, PullDown);
DigitalIn smoke(PB_2, PullUp);
DigitalIn leak(PB_0, PullUp);
DigitalIn power(PA_0, PullDown);
DigitalIn reset(PC_2, PullUp);

DigitalOut sim_power(PB_7, 0);
DigitalOut led(PC_8, 1);

double firmware_version = 1.2;

uint8_t mac_addr[6] = {0x9A, 0x5E, 0xCB, 0xFB, 0x51, 0x90};

bool door_state = false;
bool smoke_state = false;
bool leak_state = false;
bool power_state = false;
bool state_changed = false;
bool pir_ready = false;
bool eth_initialized = false;
bool static_ip = false;
bool loaded_config = false;
bool cleared_ip = true;
double tmp = 0.0, hmd = 0.0;
int tmp_hmd_state = -2;
int blink_cnt = 0;
char disp[50];
int pir_pulses = 0;
int pir_state = 0;
int check_cnt = 0;
int motion_timeout = 10;
int data_send_interval = 10;
int data_retry_interval = 120;
int next_data_time = data_send_interval;

const char* default_url = "things.saymantechcloud.ir";
uint32_t thingsboard_ip = 0;
int default_port = 8080;
const char* default_token = "jhxCGQK9HjLO9yxIWmed";
const char* default_device_id = "200028";

char url[51];
int port;
char token[21];
char device_id[21];
char ip[16];
char subnet[16];
char gw[16];
char dns[16];
char ee_buffer[50];

char send_buffer[350];
char buffer[20000];
char uptime[20];
TCPSocketConnection socket;
TCPSocketServer server;

time_t last_tmp_hmd_read = 0;
time_t last_data_send = 0;
time_t now = 0;

Timer t;

void clear_flash(){
    flash.erase(0x8060000, 0x20000);
}

void int_to_byte_array(int n, char* bsz_buffer){
    for(int i = 0;i < 4;i++){
        bsz_buffer[i] = (n >> (8 * i));
    }
}

int byte_array_to_int(char* bsz_buffer){
    int bsz = 0;
    for(int i = 0;i < 4;i++){
        bsz += (bsz_buffer[i] << (8 * i)); 
    }
    return bsz;
}

const char* get_bool_str(bool is){
    if(is) return "true";
           return "false";
}

void load_default_config(){
    sprintf(url, "%s", default_url);
    sprintf(token, "%s", default_token);
    sprintf(device_id, "%s", default_device_id);
    port = default_port;
}

void load_config_from_flash(){
    int addr = 0x8060000;
    pc.printf("Reading pattern from flash...");
    if (flash.read(ee_buffer, addr, 4) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done.\n");
    int temp = byte_array_to_int(ee_buffer);
    pc.printf("pattern = %X\n", temp);
    if (temp != 0x13131313){
        pc.printf("Config not available on flash\n");
        return;
    }
    addr += 4;

    pc.printf("Reading url size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    pc.printf("Done, url size = %d\n", temp);
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    addr++;
    pc.printf("Reading url from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        url[i] = ee_buffer[i];
    }
    url[temp] = 0;
    pc.printf("Done, url = %s\n", url);
    addr += temp;

    pc.printf("Reading port from flash...");
    if (flash.read(ee_buffer, addr, 4) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    port = byte_array_to_int(ee_buffer);
    pc.printf("Done, port = %d\n", port);
    addr += 4;

    pc.printf("Reading token size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, token size = %d\n", temp);
    addr++;
    pc.printf("Reading token from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        token[i] = ee_buffer[i];
    }
    token[temp] = 0;
    pc.printf("Done, token = %s\n", token);
    addr += temp;

    pc.printf("Reading device_id size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, device_id size = %d\n", temp);
    addr++;
    pc.printf("Reading device_id from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        device_id[i] = ee_buffer[i];
    }
    device_id[temp] = 0;
    pc.printf("Done, device_id = %s\n", device_id);
    addr += temp;
    loaded_config = true;
}

void load_ip_from_flash(){
    int addr = 0x8060100, temp = 0;
    pc.printf("Reading pattern from flash...");
    if (flash.read(ee_buffer, addr, 4) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done.\n");
    temp = byte_array_to_int(ee_buffer);
    pc.printf("pattern = %X\n", temp);
    if (temp != 0x13131313){
        pc.printf("IP not available on flash\n");
        return;
    }
    addr += 4;
    pc.printf("Reading ip size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, ip size = %d\n", temp);
    addr++;
    pc.printf("Reading ip from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        ip[i] = ee_buffer[i];
    }
    ip[temp] = 0;
    pc.printf("Done, ip = %s\n", ip);
    addr += temp;

    pc.printf("Reading subnet size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, subnet size = %d\n", temp);
    addr++;
    pc.printf("Reading subnet from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        subnet[i] = ee_buffer[i];
    }
    subnet[temp] = 0;
    pc.printf("Done, subnet = %s\n", subnet);
    addr += temp;

    pc.printf("Reading gateway size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, gateway size = %d\n", temp);
    addr++;
    pc.printf("Reading gateway from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        gw[i] = ee_buffer[i];
    }
    gw[temp] = 0;
    pc.printf("Done, gateway = %s\n", gw);
    addr += temp;

    pc.printf("Reading dns size from flash...");
    if (flash.read(ee_buffer, addr, 1) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    temp = ee_buffer[0];
    if(temp == 255 | temp == 0){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, dns size = %d\n", temp);
    addr++;
    pc.printf("Reading dns from flash...");
    if (flash.read(ee_buffer, addr, temp) != 0) {
        pc.printf("Failed!\n");
        return;
    }
    for (int i = 0; i < temp; i++) {
        dns[i] = ee_buffer[i];
    }
    dns[temp] = 0;
    if(temp == 255){
        pc.printf("Failed!\n");
        return;
    }
    pc.printf("Done, dns = %s\n", dns);
    static_ip = true;
}

int write_config_to_flash(){
    char sz_buffer[1];
    char sz = 0;
    int addr = 0x8060000;
    int_to_byte_array(0x13131313, ee_buffer);
    pc.printf("Writing pattern 0x13131313 to flash...");
    if (flash.program(ee_buffer, addr, 4) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr+=4;

    sz = sprintf(ee_buffer, "%s", url);
    sz_buffer[0] = sz;
    pc.printf("Writing url size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing url: %s to flash...", url);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;

    int_to_byte_array(port, ee_buffer);
    pc.printf("Writing port: %d to flash...", port);
    if (flash.program(ee_buffer, addr, 4) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += 4;

    sz = sprintf(ee_buffer, "%s", token);
    sz_buffer[0] = sz;
    pc.printf("Writing token size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing token: %s to flash...", token);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;

    sz = sprintf(ee_buffer, "%s", device_id);
    sz_buffer[0] = sz;
    pc.printf("Writing device_id size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing device_id: %s to flash...", device_id);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;
    return 0;
}

int write_ip_to_flash(){
    char sz_buffer[1];
    char sz = 0;
    int addr = 0x8060100;

    int_to_byte_array(0x13131313, ee_buffer);
    pc.printf("Writing pattern 0x13131313 to flash...");
    if (flash.program(ee_buffer, addr, 4) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr+=4;

    sz = sprintf(ee_buffer, "%s", ip);
    sz_buffer[0] = sz;
    pc.printf("Writing ip size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing ip: %s to flash...", ip);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;

    sz = sprintf(ee_buffer, "%s", subnet);
    sz_buffer[0] = sz;
    pc.printf("Writing subnet size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing subnet: %s to flash...", subnet);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;

    sz = sprintf(ee_buffer, "%s", gw);
    sz_buffer[0] = sz;
    pc.printf("Writing gateway size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing gateway: %s to flash...", gw);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;

    sz = sprintf(ee_buffer, "%s", dns);
    sz_buffer[0] = sz;
    pc.printf("Writing dns size: %d to flash...", sz);
    if (flash.program(sz_buffer, addr, 1) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr++;
    pc.printf("Writing dns: %s to flash...", dns);
    if (flash.program(ee_buffer, addr, sz) != 0) {
        pc.printf("Failed\n");
        return -1;
    }
    pc.printf("Done\n");
    addr += sz;

    return 0;
}

void status_update(string mystr){
    int x = 35, y = 144;
    tft.setCursor(x, y);
    tft.fillRect(x,y-6,128,9,BLACK);
    tft.setTextSize(1);
    tft.setFont(&calibril_5);
    tft.setTextColor(YELLOW);
    tft.setCursor(x, y);
    sprintf(disp, "%s", mystr.c_str());
    tft.print(disp);
}

void update_temp(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);
    tft.fillRect(61,3,45,13,BLACK);
    tft.setCursor(60, 15);

    if(tmp_hmd_state <= 0){
        sprintf(disp,"  ----");
    }
    else{
        sprintf(disp,"%05.2f", tmp);
    }

    tft.print(disp);
}

void update_hum(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);

    tft.fillRect(56,21,45,13,BLACK);
    tft.setCursor(55, 33);
    if(tmp_hmd_state <= 0){
        sprintf(disp,"  ----");
    }
    else{
        sprintf(disp,"%05.2f", hmd);
    }
    tft.print(disp);
}

void update_door(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);

    if(door_state){
        tft.fillRect(56,38,44,14,RED);
        tft.fillRect(100,38,18,14,BLACK);
        sprintf(disp,"OPEN");
    }
    else{
        tft.fillRect(56,38,62,14,DARK_GREEN);
        sprintf(disp,"CLOSED");
    }
    tft.setCursor(55, 51);
    tft.print(disp);
}

void update_pir(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);

    if(pir_state == 0){
        tft.fillRect(75,56,29,15,DARK_GREEN);
        sprintf(disp,"NO");
    }
    else if(pir_state == 1){
        tft.fillRect(75,56,29,15,ORANGE);
        sprintf(disp,"MB");
    }
    else{
        tft.fillRect(75,56,29,15,RED);
        sprintf(disp,"YES");
    }

    tft.setCursor(75, 69);
    tft.print(disp);
}

void update_smoke(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);

    if(smoke_state == 1){
        tft.fillRect(75,74,29,15,DARK_GREEN);
        sprintf(disp,"NO");
    }
    else{
        tft.fillRect(75,74,29,15,RED);
        sprintf(disp,"YES");
    }

    tft.setCursor(75, 87);
    tft.print(disp);
}

void update_leak(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);

    if(leak_state == 1){
        tft.fillRect(75,92,29,15,DARK_GREEN);
        sprintf(disp,"NO");
    }
    else{
        tft.fillRect(75,92,29,15,RED);
        sprintf(disp,"YES");
    }

    tft.setCursor(75, 105);
    tft.print(disp);
}

void update_power(){
    tft.setFont(&calibril_10);
    tft.setTextColor(WHITE);

    if(power_state == 1){
        tft.fillRect(75,110,29,15,DARK_GREEN);
        sprintf(disp,"YES");
    }
    else{
        tft.fillRect(75,110,29,15,RED);
        sprintf(disp,"NO");
    }

    tft.setCursor(75, 123);
    tft.print(disp);
}

void draw_footer(){
    tft.drawRGBBitmap(0, 132, bottom_payvand, 128, 28);
}

void show_ip(){
    tft.setCursor(50, 157);
    tft.setTextSize(1);
    tft.setFont(&calibril_5);
    tft.setTextColor(WHITE);
    sprintf(disp, "IP:%s", eth.getIPAddress());
    tft.print(disp);
    cleared_ip = false;
}

void init_lcd(){
    tft.begin();
    tft.setBitrate(48000000);
    tft.setRotation(2);
    tft.clearScreen();
    tft.setFont(&calibril_10);
    int y = 15, h = 18;
    tft.setTextColor(YELLOW);
    tft.setCursor(3, y);
    sprintf(disp,"Temp:");
    tft.print(disp);

    tft.setCursor(110, y);
    sprintf(disp,"C");
    tft.print(disp);
    y += h;

    tft.setCursor(3, y);
    sprintf(disp,"Hum:");
    tft.print(disp);

    tft.setCursor(110, y);
    sprintf(disp,"%%");
    tft.print(disp);
    y += h;

    tft.setCursor(3, y);
    sprintf(disp,"Door:");
    tft.print(disp);
    y += h;

    tft.setCursor(3, y);
    sprintf(disp,"Motion:");
    tft.print(disp);
    y += h;

    tft.setCursor(3, y);
    sprintf(disp,"Smoke:");
    tft.print(disp);
    y += h;

    tft.setCursor(3, y);
    sprintf(disp,"Leakage:");
    tft.print(disp);
    y += h;

    tft.setCursor(3, y);
    sprintf(disp,"Power:");
    tft.print(disp);

    draw_footer();
}

void start_server(){
    server.close();
    int result = server.bind(8080);
    if(result != 0){
        eth_initialized = false;
        pc.printf("Failed to bind server port\n");
    }
    result = server.listen();
    if(result != 0){
        pc.printf("Failed to listen\n");
        eth_initialized = false;
    }
    server.set_blocking(false, 100);

}

int init_ethernet(){
    if(!eth.ethernet_link()){
        return -1;
    }
    status_update("Initializing ethernet");
    eth.reset();
    eth.reset();
    Watchdog::get_instance().kick();
    pc.printf("Initializing ethernet ");
    if (static_ip) {
        pc.printf("with static ip...");
        if (eth.init(mac_addr, ip, subnet, gw, dns) != 0) {
            status_update("Failed");
            pc.printf("Failed\n");
            wait_us(100000);
            return -1;
        }
    }
    else{
        pc.printf("with dhcp...");
        if(eth.init(mac_addr) != 0){
        pc.printf("Failed\n");
        status_update("Failed");
        wait_us(100000);
        return -1;
        }
    }
    Watchdog::get_instance().kick();
    if(eth.connect() != 0){
        pc.printf("Failed\n");
        status_update("Failed");
        wait_us(100000);
        return -1;
    }
    eth_initialized = true;
    status_update("Done.");
    pc.printf("Done.\nmac:%s - ip:%s - subnet:%s - gateway:%s - dns:%s\n", eth.getMACAddress(), eth.getIPAddress(), eth.getNetworkMask(), eth.getGateway(), eth.getDNS());
    show_ip();
    start_server();
    return 0;
}

int send_data(){
    last_data_send = time(NULL);
    if(!eth_initialized){
        if(init_ethernet() != 0){
            return -1;
        }
    }
    uint32_t temp;
    eth.gethostbyname("www.time.ir", &temp);
    Watchdog::get_instance().kick();
    if (!socket.is_connected()) {
        status_update("Connecting");
        pc.printf("Connecting to thingsboard...");

        if(socket.connect(url, port) != 0){
            socket.close();
            pc.printf("Failed\n");
            return -1;
        }
    }
    socket.set_blocking(false, 100);
    pc.printf("Done\n");
    status_update("done");
    int len = pc.printf("{\"device_id\":\"%s\",\"door\":%d,\"pir\":%d,\"temperature\":%.2f,\"humidity\":%.2f,\"smoke\":%d,\"leak\":%d,\"power\":%d}\r\n", device_id, door_state, pir_state, tmp, hmd, !smoke_state, !leak_state, power_state) - 2;
    pc.printf("Sending data...");
    status_update("Sending data...");
    int sz = sprintf(send_buffer, post_template, token, url, port, len, device_id, door_state, pir_state, tmp, hmd, !smoke_state, !leak_state, power_state);
    Watchdog::get_instance().kick();
    // pc.printf("size = %d\n%s\n", sz, send_buffer);
    if(socket.send(send_buffer, sz) != sz){
        socket.close();
        pc.printf("Failed\n");
        return -1;
    }
    // socket.close();
    pc.printf("Done\n");
    status_update("Done");
    return 0;
}

void send_data_wrapper(){
    if(!loaded_config){
        return;
    }
    if(send_data() == 0){
        next_data_time = data_send_interval;
    }
    else{
        next_data_time = data_retry_interval;
    }
}

void read_tmp_humid(){
    if(am.read()){
        tmp = am.celsius;
        hmd = am.humidity;
        tmp_hmd_state = 1;
    }
    else{
        if(tmp_hmd_state == -2){
            tmp = -1.0;
            hmd = -1.0; 
        }
        else{
            tmp_hmd_state = -1;
            tmp = -2.0;
            hmd = -2.0;
        }
    }
    update_temp();
    update_hum();
    last_tmp_hmd_read = time(NULL);
}

void check_sensors(){
    Watchdog::get_instance().kick();
    check_cnt++;
    if(door.read() != door_state){
        door_state = door.read();
        state_changed = true;
        update_door();
    }

    if(smoke.read() != smoke_state){
        smoke_state = smoke.read();
        state_changed = true;
        update_smoke();
    }

    if(leak.read() != leak_state){
        leak_state = leak.read();
        state_changed = true;
        update_leak();
    }

    if (power.read() != power_state) {
        power_state = power.read();
        state_changed = true;
        update_power();
    }

    if(pir == 0){
        pir_ready = true;
    }

    if(pir && pir_ready){
        check_cnt = 0;
        pir_pulses++;
        int new_pir_state = -1;
        if(pir_pulses == 0){
            new_pir_state = 0;
        }
        if(pir_pulses == 1){
            new_pir_state = 1;
        }
        else if(pir_pulses > 1){
            new_pir_state = 2;
        }
        if(new_pir_state != pir_state){
            state_changed = true;
            pir_state = new_pir_state;
        }
        pir_ready = false;
        update_pir();
    }

    if(state_changed){
        send_data_wrapper();
        state_changed = false;
    }
    
    if(check_cnt > motion_timeout * 10){
        pir_pulses = 0;
        pir_state = 0;
        update_pir();
        check_cnt = 0;
    }
}

void blink(){
    if(led){
        led = 0;
    }
    else{
        blink_cnt++;
    }
    if(blink_cnt >= 10){
        led = 1;
        blink_cnt = 0;
        status_update("Running...");
    }
}

void update_runtime(){
    int h, m, s;
    long long temp_millis = t.read();
    h = temp_millis / 3600;
    temp_millis = temp_millis % 3600;
    m = temp_millis / 60;
    s = temp_millis % 60;
    sprintf(uptime, "%02d:%02d:%02d", h, m, s);
}

void check_incoming_connection(){
    TCPSocketConnection client;
    client.set_blocking(false);
    if(server.accept(client) != 0){
        return;
    }
    
    pc.printf("Connection from: %s\r\n", client.get_address());
    int n = client.receive(buffer, 20000);
    if (n <= 0) {
        return;
    }
    buffer[n] = 0;
    // pc.printf("request = %s\n", buffer);
    string data(buffer);
    int idx1 = data.find("/");
    int idx2 = data.find("HTTP");
    if(idx1 == -1 || idx2 == -1){
        pc.printf("Invalid request\n");
        return;
    }
    string method = data.substr(0, idx1-1);
    string path = data.substr(idx1, idx2 - idx1 - 1);
    pc.printf("new request: Method = %s, path = %s\n", method.c_str(), path.c_str());
    if(method.compare("GET") == 0){
        if(path.compare("/") == 0){
            update_runtime();
            int len = sprintf(buffer, index, 200, uptime, 
                              url, port, token, device_id,
                              eth.getIPAddress(), 
                              eth.getNetworkMask(), 
                              eth.getGateway(), 
                              eth.getDNS(),
                              static_ip);
            client.send_all(buffer, len);
        }
        else if(path.compare("/update") == 0){
            update_runtime();
            int len = sprintf(buffer, response, 200, uptime);
            client.send_all(buffer, len);
        }
        else{
            int len = sprintf(buffer, "%s", not_found);
            client.send_all(buffer, len);
        }
    }
    else if(method.compare("POST") == 0){
        int idx = 0;
        while(true){
            idx = path.find("%22");
            if (idx == -1) {
                break;
            }
            path.replace(idx, 3, "");
        }
        idx = path.find("?");
        if(idx == -1){
            return;
        }
        string data = path.substr(idx+1);
        path = path.substr(0, idx);
        pc.printf("path: %s, data: %s\n", path.c_str(), data.c_str());
        if(path.compare("/data") == 0){
            int idx0, idx1;

            idx0 = data.find("thingsboard");
            idx1 = data.find(",", idx0+1);
            sprintf(url, "%s", data.substr(idx0+12, idx1 - idx0 - 12).c_str());

            idx0 = data.find("port");
            idx1 = data.find(",", idx0+1);
            port = (int)atoi(data.substr(idx0+5, idx1 - idx0 - 5).c_str());

            idx0 = data.find("token");
            idx1 = data.find(",", idx0+1);
            sprintf(token, "%s", data.substr(idx0+6, idx1 - idx0 - 6).c_str());

            idx0 = data.find("device_id");
            idx1 = data.find("}", idx0+1);
            sprintf(device_id, "%s", data.substr(idx0+10, idx1 - idx0 - 10).c_str());
            
            idx0 = data.find("mode");
            idx1 = data.find(",", idx0+1);
            int mode = (int)atoi(data.substr(idx0+5, idx1 - idx0 - 5).c_str());

            pc.printf("thingsboard: %s, port: %d, token: %s, device name: %s\n", url, port, token, device_id);
            int len = 0;
            clear_flash();
            if(write_config_to_flash() == 0){
                if(mode == 0 && !static_ip){
                    len = sprintf(buffer, response, 200, "true");
                    client.send_all(buffer, len);
                    loaded_config = true;
                    pc.printf("Configuration saved successfully\n");
                    client.close();
                    wait_us(1000000);
                    flash.deinit();
                    NVIC_SystemReset();
                    return;
                }
            }  
            else{
                len = sprintf(buffer, response, 200, "0");
                client.send_all(buffer, len);
                pc.printf("Failed to save configuration\n");
                return;
            }

            idx0 = data.find("ip");
            idx1 = data.find(",", idx0+1);
            sprintf(ip, "%s", data.substr(idx0+3, idx1 - idx0 - 3).c_str());

            idx0 = data.find("subnet");
            idx1 = data.find(",", idx0+1);
            sprintf(subnet, "%s", data.substr(idx0+7, idx1 - idx0 - 7).c_str());

            idx0 = data.find("gateway");
            idx1 = data.find(",", idx0+1);
            sprintf(gw, "%s", data.substr(idx0+8, idx1 - idx0 - 8).c_str());

            idx0 = data.find("dns");
            idx1 = data.find(",", idx0+1);
            sprintf(dns, "%s", data.substr(idx0+4, idx1 - idx0 - 4).c_str());
            pc.printf("ip: %s, subnet: %s, gateway: %s, dns: %s\n", ip, subnet, gw, dns);

            if(write_ip_to_flash() == 0){
                len = sprintf(buffer, response, 200, "true");
                client.send_all(buffer, len);
                pc.printf("IP setting saved successfully\n");
                client.close();
                wait_us(1000000);
                flash.deinit();
                NVIC_SystemReset();
            }
            else{
                len = sprintf(buffer, response, 200, "0");
                client.send_all(buffer, len);
                pc.printf("Failed to save configuration\n");
            }
        }
    }
    else{
        int len = sprintf(buffer, "%s", not_found);
        client.send_all(buffer, len);
    }
}