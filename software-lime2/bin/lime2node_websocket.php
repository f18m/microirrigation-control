<?php
  
    // 
    // lime2node_websocket.php
    // This is a library of PHP functions to be used on a Linux system to communicate over SPI with the "lime2" node
    // part of the https://github.com/f18m/microirrigation-control github project.
    //
    // This websocket server helps solving 2 problems:
    //  1) allows the webserver to immediately provide the client with an HTML page without having to wait for the
    //     completion of command transmission (which may take >20secs).
    //  2) avoid having the webserver running as "root" user. This websocket server still needs to run as root though,
    //     to be able to access the SPI bus. 
    //
    // Author: Francesco Montorsi
    // Creation date: Nov 2017
    //
    
    
    // Ratchet stuff
    
    use Ratchet\MessageComponentInterface;
    use Ratchet\ConnectionInterface;
    require __DIR__ . '/vendor/autoload.php';
    include 'lime2node_comm_lib.php';
  
  
    // constants
    
    $logfile='/var/log/lime2node_websocket_srv.log';
  
    function websocketsrv_write_log($msg) {
        global $logfile;
        
        $date = new DateTime();
        $date = $date->format("y:m:d h:i:s");

        file_put_contents($logfile, $date . ": " . $msg . "\n", FILE_APPEND | LOCK_EX);
    }
  
    /*
    
        APPARENTLY MULTI-THREADING IS NOT REALLY NATIVE IN PHP SERVERS AROUND...
        FOR THIS REASON AVOID USING THREADS AND RATHER RELY ON EXTERNAL SCRIPTS RUN IN BACKGROUND! 
    
    class AsyncOperation extends Thread
    {
        public function __construct($arg) {
            $this->arg = $arg;
        }
    
        public function run() {
            lime2node_empty_log();
            lime2node_write_log("PHP Lime2Node backend: acquired lock file $cli_command_lockfile... proceeding with command: " . $this->arg);
            lime2node_init_over_spi();
            
            // for testing purpose: send TURNON command
            $tid = lime2node_get_last_transaction_id_and_advance();
            $ack_contents = array();
            if ($this->arg == "TURNON")
              $result = lime2node_send_spi_cmd($turnon_cmd, $tid);
            else
              $result = lime2node_send_spi_cmd($turnoff_cmd, $tid);
              
            if ($result["valid"])
              lime2node_wait_for_ack($tid);
          
            lime2node_write_log("PHP Lime2Node backend: command sequence completed. Exiting.");
        }
    }*/
    
    
    
    
    // Ratchet class for SPI communication:
    
    class Lime2NodeWebSocket implements MessageComponentInterface
    {
        protected $clients;
        protected $background_thread;
    
        public function __construct() {
            $this->clients = new \SplObjectStorage;
        }
        
        public function onOpen(ConnectionInterface $conn) {
            // Store the new connection to send messages to later
            $this->clients->attach($conn);
            websocketsrv_write_log("New connection with resource ID {$conn->resourceId}");
        }
    
        public function onMessage(ConnectionInterface $from, $msg) {
            global $last_spi_op_logfile;
            websocketsrv_write_log(sprintf('From connection with resource ID %d received message "%s"', $from->resourceId, $msg));
  
            if ($msg == "TURNON" || $msg == "TURNOFF")
            {
                /*   MULTITHREADING OPTION DISABLED FOR NOW:
                
                if ($background_thread->isRunning())
                {
                  $background_thread->join();
                  unset($background_thread);
                }
                
                $background_thread = new AsyncOperation($msg);
                $background_thread->start();*/
                
                
                // run the command in a detached shell.
                // Note that the /dev/null redirections are very important otherwise PHP will NOT detach the child shell and will
                // instead wait for that to complete!
                
                $command = '/opt/microirrigation-control/software-lime2/bin/lime2node_cli_test.php ' . $msg . ' 2>/dev/null >/dev/null &';
                websocketsrv_write_log("Received ${msg} command; running ${command}");
                
                lime2node_empty_log();
              	shell_exec($command);
            }
            else if ($msg == "GET_UPDATE")
            {
                websocketsrv_write_log("Sending contents of file $last_spi_op_logfile over the websocket");
                
                 // last_spi_op_logfile variable is defined in lime2node_comm_lib.php
                $contents = file_get_contents($last_spi_op_logfile);
                $from->send($contents);
            }
            else
            {
                websocketsrv_write_log("Unknown ${msg} command");
            }
        }
    
        public function onClose(ConnectionInterface $conn) {
            // The connection is closed, remove it, as we can no longer send it messages
            $this->clients->detach($conn);
            websocketsrv_write_log("Connection with resource ID {$conn->resourceId} has disconnected");
        }
    
        public function onError(ConnectionInterface $conn, \Exception $e) {
            websocketsrv_write_log("An error has occurred: {$e->getMessage()}");
            $conn->close();
        }
    }
?>


