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
      "log-file:",                   // Required value
      "log-level:"                   // Required value
  );
  $options = getopt("", $longopts);
  //var_dump($options);
    
  $cmd_to_send = $turnon_cmd;
  $cmdParameter = "1";     // currently when cmd=TURNON/TURNOFF only 2 values of cmdParameter are supported: '1' or '2' to indicate the relay channel group
  $run_cmd_sequence = false;
  $get_battery_level = false;
  
  foreach (array_keys($options) as $opt) switch ($opt) {
    case "spi-command":
      $value = $options["spi-command"];
      if ($value == "TURNON") {
        $cmd_to_send = $turnon_cmd;
      } else if ($value == "TURNOFF") {
        $cmd_to_send = $turnoff_cmd;
      } else if ($value == "NOOP") {
        $cmd_to_send = $noop_cmd;
      } else if ($value == "STATUS") {
        echo "Invalid value for --spi-command: the STATUS command is not for the remote node and is reserved to achieve SPI communications.\n";
        die();
        //$cmd_to_send = $status_cmd;
        //$cmdParameter = "";
      } else if ($value == "TURNON_WITH_TIMER") {
        $run_cmd_sequence = true;
      } else if ($value == "GET_BATTERY_LEVEL") {
        $get_battery_level = true;
      } else {
        echo "Invalid value for --spi-command: [$value]. Only values 'TURNON' or 'TURNOFF' or 'NOOP' or 'TURNON_WITH_TIMER' or 'GET_BATTERY_LEVEL' are accepted.\n";
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
      
    case "log-level":
      $value = $options["log-level"];
      lime2node_set_loglevel($value);
      break;
      
    case "help":
      echo "lime2node_cli_backend.php\n";
      echo "  --help\n";
      echo "  --spi-command <cmd>\n";
      echo "  --spi-command-parameter <param>\n";
      echo "  --log-file <logfile>: if not provided, everything is printed on stdout\n";
      echo "  --log-level INFO/DEBUG: if not provided, default log level is INFO\n";
      exit(1);
      
    default:
      echo "Unknown option $opt\n";
      die();
  }
  
  if (array_key_exists('log-file', $options) == false)
    lime2node_set_logfile("stdout");    // then set logfile == stdout

  // now start SPI activities:

  echo "Opening for logging '" . lime2node_get_logfile() . "' with log level " . lime2node_get_loglevel() . "...\n";
  lime2node_empty_log();
  lime2node_write_log("INFO", "PHP Lime2Node backend: acquired lock on SPI bus... proceeding with command sequence");
  lime2node_init_over_spi();
  
  if ($run_cmd_sequence)
  {
    $timerMin = intval($cmdParameter);
  
    for ($i = 1; $i <= 2; $i++) {
       // run a TURNON command 
      $cmd_to_send = $turnon_cmd;
      $cmdParameter = strval($i);
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
      
      lime2node_write_log("INFO", "Successfully received the ACK from the remote node! " . lime2node_get_battery_info($received_ack["batteryRead"]) );
    }
    
    // now wait for a certain amount of minutes
    lime2node_write_log("INFO", "Now sleeping for ${timerMin} minutes before sending TURNOFF command");
    sleep(60 * $timerMin);
    
    for ($i = 1; $i <= 2; $i++) {
      // run a TURNOFF command 
      $cmd_to_send = $turnoff_cmd;
      $cmdParameter = strval($i);
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
      
      lime2node_write_log("INFO", "Successfully received the ACK from the remote node! " . lime2node_get_battery_info($received_ack["batteryRead"]) );
    }
  }
  else if ($get_battery_level)
  {
    // return machine-friendly output:
  
    $tid = lime2node_get_last_transaction_id_and_advance();
    $result = lime2node_send_spi_cmd($noop_cmd, $tid, "0"); // cmdParameter does not actually matter
  
    if ($result["valid"])
    {
      $received_ack = lime2node_wait_for_ack($tid);
      if ($received_ack["valid"])
        lime2node_write_log("ALERT", (string)(lime2node_get_battery_level_percentage($received_ack["batteryRead"])));
    }
    else {
      lime2node_write_log("INFO", "-1");
    }
  }
  else
  {
    // basic TURNON/TURNOFF/NOOP/STATUS command
    
    if ($cmd_to_send == $status_cmd)
    {
        // override user-provided TID and cmd parameter: STATUS command is special and wants
        // fixed value for TID and cmd param:
        $tid = $tid_for_status_cmd;
        $cmdParameter = $cmdparam_for_status_cmd;
    }
    else
    {
        $tid = lime2node_get_last_transaction_id_and_advance();
    }
    
    lime2node_write_log("INFO", "Sending $cmd_to_send command (with param=$cmdParameter and tid=$tid) to remote node...");
    $result = lime2node_send_spi_cmd($cmd_to_send, $tid, $cmdParameter);
  
    if ($result["valid"])
    {
      lime2node_write_log("INFO", "Command was sent successfully over SPI. Waiting for the ACK from the remote node...");
  
      $received_ack = lime2node_wait_for_ack($tid);
      if ($received_ack["valid"])
        lime2node_write_log("INFO", "Successfully received the ACK from the remote node! " . lime2node_get_battery_info($received_ack["batteryRead"]) );
      else
        lime2node_write_log("INFO", "Failed waiting for the ACK.");
    }
    else {
      lime2node_write_log("INFO", "Command TX over SPI failed. Aborting.");
    }
  }
  
  lime2node_write_log("INFO", "PHP Lime2Node backend: command sequence completed. Exiting.");
?>
