const char* post_template = 
"POST /api/v1/%s/telemetry HTTP/1.1\r\n"\
"Host: %s:%d\r\n"\
"User-Agent: python-requests/2.22.0\r\n"\
"Accept-Encoding: gzip, deflate\r\n"\
"Accept: */*\r\n"\
"Connection: keep-alive\r\n"\
"Content-Length: %d\r\n"\
"Content-Type: application/json\r\n"\
"\r\n"\
"{\"device_id\":\"%s\",\"door\":%d,\"pir\":%d,\"temperature\":%.2f,\"humidity\":%.2f,\"smoke\":%d,\"leak\":%d,\"power\":%d}";

const char* index = R"""(
HTTP/1.1 %d OK
Server: MBED OS
Content-Type: text/html

<!DOCTYPE html>
<html lang="en">
    <head>
        <meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate" />
        <meta http-equiv="Pragma" content="no-cache" />
        <meta http-equiv="Expires" content="0" />
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.5.0/css/bootstrap.min.css" integrity="sha384-9aIt2nRpC12Uk9gS9baDl411NQApFmC26EwAOH8WgZl5MYYxFfc+NcPb1dKGj7Sk" crossorigin="anonymous">
        <link rel="icon" href="https://www.systemgroup.net/wp-content/themes/sg/dist/images/favicon.ico" sizes="32x32" />
        <title>Config</title>
        <script>
        var HttpClient = function() {
            this.get = function(aUrl, aCallback) {
                var anHttpRequest = new XMLHttpRequest();
                anHttpRequest.onreadystatechange = function() { 
                    if (anHttpRequest.readyState == 4 && anHttpRequest.status == 200)
                        aCallback(anHttpRequest.responseText);
                }

                anHttpRequest.open( "GET", aUrl);            
                anHttpRequest.send( null );
            }
            this.post = function(aUrl, aCallback){
                var anHttpRequest = new XMLHttpRequest();
                anHttpRequest.onreadystatechange = function() { 
                    if (anHttpRequest.readyState == 4 && anHttpRequest.status == 200)
                        aCallback(anHttpRequest.responseText);
                }
                anHttpRequest.open("POST", aUrl);
                anHttpRequest.send( null );
            }
        }
        function update(){
            var client = new HttpClient();
            client.get('/update', function(response) {
                console.log(response);
                document.getElementById("time").innerHTML = response;
                document.getElementById('msg-box').hidden = true;
            });
        }

        function save(){
            const formData = document.getElementById('data_form');
            let data = {}
            if (document.getElementById("dhcp").checked) {
                data["mode"] = 0;
            }
            else{
                data["mode"] = 1;
                data["ip"] = document.getElementById('ip').value;
                data["subnet"] = document.getElementById('subnet').value;
                data["gateway"] = document.getElementById('gateway').value;
                data["dns"] = document.getElementById('dns').value;
            }
            data["thingsboard"] = document.getElementById('thingsboard').value;
            data["port"] = document.getElementById('port').value;
            data["token"] = document.getElementById('token').value;
            data["device_id"] = document.getElementById('device_id').value;
            for(var key in data){
                if(data[key] === ""){
                    alert("Fill all data");
                    return;
                }
            }
            data = JSON.stringify(data);
            var client = new HttpClient();
            client.post('/data?'+data, function(response) {
                console.log(response);
                msg_box = document.getElementById('msg-box');
                msg = document.getElementById('msg');
                if(response.search("true") != -1){
                    msg_box.style.backgroundColor = "green";
                    msg.innerHTML = "Config saved successfully, Restarting device to apply config";
                }
                else{
                    msg_box.style.backgroundColor = "red";
                    msg.innerHTML = "Failed to save config";
                }
                msg_box.hidden = false;
            });
        }

        function mode_change(radio){
            if (document.getElementById("dhcp").checked) {
                document.getElementById("ip_setting").hidden = true;
            }
            else{
                document.getElementById("ip_setting").hidden = false;
                document.getElementById("ip").value = document.getElementById("ipl").innerHTML;
                document.getElementById("subnet").value = document.getElementById("sbl").innerHTML;
                document.getElementById("gateway").value = document.getElementById("gwl").innerHTML;
                document.getElementById("dns").value = document.getElementById("dnsl").innerHTML;
            }
        }

        function set_url(){
            document.getElementById('thingsboard').value = document.getElementById('old_url').innerHTML
        }
        function set_port(){
            document.getElementById('port').value = document.getElementById('old_port').innerHTML
        }
        function set_token(){
            document.getElementById('token').value = document.getElementById('old_token').innerHTML
        }
        function set_name(){
            document.getElementById('device_id').value = document.getElementById('old_name').innerHTML
        }
        setInterval(update, 1000);
    </script>
    <body>
        <div class="container">
            <div class="jumbotron">
            <h1>Server Room Monitoring</h1>
            <span style="font-size:30px">Uptime:</span>
            <span style="font-size:30px" id="time">%s</span>
            <div id="data_form">
                <label for="thingsboard">Thingsboard address: </label>
                <input id="thingsboard" pattern="^(?:[\w-]+\.)+[a-z]{1,6}$|^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$|^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" maxlength="50" required>
                Current: <b>%<label id="old_url" style="margin-right: 10px;">%s</label><button onclick="set_url()">use</button></b>
                <br>
                <br>
                <label for="port">Port: </label>
                <input id="port" maxlength="5" required>
                Current: <b><label id="old_port" style="margin-right: 10px;">%d</label></b><button onclick="set_port()">use</button>
                <br>
                <br>
                <label for="token">Token: </label>
                <input id="token" maxlength="20" required>
                Current: <b><label id="old_token" style="margin-right: 10px;">%s</label></b><button onclick="set_token()">use</button>
                <br>
                <br>
                <label for="device_id">Device name: </label>
                <input id="device_id" maxlength="20" required>
                Current: <b><label id="old_name" style="margin-right: 10px;">%s</label></b><button onclick="set_name()">use</button>
                <br>
                <br>
                <div style="border: 5px groove black;">
                    <h4>Current IP address:</h4>
                    IP Address: <label id="ipl">%s</label>
                    <br>
                    Subnet Mask: <label id="sbl">%s</label>
                    <br>
                    Default gateway: <label id="gwl">%s</label>
                    <br>
                    DNS Server: <label id="dnsl">%s</label>
                    <br>
                </div>
                <br>
                <h4>
                    IP Mode: 
                    <input type="radio" id="dhcp"   onclick="mode_change(this);" name="ip_mode">
                    <label for="dhcp">DHCP</label>
                    <input type="radio" id="static" onclick="mode_change(this);" name="ip_mode">
                    <label for="static">Static</label><br>
                </h4>
                <div id="ip_setting">
                    <label>device IP Address: </label>
                    <input id="ip" type="text" minlength="7" maxlength="15" size="15" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" required>
                    <br>
                    <br>
                    <label>Subnet mask: </label>
                    <input id="subnet" type="text" minlength="7" maxlength="15" size="15" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" required>
                    <br>
                    <br>
                    <label>Default gateway: </label>
                    <input id="gateway" type="text" minlength="7" maxlength="15" size="15" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" required>
                    <br>
                    <br>
                    <label>DNS Server: </label>
                    <input id="dns" type="text" minlength="7" maxlength="15" size="15" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" required>
                    <br>
                    <br>
                </div>
            </div>
            <button id="submit_button" onclick="save();">save</button>
            <div id="msg-box" hidden>
                <h3><label id="msg"></label></h3>
            </div>
            </div>
        </div>
        <script>
            mode = %d;
            if(mode == 0){
                document.getElementById("dhcp").checked = true;
                document.getElementById("ip_setting").hidden = true;
            }
            else{
                document.getElementById("static").checked = true;
                document.getElementById("ip_setting").hidden = false;
                document.getElementById("ip").value = document.getElementById("ipl").innerHTML;
                document.getElementById("subnet").value = document.getElementById("sbl").innerHTML;
                document.getElementById("gateway").value = document.getElementById("gwl").innerHTML;
                document.getElementById("dns").value = document.getElementById("dnsl").innerHTML;
            }
        </script>
    </body>
</html>
)""";

const char* response = R"""(
HTTP/1.1 %d OK
Server: MBED OS
Content-Type: text/plain

%s
)""";

const char* not_found = R"""(
HTTP/1.1 404 Not Found
Server: MBED OS
Content-Type: text/html

<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.5.0/css/bootstrap.min.css" integrity="sha384-9aIt2nRpC12Uk9gS9baDl411NQApFmC26EwAOH8WgZl5MYYxFfc+NcPb1dKGj7Sk" crossorigin="anonymous">
        <link rel="icon" href="https://www.systemgroup.net/wp-content/themes/sg/dist/images/favicon.ico" sizes="32x32" />
        <title>404</title>
    <body>
        <div class="container">
            <div class="jumbotron">
            <h1>Page not found</h1>
            </div>
        </div>
    </body>
</html>
)""";