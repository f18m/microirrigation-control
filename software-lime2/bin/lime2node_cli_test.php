#!/usr/bin/php    
<?php

  include 'lime2node_over_spi.php';
  
  lime2node_init_over_spi();
  
  // for testing purpose: send TURNON command
  $tid = lime2node_get_last_transaction_id_and_advance();
  $ack_contents = array();
  $result = lime2node_send_spi_cmd($turnon_cmd, $tid);
  if ($result["valid"])
    lime2node_wait_for_ack($tid);
  /*
  echo "Sleeping before sending turn off command...\n";
  sleep(5);
  
  // then send a TURNOFF command
  $tid = get_last_transaction_id_and_advance();
  $ack = send_spi_cmd($turnoff_cmd, $tid);
  wait_for_ack($tid);*/
?>

