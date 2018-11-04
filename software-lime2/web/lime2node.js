// 
// lime2node.js
// This is a library of Javascript functions that help managing a websocket
// for sending dynamic updates to the browser where this Javascript runs.
//
// The server-side is written in PHP and is contained into the 'lime2node_websocket_srv.php'
// file.
//  
// This is part of the https://github.com/f18m/microirrigation-control github project
//
// Author: Francesco Montorsi
// Creation date: Nov 2017
//


/*
   NOTE: SSE events do not work correctly with long text files like log files!!! :(
    
    var evtSource = new EventSource("http://ffserver.changeip.org:82/lime2node_server_sent_event.php", { withCredentials: false } ); 
    evtSource.onmessage = function(e) {
      //$(js_updated).html(e.data);
      document.getElementById("js_updated").innerHTML = e.data; 
    }

    Rather let's go with WebSockets!!!
*/


// CONSTANTS

var ws_server_url = 'ws://ffserver.changeip.org:8080/';
    // we expect the WebSocket server to be up and running on the server-side on port 8080

// GLOBALS

var socket;
var socket_ready;
var last_command;
var num_updates = 0;
var num_battery_updates = 0;

// FUNCTIONS

function ws_init() {
    if (!("WebSocket" in window)) {
        alert("Your browser doesn't support latest Websockets");
        return;
    }
    
    // create socket
    socket = new WebSocket(ws_server_url);
    socket_ready = false;
    
    // attach the websocket "message received" to the HTML element showing the acivity log:
    socket.onmessage = function (evt) {
        logContent = evt.data; 
    
        // we can receive only 2 types of message from the WebSocket: a log update or battery level updates
        if (last_command == "GET_BATTERY_LEVEL" && logContent )
        {
          var batteryVoltagePercentage = parseFloat(logContent);
          console.log("Received battery level update: %f.", batteryVoltagePercentage);
          if (!isNaN(batteryVoltagePercentage))
          {
            // use Bootstrap framework to update the bar: (see http://getbootstrap.com)
            $("#battery .bar").css("width", batteryVoltagePercentage + "%");
            //document.getElementById("js_updated").style.width = batteryVoltagePercentage + "%";
          }
        }
        else
          document.getElementById("js_updated").innerHTML = logContent; 
    };
    
    socket.onopen = function() {
        socket_ready = true;
        console.log("Socket now ready.");
    };
}

function ws_timer() {

    // launch a battery level update:
    if (socket_ready)
    {
      if (num_battery_updates == 0 || (num_updates%100)==0)
      {
        ws_send_cmd("GET_BATTERY_LEVEL", "");
        console.log("Refreshing battery level.");
        
        // launched battery update!
        num_battery_updates++;
      }
    }

    // ask log content updates:
    socket.send(JSON.stringify({
  command: 'GET_UPDATE'
}));

    num_updates++;
}

function ws_check_ready() {
    if (!socket_ready)
    {
        alert('Socket not ready. Please wait a couple more seconds and retry!');
        return false;
    }
    return true;
}

// this is the function that can be linked to HTML elements:
function ws_send_cmd(cmd, cmdParam) {
    if (!ws_check_ready())
      return;
    
    // send JSON over the websocket:
    socket.send(JSON.stringify({
  command: cmd,
  commandParameter: cmdParam
}));

    last_command = cmd;
}



// SCRIPT MAIN

// as soon as the HTML loads, setup the WebSocket:
document.addEventListener('DOMContentLoaded', ws_init);

// then every 1sec ask for updates:
var myRefreshTimer = setInterval(ws_timer, 1000);
