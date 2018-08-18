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
        // we can receive only 1 type of message from the WebSocket: a log update
        document.getElementById("js_updated").innerHTML = evt.data; 
    };
    
    socket.onopen = function() {
        socket_ready = true;
        console.log("Socket now ready.");
    };
}

function ws_timer() {
    socket.send(JSON.stringify({
  command: 'GET_UPDATE'
}));
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
    socket.send(JSON.stringify({
  command: cmd,
  commandParameter: cmdParam
}));
}



// SCRIPT MAIN

// as soon as the HTML loads, setup the WebSocket:
document.addEventListener('DOMContentLoaded', ws_init);

// then every 3sec ask for updates:
var myRefreshTimer = setInterval(ws_timer, 3000);
