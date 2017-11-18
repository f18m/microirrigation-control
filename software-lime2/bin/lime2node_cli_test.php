#!/usr/bin/php    
<?php

  // 
  // lime2node_cli_test.php
  // Small utility to test the PHP library for over-SPI communication
  // part of the https://github.com/f18m/microirrigation-control github project
  //
  // Author: Francesco Montorsi
  // Creation date: Nov 2017
  //
  
  include 'lime2node_comm_lib.php';
  
  // constants
  
  $cli_command_lockfile="/tmp/lime2node_cli.lock";
  
  
  // utility class:
  
  class FileLocker {
      protected static $loc_files = array();
  
      public static function lockFile($file_name, $wait = false) {
          $loc_file = fopen($file_name, 'c');
          if ( !$loc_file ) {
              throw new \Exception('Can\'t create lock file!');
          }
          if ( $wait ) {
              $lock = flock($loc_file, LOCK_EX);
          } else {
              $lock = flock($loc_file, LOCK_EX | LOCK_NB);
          }
          if ( $lock ) {
              self::$loc_files[$file_name] = $loc_file;
              fprintf($loc_file, "%s\n", getmypid());
              return $loc_file;
          } else if ( $wait ) {
              throw new \Exception('Can\'t lock file!');
          } else {
              return false;
          }
      }
  
      public static function unlockFile($file_name) {
          fclose(self::$loc_files[$file_name]);
          @unlink($file_name);
          unset(self::$loc_files[$file_name]);
      }
  } 

  
  
  // main:
  
  
  // ensure only one turnon/turnoff command can be running at any given time:
  if ( !FileLocker::lockFile($cli_command_lockfile) ) {
      echo "Can't lock file $cli_command_lockfile: another operations is ongoing. Aborting.\n";
      die();
  }

  lime2node_empty_log();
  lime2node_write_log("PHP Lime2Node backend: acquired lock file $cli_command_lockfile... proceeding with command: " . $argv[1]);
  lime2node_init_over_spi();
  
  // for testing purpose: send TURNON command
  $tid = lime2node_get_last_transaction_id_and_advance();
  $ack_contents = array();
  if ($argv[1] == "TURNON")
    $result = lime2node_send_spi_cmd($turnon_cmd, $tid);
  else
    $result = lime2node_send_spi_cmd($turnoff_cmd, $tid);
    
  if ($result["valid"])
    lime2node_wait_for_ack($tid);

  lime2node_write_log("PHP Lime2Node backend: command sequence completed. Exiting.");
?>

