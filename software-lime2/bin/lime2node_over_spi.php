<?php

  // constants
  $transaction_file = 'last_spi_transaction_id';
  $output_file = '/tmp/last_spi_reply';
  
  $max_wait_time_sec = 30;
  $speed_hz = 5000;
  $validack = 'ACK_';
  $turnon_cmd = 'TURNON_';
  $turnoff_cmd = 'TURNOFF';
  $status_cmd = 'STATUS_';
  
  // since the TID is sent over commandline it should stay inside the ASCII range:
  $tid_for_status_cmd = 48;    // '0'
  $first_valid_tid = 49;    // '1'
  $last_valid_tid = 57;     // '9'
  
  
  function init_lime2node_over_spi()
  {
    /* Fread is binary-safe IF AND ONLY IF you don't use magic-quotes. 
       If you do, all null bytes will become \0, and you might get surprising results when unpacking.  */
    //set_magic_quotes_runtime(0);  //only needed with PHP < 5.3.0
  }

  function trim_nulls($content_arr)
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

  function send_spi_cmd($cmd, $transactionID, $ack_contents)
  {
    global $output_file, $speed_hz;
    
    echo "Sending command over SPI:" . $cmd . " with transaction ID=" . $transactionID . "\n";
    
    $rawcommand = $cmd . chr($transactionID);
    $command = 'sudo spidev_test -D /dev/spidev2.0 -s ' . strval($speed_hz) . ' -v -p "' . $rawcommand . '" --output ' . $output_file;
    echo $command . "\n";
    
    exec($command, $output, $retval);
    if ($retval != 0)
    {
      echo "Failed sending command... spidev_test exited with " . $retval . "... output: " . implode("\n", $output) . "\n";
      return FALSE;
    }
    echo implode("\n", $output) . "\n";
    
    // read output reply from SPI
    $handle = fopen($output_file, "rb");
    $content_str = fread($handle, filesize($output_file));
    fclose($handle);
    
    // transform in binary array of uint8
    $content_arr = unpack("C*", $content_str);
    $content_arr = trim_nulls($content_arr);
    
    echo "Received a reply over SPI that is " . count($content_arr) . "B long\n";
    //print_r($content_arr);
    
    $ack_contents = $content_arr;
    return TRUE;
  }
  
  function is_valid_ack($cmdreply)
  {
    global $validack;
    $validack_arr = array_values(unpack("C*", $validack));
    
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
  
  function wait_for_ack($transactionID)
  {
    global $status_cmd, $tid_for_status_cmd, $max_wait_time_sec;
  
    // ignore the result of the first STATUS command: it's crap related to the command before the last sent command!!
    $ack = array();
    if (!send_spi_cmd($status_cmd, $tid_for_status_cmd, $ack))
      return FALSE;
    $ack = array();
    
    $waited_time_sec = 0;
    while (count($ack)==0)    // NULL replies immediately after a command mean that the radio is still estabilishing connection with the "remote" node
    {
      sleep(3);
      if (!send_spi_cmd($status_cmd, $tid_for_status_cmd, $ack))
        return FALSE;
      
      if ($waited_time_sec > $max_wait_time_sec)
      {
        echo "After " . $waited_time_sec . "secs still no valid ACK received. Aborting.\n";
        break;
      } 
      $waited_time_sec = $waited_time_sec + 3;
    }
    
    $result = is_valid_ack($ack);
    if ($result["valid"]==1 && $result["transactionID"]==$transactionID)
    {
      echo "Received valid ACK; last ACK'ed transaction ID=" . $result["transactionID"] . ", battery read=" . $result["batteryRead"] . "\n";
      return TRUE;
    }
    
    echo "Invalid ACK received (waiting for the ACK of transaction ID=" . $transactionID . " but the last ACK'ed command had transaction ID=" . $result["transactionID"] . ")!";
    return FALSE;
  }
  
  function get_last_transaction_id_and_advance()
  {
    global $transaction_file, $first_valid_tid, $last_valid_tid;
    
    $homedir = getenv("HOME");
    if (count($homedir)==0 || !file_exists($homedir))
      $homedir = "/tmp";
    
    $transaction_file = $homedir . "/" . $transaction_file;
    
    $str = file_get_contents($transaction_file);
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
?>

