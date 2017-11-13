#!/usr/bin/php    
<?php

  include 'lime2node_over_spi.php';
  
  init_lime2node_over_spi();
  
  // for testing purpose: send TURNON command
  $tid = get_last_transaction_id_and_advance();
  $ack_contents = array();
  if (send_spi_cmd($turnon_cmd, $tid, $ack_contents))
    wait_for_ack($tid);
  /*
  echo "Sleeping before sending turn off command...\n";
  sleep(5);
  
  // then send a TURNOFF command
  $tid = get_last_transaction_id_and_advance();
  $ack = send_spi_cmd($turnoff_cmd, $tid);
  wait_for_ack($tid);*/
?>

