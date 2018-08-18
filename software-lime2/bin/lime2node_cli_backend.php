#!/usr/bin/php
<?php

  //
  // lime2node_cli_backend.php
  // Small utility to use the PHP library for over-SPI communication
  // part of the https://github.com/f18m/microirrigation-control github project
  //
  // Author: Francesco Montorsi
  // Creation date: Nov 2017
  //

  include 'lime2node_comm_lib.php';




  // main:

  lime2node_acquire_lock_or_die();
  
  
  // parse command line arguments:
  
  $longopts = array(
      "help",
      "spi-command:",               // Required value
      "spi-command-parameter:",     // Required value
      "log-file:"                   // Required value
  );
  $options = getopt("", $longopts);
  //var_dump($options);
    
  $cmd_to_send = $turnon_cmd;
  $cmdParameter = "1";     // currently when cmd=TURNON/TURNOFF only 2 values of cmdParameter are supported: '1' or '2' to indicate the relay channel group
  $run_cmd_sequence = false;
  
  foreach (array_keys($options) as $opt) switch ($opt) {
    case "spi-command":
      $value = $options["spi-command"];
      if ($value == "TURNON") {
        $cmd_to_send = $turnon_cmd;
      } else if ($value == "TURNOFF") {
        $cmd_to_send = $turnoff_cmd;
      } else if ($value == "TURNON_WITH_TIMER") {
        $run_cmd_sequence = true;
      } else {
        echo "Invalid value for --spi-command: [$value]. Only values 'TURNON' or 'TURNOFF' or 'TURNON_WITH_TIMER' are accepted.\n";
        die();
      }
      break;
  
    case "spi-command-parameter":
      $value = $options["spi-command-parameter"];
      if (intval($value) <= 0 || intval($value) >= 100) {
        echo "Invalid value for --spi-command-parameter: [$value]. Only values in range [1-99] are accepted.\n";
        die();
      }
      $cmdParameter = $value;
      break;
      
    case "log-file":
      $value = $options["log-file"];
      lime2node_set_logfile($value);
      break;
      
    case "help":
      echo "lime2node_cli_backend.php\n";
      echo "  --help\n";
      echo "  --spi-command <cmd>\n";
      echo "  --spi-command-parameter <param>\n";
      echo "  --log-file <logfile>: if not provided, everything is printed on stdout\n";
      exit(1);
      
    default:
      echo "Unknown option $opt\n";
      die();
  }
  
  if (array_key_exists('log-file', $options) == false)
    lime2node_set_logfile("");    // then set logfile == stdout

  // now start SPI activities:

  echo "Opening for logging '" . lime2node_get_logfile() . "'...\n";
  lime2node_empty_log();
  lime2node_write_log("INFO", "PHP Lime2Node backend: acquired lock on SPI bus... proceeding with command sequence");
  lime2node_init_over_spi();
  
  if ($run_cmd_sequence)
  {
    $timerMin = intval($cmdParameter);
  
    // run a TURNON command 
    $cmd_to_send = $turnon_cmd;
    $cmdParameter = "2";
    $tid = lime2node_get_last_transaction_id_and_advance();
    lime2node_write_log("INFO", "Sending $cmd_to_send command (with param=$cmdParameter and tid=$tid) to remote node...");
    $result = lime2node_send_spi_cmd($cmd_to_send, $tid, $cmdParameter);
    if ($result["valid"] == false) {
      lime2node_write_log("INFO", "Command TX over SPI failed. Aborting.");
      die();
    }
    
    lime2node_write_log("INFO", "First command was sent successfully over SPI. Waiting for the ACK from the remote node...");
    $received_ack = lime2node_wait_for_ack($tid);
    if ($received_ack["valid"] == false) {
      lime2node_write_log("INFO", "Failed waiting for the ACK. Turn ON command probably was never received. Aborting.");
      die();
    }
    
    lime2node_write_log("INFO", "Successfully received the ACK from the remote node! Battery read is " .  $received_ack["batteryRead"] );
    
    // now wait for a certain amount of minutes
    lime2node_write_log("INFO", "Now sleeping for ${timerMin} minutes before sending TURNOFF command");
    sleep(60 * $timerMin);
    
    // run a TURNOFF command 
    $cmd_to_send = $turnoff_cmd;
    $cmdParameter = "2";
    $tid = lime2node_get_last_transaction_id_and_advance();
    lime2node_write_log("INFO", "Sending $cmd_to_send command (with param=$cmdParameter and tid=$tid) to remote node...");
    $result = lime2node_send_spi_cmd($cmd_to_send, $tid, $cmdParameter);
    if ($result["valid"] == false) {
      lime2node_write_log("INFO", "Command TX over SPI failed. Aborting.");
      die();
    }
    
    lime2node_write_log("INFO", "First command was sent successfully over SPI. Waiting for the ACK from the remote node...");
    $received_ack = lime2node_wait_for_ack($tid);
    if ($received_ack["valid"] == false) {
      lime2node_write_log("INFO", "Failed waiting for the ACK. Turn ON command probably was never received. Aborting.");
      die();
    }
    
    lime2node_write_log("INFO", "Successfully received the ACK from the remote node! Battery read is " .  $received_ack["batteryRead"] );
  }
  else
  {
    // basic TURNON/TURNOFF command
    $tid = lime2node_get_last_transaction_id_and_advance();
    lime2node_write_log("INFO", "Sending $cmd_to_send command (with param=$cmdParameter and tid=$tid) to remote node...");
    $result = lime2node_send_spi_cmd($cmd_to_send, $tid, $cmdParameter);
  
    if ($result["valid"])
    {
      lime2node_write_log("INFO", "Command was sent successfully over SPI. Waiting for the ACK from the remote node...");
  
      $received_ack = lime2node_wait_for_ack($tid);
      if ($received_ack["valid"])
        lime2node_write_log("INFO", "Successfully received the ACK from the remote node! Battery read is " .  $received_ack["batteryRead"] );
      else
        lime2node_write_log("INFO", "Failed waiting for the ACK.");
    }
    else {
      lime2node_write_log("INFO", "Command TX over SPI failed. Aborting.");
    }
  }
  
  lime2node_write_log("INFO", "PHP Lime2Node backend: command sequence completed. Exiting.");
?>
