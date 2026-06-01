/*!
    \file    readme.txt
    \brief   description of drive the GD30AD3344 for sampling

    \version 2024-10-08, V1.0.0, demosuites for GD30AD3344
*/

/*
    Copyright (c) 2020, GigaDevice Semiconductor Inc.

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this 
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission.
    4. The use of this software may or may not infringe the patent rights of one or 
       more patent holders. This license does not release you from the requirement 
       that you obtain separate licenses from these patent holders to use this software.
    5. Use of the software either in source or binary form, must be run on or directly 
       connected to an GigaDevice component.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*/

  This demo is based on the GD30AD3340_EVB board, it shows how to configure to drive 
the GD30AD3344 for sampling in continuous conversion mode and single-shot mode. 

  There is a brief description of the connection between GD30AD3344 and GD32F303CxTx.
For detailed information, please refer to the schematic of the host board (PAHBoard Rev.A)
and the GD30AD3344 EVB schematic:

        GD30AD3344                GD32F303CxTx        
          SCLK                    SPI0_CLK PA5        
          /CS                     SPI0_NSS PA4        
          DIN                     SPI0_MOSI PA7       
          DOUT                    SPI0_MISO PA6       

  Operating Methods:
  1. Selecting the power supply method for the host-board and the power supply range for 
     the GD30AD3344_EVB board. 
  2. Connected the source to the AIN0 channel.
  3. Download this software and press the RESET button on the host- board to start sampling.
  4. After the sampling is completed, view the data displayed on the serial port interface.
  5. Analyze the sampled data.

  NOTE: This software defaults to single-ended input and continuous mode to sampling signals 
        from AIN0, and the default sample count is 4096. For Single-shot mode or other functions,
        please refer to GD30AD3344 User Guide_Rev.A.
        1. Single-shot mode
        2. Continuous mode
        3. Channel switch read
        4. Programmable sampling rate
        5. Programmable input voltage range
        6. single-ended input or dual-ended input
        7. Analog input channel selection
        8. 16-bit transfer cycle and 32-bit transfer cycle

