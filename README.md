# SPI Flash Analyzer Plugin

A SPI Flash Analyzer Plugin for Saleae Logic software.

## Features

- Support single, dual, and quad-line SPI mode
- Handles dynamic changing between single and quad or dual mode detected based on commands
- Decodes status register bit fields
- SPI0 and SPI3 mode
- Continuous read mode handling
- FrameV2 format supported
- The following manufacturers command sets are supported:
  - Winbond
  - Macronix
  - Renesas
  - GigaDevice
  - Adesto
  - Microchip
  - Micron
  - Cypress
  - Issi
  - Fujitsu

Capture showing JEDEC ID

![JEDEC ID](./img/jedec_id.png)

Capture showing Quad Read I/O

![Quad Read IO](./img/quad_read_io.png)

Capture showing simplified raw-data terminal output

![Terminal Output](./img/terminal_output.png)

Capture showing the decoded protocol table

![Terminal Output](./img/protocol_table.png)

Settings

![Settings](./img/settings.png)

## Known Issues

- The Saleae Logic framework does not officially support multiple bubble-text
  entires that overlap in time. The current chip select behavior breaks this rule
  and as a result causes some graphical anomalies in the rendering of the
  bubble-text. The primary observable issue is that some bubble text may not
  expand out to their more detailed representations. To resolve this a setting
  is added to enable/disable the command-summary feature that uses the
  chip-select line to provide a basic overview of the transaction. This feature
  is disabled by default.
- Rerunning the analyzer seems to generate an extra terminal output row for the
  final line. This may manifest itself in other locations. This issue has not
  been investigated in great detail yet to know if this is an issue of the
  Logic software itself or the plugin.

## Bug Reporting & Feature Requests

If you see a problem with how the plugin decodes data, is missing information,
or you would like to see support for another type of serial flash memory please
create a GitHub issue.

## Attribution

Much of this work was originally based on the `saleae_spi_flash` originally authored
Jerzy Kasenberg at https://github.com/kasjer/saleae_spiflash. The work done
in this project is very much appreciated and is deserving of recognition.
Additional contributors include: Jonathan Zentgraf and Ian Rees.

## License

Copyright © Jon Carrier

This project is licensed under the MIT license. For more details please refer to
[LICENSE](./LICENSE).
