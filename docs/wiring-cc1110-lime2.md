# Wiring of SPI bus between Lime2 node and CC1110 board #

The USART #0 of the CC1110 is exposed on some pins of its EVM board:

| SPI pin name  | CC1110 EVM pin  |
|---------------|-----------------|
| MISO          | P0.2            |
| MOSI          | P0.3            |
| SSN           | P0.4            |
| SCK           | P0.5            |

And for the Lime2 SPI#2 (in my opinion the easiest one to use) the pin mapping 
(referring to the layout mode "A") is:

| SPI pin name  | Processor pin | GPIO1 connector pin |
|---------------|---------------|---------------------|
| MISO          | PC22          | pin#39              |
| MOSI          | PC21          | pin#37              |
| SSN           | PC19          | pin#33              |
| SCK           | PC20          | pin#35              |

Note that all pins are located on the "GPIO-1 (General Purpose Input/Output) 40pin connector".

