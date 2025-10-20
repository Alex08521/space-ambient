ENG

Space Ambient is a user service for KDE Plasma 6 that plays pre-installed music in the background on the desktop, automatically disabling it when other applications play audio. You can support me here https://boosty.to/alexpluz/donate

If the service did not start after installing the package, run the command
```
systemctl --user start space-ambient.service
```
To check the service status, run
```
systemctl --user status space-ambient.service
```

To add your own music, add it to the project /data directory in .ogg/.oga format and run build.sh

Enjoy using it!

РУС

Space Ambient это пользовательский сервис для KDE Plasma 6, который проигрывает предустановленную музыку фоном на рабочем столе, автоматически отключая её при воспроизведении звука другими приложениями. Поддержать меня можно здесь https://boosty.to/alexpluz/donate

Если сервис не запустился после установки пакета, нужно выполнить команду
```
systemctl --user start space-ambient.service
```
Для проверки статуса работы сервиса нужно выполнить
```
systemctl --user status space-ambient.service
```

Для добавления своей музыки нужно добавить её в формате .ogg/.oga в директорию проекта /data и запустить build.sh

Приятного использования!
