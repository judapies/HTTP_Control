# HTTP_Control
Control Infrarrojo a traves de HTTP

El codigo anterior esta basado en la libreria IR de markszabo, la cual se encuentra en el siguiente link https://github.com/markszabo/IRremoteESP8266 y fue basado tambien en https://github.com/krzychb/OnlineHumidifier/tree/master/A3-HTTP, basado en estas dos librerias se construyo un sistema basado en el ESP01, para realizar el control de dispositivos a traves de comandos HTTP.

# Algo del codigo

**Una vez compile y cargue el programa en el ESP01, usted debra escribir en su navegador la dirección IP establecida en el codigo.**

>wifiManager.setSTAStaticIPConfig(IPAddress(192,168,100,80), IPAddress(192,168,100,0), IPAddress(255,255,255,0));

Una vez ingresada la IP le aparecera una imagen como la siguiente:

 [2]: https://www.codecademy.com/tracks/web

