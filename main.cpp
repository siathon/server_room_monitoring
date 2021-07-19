#include "main.h"

int main(){
    t.start();
    Watchdog::get_instance().start();
    Watchdog::get_instance().kick();
    
    init_lcd();
    flash.init();
    if(!reset){
        pc.printf("Clear flash\n");
        status_update("Clearing settings");
        clear_flash();
        status_update("Release to continue");
        while(!reset);
    }
    load_config_from_flash();
    load_ip_from_flash();
    // if (!loaded_config) {
    //     load_default_config();
    // }

    door_state = door.read();
    pir_state = pir.read();
    smoke_state = smoke.read();
    leak_state = leak.read();
    power_state = power.read();
    read_tmp_humid();
    update_temp();
    update_hum();
    update_door();
    update_pir();
    update_smoke();
    update_leak();
    update_power();
    init_ethernet();
    send_data_wrapper();
    int timer = 0;
    while (true) {
        check_sensors();
        blink();
        timer++;
        if(!eth.ethernet_link()){
            eth_initialized = false;
            if(!cleared_ip){
                draw_footer();
                cleared_ip = true;
            }
        }
        if(!eth_initialized){
            if(eth.ethernet_link()){
                init_ethernet();
            }
            else{
                wait_us(100000);
            }
        }
        else{
            check_incoming_connection();
        }
        if(eth_initialized){
            timer++;
            if(timer >= 600){
                timer = 0;
                start_server();
            }
        }
                now = time(NULL);
        if(now > last_tmp_hmd_read + 5){
            read_tmp_humid();
        }
        if(now > last_data_send + next_data_time){
            last_data_send = now;
            send_data_wrapper();
        }
    }
}
