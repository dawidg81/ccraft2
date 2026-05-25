Following ocurrencies of duplicate symbols found by linker:

* `levelRegistry` defined in commands.cpp, level.cpp and main.cpp, used in utils.cpp. It should exist in exactly one .cpp file and header file as `extern`.
* `commandHandler` defined in commands.cpp and main.cpp. Should be one extern definition (most likely in commands.cpp).
* `logger` object, defined in logger_instance.cpp and utils.cpp. There's already logger_instance.cpp. Make it `extern` in utils.cpp.
* `serverSocket` object, defined in main.cpp and utils.cpp. Should be left only in one of the .cpp files.
