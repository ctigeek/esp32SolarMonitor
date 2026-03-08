# esp32SolarMonitor

The esp is connected to three ADS1115 boards, which are voltage comparators/samplers. It communicates via an i2c bus.

![esp32board](https://github.com/user-attachments/assets/56281866-ec67-4564-8c42-553bc2590802)

The inputs of the ADS1115 are connected to the blue current-sensor clips, with a 30-1 ration.
![esp32breakerbox](https://github.com/user-attachments/assets/27e09412-c994-4b3e-9e7f-b891ec7f10ec)

![esp32programming](https://github.com/user-attachments/assets/726453ee-5383-4648-8a58-e35a8cd85528)

The data is processed and sent to Home Assistant.
![esp32HomeAssistant](https://github.com/user-attachments/assets/aeaaac0b-ec7d-429f-a806-6d916832f1b4)
