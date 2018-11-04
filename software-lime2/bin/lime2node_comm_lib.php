<?php

  //
  // lime2node_comm_lib.php
  // This is a library of PHP functions to be used on a Linux system to communicate over SPI with the "lime2" node
  // part of the https://github.com/f18m/microirrigation-control github project
  //
  // Author: Francesco Montorsi
  // Creation date: Nov 2017
  //


  // constants
  $transaction_filename = 'last_spi_transaction_id';        // this filename will be created under $HOME directory
  $output_file = '/tmp/last_spi_reply';
  $last_spi_op_logfile = '/var/log/lime2node_last_operation.log';
  $enabled_loglevel = "INFO";
  $spi_bus_lockfile = "/tmp/lime2node_spi_bus.lock";
  
  // SPI protocol details:
  $max_wait_time_sec = 30;
  $speed_hz = 5000;
  $validack = 'ACK_';
  
  // commands - the SPI/OtA protocol dictates a len of 7 bytes:
  $turnon_cmd  = 'TURNON_';
  $turnoff_cmd = 'TURNOFF';
  $noop_cmd    = 'NOOP___';
  $status_cmd  = 'STATUS_';

  // since the Transaction ID (TID) is sent over commandline it should stay inside the ASCII range:
  $tid_for_status_cmd = 48;    // '0'
  $first_valid_tid = 49;    // '1'
  $last_valid_tid = 57;     // '9'

  $cmdparam_for_status_cmd = '0';
  
  // BATTERY ADC->VOLTAGE CONVERSION FACTORS
  $battery_angular_coeff = 0.069;  // in V/ADC
  $battery_voltage_offset = 4.386; // in V
  $battery_max_voltage = 13.0; // in V
  

  //
  // LOGGING
  //

  function lime2node_set_logfile($newlogfile)
  {
    global $last_spi_op_logfile;
    $last_spi_op_logfile=$newlogfile;    // empty string is allowed and means stdout
    
    if ($last_spi_op_logfile != "" && $last_spi_op_logfile != "stdout")
      lime2node_empty_log();
    else
      echo "Selected stdout as logging mechanism.\n";
  }
  
  function lime2node_set_loglevel($newloglevel)  // string with loglevel 'ALERT' or 'INFO' or 'DEBUG'
  {
    global $enabled_loglevel;
    $enabled_loglevel=$newloglevel;
  }
  
  function lime2node_empty_log()
  {
    global $last_spi_op_logfile;
    
    if ($last_spi_op_logfile != "" && $last_spi_op_logfile != "stdout") {
      $f = @fopen($last_spi_op_logfile, "r+");
      if ($f !== false) {
          ftruncate($f, 0);
          fclose($f);
      }
    }
  }
  
  function lime2node_loglevel2number($level)
  {
    if ($level == "DEBUG") {
      return 1;
    } else if ($level == "INFO") {
      return 2;
    } else if ($level == "ALERT") {
      return 3;
    }
    return 2;    // if $level contains typos, default to INFO level 
  }

  function lime2node_write_log($level, $msg)
  {
    global $last_spi_op_logfile;
    global $enabled_loglevel;
    
    if (lime2node_loglevel2number($level) < lime2node_loglevel2number($enabled_loglevel))
      return;   // discard
    
    if ($last_spi_op_logfile != "" && $last_spi_op_logfile != "stdout") {
      file_put_contents($last_spi_op_logfile, $msg . "\n", FILE_APPEND | LOCK_EX);
    } else {
      // stdout was selected!
      echo $msg . "\n";
    }
  }

  function lime2node_get_logfile()
  {
    global $last_spi_op_logfile;
    return $last_spi_op_logfile;
  }

  function lime2node_get_loglevel()
  {
    global $enabled_loglevel;
    return $enabled_loglevel;
  }
  
  

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



  //
  // SPI COMMUNICATION HELPER FUNCTIONS
  //
  
  function lime2node_acquire_lock_or_die()
  {
    global $spi_bus_lockfile;
    // ensure only one turnon/turnoff command can be running at any given time:
    if ( !FileLocker::lockFile($spi_bus_lockfile) ) {
        echo "Can't lock file $spi_bus_lockfile: another operations is ongoing. Aborting.\n";    // this must be ECHOED!
        die();
    }
  }

  function lime2node_init_over_spi()
  {
    /* Fread is binary-safe IF AND ONLY IF you don't use magic-quotes.
       If you do, all null bytes will become \0, and you might get surprising results when unpacking.  */
    //set_magic_quotes_runtime(0);  //only needed with PHP < 5.3.0
  }

  function lime2node_trim_nulls($content_arr)
  {
    // Re-index to ensure zero-based indexing of keys:
    $content_arr = array_values($content_arr);

    // strip first NULL bytes if any
    while ( count($content_arr)>0 )
    {
      if ( $content_arr[0] == 0 )
      {
        unset($content_arr[0]); // This removes the element from the array

        // Re-index:
        $content_arr = array_values($content_arr);
      }
      else
        break;
    }

    // strip final NULL bytes if any
    while ( count($content_arr)>0 )
    {
      $last_chr_idx = count($content_arr) - 1;
      if ( $content_arr[$last_chr_idx] == 0 )
      {
        unset($content_arr[$last_chr_idx]); // This removes the element from the array

        // Re-index:
        $content_arr = array_values($content_arr);
      }
      else
        break;
    }

    return $content_arr;
  }

  function lime2node_assert_valid_cmd($cmd, $cmdParameter)
  {
    global $turnon_cmd, $turnoff_cmd, $noop_cmd, $status_cmd;
    if ($cmd != $turnon_cmd &&
        $cmd != $turnoff_cmd &&
        $cmd != $noop_cmd &&
        $cmd != $status_cmd) {
      lime2node_write_log("DEBUG", "Invalid command [$cmd]. Aborting.");
      die();
    }
    
    if ($cmd == $turnon_cmd ||
        $cmd == $turnoff_cmd)
    {
      if ($cmdParameter != "1" &&
          $cmdParameter != "2") {
        lime2node_write_log("DEBUG", "Invalid command parameter [$cmdParameter]. Aborting.");
        die();
      }
    }
  }
  
  function lime2node_send_spi_cmd($cmd, $transactionID, $cmdParameter)
  {
    global $output_file, $speed_hz;
    global $status_cmd, $tid_for_status_cmd, $cmdparam_for_status_cmd;

    lime2node_assert_valid_cmd($cmd, $cmdParameter);
    
    if ($cmd == $status_cmd)
    {
        // override user-provided TID and cmd parameter: STATUS command is special and wants
        // fixed value for TID and cmd param:
        assert($transactionID == $tid_for_status_cmd);
        assert($cmdParameter == $cmdparam_for_status_cmd);
    }

    lime2node_write_log("DEBUG", "Sending command over SPI:" . $cmd . " with transaction ID=" . $transactionID . " and parameter=" . $cmdParameter);
    $rawcommand = $cmd . chr($transactionID) . $cmdParameter;

    // NOTE: the "sudo" operation is required when running e.g. on a webserver that is not running as ROOT user:
    //       to be able to send/receive data over SPI, root permissions are needed.
    $command = 'sudo spidev_test -D /dev/spidev2.0 -s ' . strval($speed_hz) . ' -v -p "' . $rawcommand . '" --output ' . $output_file;
    lime2node_write_log("DEBUG", $command);

    exec($command, $output, $retval);
    if ($retval != 0)
    {
      lime2node_write_log("DEBUG", "Failed sending command... spidev_test exited with " . $retval . "... output: " . implode("\n", $output));
      $ret_array = array(
          "valid"  => FALSE,
          "ack" => array(),
      );
      return $ret_array;
    }
    lime2node_write_log("DEBUG", implode("\n", $output));

    // read output reply from SPI
    $handle = fopen($output_file, "rb");
    $content_str = fread($handle, filesize($output_file));
    fclose($handle);

    // transform in binary array of uint8
    $content_arr = unpack("C*", $content_str);
    $content_arr = lime2node_trim_nulls($content_arr);
    lime2node_write_log("DEBUG", "Received a reply over SPI that is " . count($content_arr) . "B long");
    //print_r($content_arr);

    $ret_array = array(
        "valid"  => TRUE,
        "ack" => $content_arr,
    );
    return $ret_array;
  }

  function lime2node_parse_ack($cmdreply)
  {
    global $validack;
    $validack_arr = array_values(unpack("C*", $validack));

    //print_r($cmdreply);
    $cmdreply_slice = array_slice($cmdreply, 0, 4);   // truncate stuff following the ACK_ string
    //print_r($cmdreply_slice);
    //print_r($validack_arr);

    $valid = FALSE;
    $transactionID = 0;
    $batteryRead = 0;
    if ($cmdreply_slice == $validack_arr)
    {
      $valid = TRUE;

      // after a valid ACK there are:
      // 1 byte of TRANSACTION ID
      // 1 byte of REMOTE BATTERY READ
      if (count($cmdreply)==6)
      {
        $transactionID = $cmdreply[4];
        $batteryRead = $cmdreply[5];
      }
      //else: the two final values were NULs trimmed away
    }

    $ret_array = array(
        "valid"  => $valid,
        "transactionID" => $transactionID,
        "batteryRead" => $batteryRead,
    );
    return $ret_array;
  }

  function lime2node_wait_for_ack($transactionID)
  {
    global $status_cmd, $tid_for_status_cmd, $max_wait_time_sec, $cmdparam_for_status_cmd;

    $invalid_ack_ret = array(
        "valid" => FALSE,
    );

    // ignore the result of the first STATUS command: it's crap related to the command before the last sent command!!
    $send_ret = lime2node_send_spi_cmd($status_cmd, $tid_for_status_cmd, $cmdparam_for_status_cmd);
    if (!$send_ret["valid"])
      // failed SPI transaction... something is really going bad
      return $invalid_ack_ret;

    //$ack = $send_ret["ack"];
    $waited_time_sec = 0;
    $valid_ack_ret = array(
        "transactionID" => 0,
    );
    while (TRUE) //count($ack)==0)    // NULL replies immediately after a command mean that the radio is still 
    {                                 // estabilishing connection with the "remote" node
      sleep(2);

      $send_ret = lime2node_send_spi_cmd($status_cmd, $tid_for_status_cmd, $cmdparam_for_status_cmd);
      if (!$send_ret["valid"])
        // failed SPI transaction... something is really going bad
        return $invalid_ack_ret;

      if ($waited_time_sec > $max_wait_time_sec)
      {
        lime2node_write_log("DEBUG", "After " . $waited_time_sec . "secs still no valid ACK received. Aborting.");
        break;
      }
      $waited_time_sec = $waited_time_sec + 3;

      $ack = $send_ret["ack"];
      $valid_ack_ret = lime2node_parse_ack($ack);
      if ($valid_ack_ret["valid"]==1)
      {
        if ($valid_ack_ret["transactionID"]==$transactionID)
        {
          lime2node_write_log("DEBUG", "Received valid ACK; last ACK'ed transaction ID=" . $valid_ack_ret["transactionID"] 
                                        . ". " . lime2node_get_battery_info($valid_ack_ret["batteryRead"]));
          return $valid_ack_ret; 
        }
        else
        {
          lime2node_write_log("DEBUG", "Received ACK for a previous transaction ID " . $valid_ack_ret["transactionID"] 
                                        . " while waiting for ACK of ID " . $transactionID);
        }
      }
    }

    lime2node_write_log("DEBUG", "Invalid ACK received (waiting for the ACK of transaction ID=" . $transactionID .
                        " but the last ACK'ed command had transaction ID=" . $valid_ack_ret["transactionID"] . ")!");
    return $invalid_ack_ret;
  }

  function lime2node_get_last_transaction_id_and_advance()
  {
    global $transaction_filename, $first_valid_tid, $last_valid_tid;

    $homedir = getenv("HOME");
    if (count($homedir)==0 || !file_exists($homedir))
      $homedir = "/tmp";

    // load last TID from file:
    $transaction_file = $homedir . "/" . $transaction_filename;
    if (file_exists($transaction_file))
      $str = file_get_contents($transaction_file);
    else
      $str = 0;

    $ret_tid = intval($str)+1;

    // since the TID is sent over commandline it should stay inside the ASCII range:
    if ($ret_tid < $first_valid_tid)
      $ret_tid=$first_valid_tid;    // reset transaction ID so that it always stays inside an uint8_t
    if ($ret_tid > $last_valid_tid)
      $ret_tid=$first_valid_tid;    // reset transaction ID so that it always stays inside an uint8_t

    // advance TID
    file_put_contents($transaction_file, $ret_tid);

    return $ret_tid;
  }
  
  function lime2node_get_battery_level($batteryRead_adc_counts)
  {
    global $battery_angular_coeff, $battery_voltage_offset;
    
    // simple basic linear fitting :)
    $battery_voltage = $battery_angular_coeff * $batteryRead_adc_counts + $battery_voltage_offset;

    // human-friendly formatting:
    return $battery_voltage;
  }
  
  function lime2node_get_battery_level_percentage($batteryRead_adc_counts)
  {
    global $battery_max_voltage;
    $battery_voltage = lime2node_get_battery_level($batteryRead_adc_counts);
    return 100.0 * ($battery_voltage / $battery_max_voltage);
  }
  
  function lime2node_get_battery_info($batteryRead_adc_counts)
  {
    $battery_voltage = lime2node_get_battery_level($batteryRead_adc_counts);

    // human-friendly formatting:
    return "Battery read is " . $battery_voltage . "V (" .  $batteryRead_adc_counts . " ADC counts)";
  }
?>
