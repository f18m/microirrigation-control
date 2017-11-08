#!/usr/bin/php    
<?php

  // constants
  $outputfile = '/tmp/last_spi_reply';


  function send_spi_cmd($cmd)
  {
    global $outputfile;
    
    echo "Sending command over SPI to get status:\n";
    $command = 'spidev_test -D /dev/spidev2.0 -s 3000 -v -p "' . $cmd . '" --output ' . $outputfile;
    $output = shell_exec($command);
    echo $output;
    
    // read output reply from SPI
    $handle = fopen($outputfile, "rb");
    $content_str = fread($handle, filesize($outputfile));
    fclose($handle);
    
    // transform in binary array of uint8
    $content_arr = unpack("C*", $content_str);
    
    // strip first NULL bytes if any
    while ( count($content_arr)>0 )
    {
      if ( $content_arr[1] == 0 )
      {
        unset($content_arr[1]); // This removes the element from the array
        
        // Re-index:
        $content_arr = array_values($content_arr);
      }
      else
        break;
    }
    
    echo "Received a reply over SPI that is " . count($content_arr) . "B long\n";
    print_r($content_arr);
    
    return $content_arr;
  }
  
  /* Fread is binary-safe IF AND ONLY IF you don't use magic-quotes. 
     If you do, all null bytes will become \0, and you might get surprising results when unpacking.  */
  set_magic_quotes_runtime(0); 

  $ack = send_spi_cmd('STATUS_0');
  
?>

