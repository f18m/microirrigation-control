<?php
  
    // 
    // lime2node_websocket_srv.php
    // This is a library of PHP functions to be used on a Linux system to communicate over SPI with the "lime2" node
    // part of the https://github.com/f18m/microirrigation-control github project
    //
    // Author: Francesco Montorsi
    // Creation date: Nov 2017
    //
    
    include 'lime2node_websocket.php';
    
    use Ratchet\Server\IoServer;
    use Ratchet\Http\HttpServer;
    use Ratchet\WebSocket\WsServer;

    require __DIR__ . '/vendor/autoload.php';
  
  
    // Start Ratchet WebSocket server:
  
    websocketsrv_write_log("Starting Lime2Node WebSocket server");
    
    $server = IoServer::factory(
        new HttpServer(
            new WsServer(
                new Lime2NodeWebSocket()
            )
        ),
        8080
    );

    $server->run();
    
    websocketsrv_write_log("Ending Lime2Node WebSocket server");
?>
