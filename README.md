

![ESP01_100R_and_330R_bb](https://github.com/IoTDot/Alexa-IR-Bridge-OTA/assets/160416758/514fcdc0-d9aa-4099-8b5f-507045c2a05e)


![ESP01_100R_bb](https://github.com/IoTDot/Alexa-IR-Bridge-OTA/assets/160416758/bbe9dcb7-05ea-4043-b776-04fa19275057)



Warnings

Przy kompilacji kodu dla ESP32 mogą pojawić się ostrzeżenia w terminalu:

```jsx
/home/admin/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c: In function 'uartSetPins':
/home/admin/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c:153:9: warning: 'return' with no value, in function returning non-void
return;
^~~~~~
/home/admin/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c:149:6: note: declared here
bool uartSetPins(uint8_t uart_num, int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin)
```

![image](https://github.com/IoTDot/Alexa-IR-Bridge-OTA/assets/160416758/17a7001e-33aa-4416-8012-ee643c2af7c4)


a w zakładce Problems:

```jsx
[{
"resource": "/home/admin/Desktop/IOT_pilot/Alexa IR Bridge OTA/home/admin/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c",
"owner": "cpp",
"severity": 4,
"message": "'return' with no value, in function returning non-void",
"startLineNumber": 153,
"startColumn": 9,
"endLineNumber": 153,
"endColumn": 9
}]
```

![image](https://github.com/IoTDot/Alexa-IR-Bridge-OTA/assets/160416758/184e894d-76f6-4801-b678-1cd66e70a51e)

Ignorujemy je ponieważ nie dotyczą one naszego oprogramowania
