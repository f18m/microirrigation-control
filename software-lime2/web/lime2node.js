// 
// lime2node.js
// This is a library of PHP functions to be used on a Linux system to communicate over SPI with the "lime2" node
// part of the https://github.com/f18m/microirrigation-control github project
//
// Author: Francesco Montorsi
// Creation date: Nov 2017
//


/*
   SSE events does not work correctly with long text files like log files!!! :(
    
    var evtSource = new EventSource("http://ffserver.changeip.org:82/lime2node_server_sent_event.php", { withCredentials: false } ); 
    evtSource.onmessage = function(e) {
      //$(js_updated).html(e.data);
      document.getElementById("js_updated").innerHTML = e.data; 
    }
*/


/*
    Rather let's go with WebSockets!!!
*/

var socket;
function ws_init() {
    if (!("WebSocket" in window)) {
        alert("Your browser doesn't support latest Websockets");
        return;
    }
    
    // we expect the WebSocket server to be up and running on the server-side on port 8080
    var wsUrl = 'ws://ffserver.changeip.org:8080/';
    socket = new WebSocket(wsUrl);
    
    socket.onmessage = function (evt) {
        // we can receive only 1 type of message from the WebSocket: a log update
        document.getElementById("js_updated").innerHTML = evt.data; 
    };
    
    socket.onopen = function() {
        // upon successful WebSocket connection, we send command:
        if (window.location.href.indexOf("irrigation-start") !== -1)
             socket.send('TURNON');
        else if (window.location.href.indexOf("irrigation-stop") !== -1)
             socket.send('TURNOFF');
    };
}

function ws_timer() {
    socket.send('GET_UPDATE');
}

// as soon as the HTML loads, setup the WebSocket:
document.addEventListener('DOMContentLoaded', ws_init);

// then every 3sec ask for updates:
var myRefreshTimer = setInterval(ws_timer, 3000);
