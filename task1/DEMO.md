# Демо работы

## Работа с чатами через чтение/запись
```
-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- ll /dev/telegram 
total 0
crw------- 1 root root 243, 1 May  8 01:37 chat0
crw------- 1 root root 243, 2 May  8 01:37 chat1
crw------- 1 root root 243, 3 May  8 01:37 chat2
crw------- 1 root root 243, 0 May  8 01:37 create_chat

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- echo "message0" | sudo tee /dev/telegram/chat0 > /dev/null

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0                              
[01:38:42] message0

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- echo "message1" | sudo tee /dev/telegram/chat0 > /dev/null

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0                              
[01:38:42] message0
[01:38:49] message1
```

## Независимость чатов
(На примере после команд выше)
```
-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- echo "message2" | sudo tee /dev/telegram/chat1 > /dev/null

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- echo "message3" | sudo tee /dev/telegram/chat2 > /dev/null

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat2                              
[01:41:09] message3

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat1
[01:41:01] message2

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0
[01:38:42] message0
[01:38:49] message1
```

## Работа с control device
### Чтение списка чатов
```
-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<1:%>- sudo ./bin/tgctl list
chat0
chat1
chat2
```

### Создание новых чатов
```
-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- ll /dev/telegram
total 0
crw------- 1 root root 243, 1 May  8 01:37 chat0
crw------- 1 root root 243, 2 May  8 01:37 chat1
crw------- 1 root root 243, 3 May  8 01:37 chat2
crw------- 1 root root 243, 0 May  8 01:37 create_chat

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- echo "named3" | sudo tee /dev/telegram/create_chat > /dev/null 

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- echo "" | sudo tee /dev/telegram/create_chat > /dev/null     

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- ll /dev/telegram              
total 0
crw------- 1 root root 243, 1 May  8 01:37 chat0
crw------- 1 root root 243, 2 May  8 01:37 chat1
crw------- 1 root root 243, 3 May  8 01:37 chat2
crw------- 1 root root 243, 5 May  8 01:44 chat4
crw------- 1 root root 243, 0 May  8 01:37 create_chat
crw------- 1 root root 243, 4 May  8 01:43 named3
```

## Работа с пользовательским приложением
### Control интерфейс
```
-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<130:%>- sudo ./bin/tgctl list  
chat0
chat1
chat2
named3
chat4

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl create named5 
named5

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl create       
chat6

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl list         
chat0
chat1
chat2
named3
chat4
named5
chat6
```

### Интерфейс чата
```
-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0
[01:38:42] message0
[01:38:49] message1
[01:48:44] message0
[01:48:45] message0
[01:48:48] message0

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl count /dev/telegram/chat0               
5

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl get-limit /dev/telegram/chat0
10

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl set-limit /dev/telegram/chat0 2
read limit set to 2

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0              
[01:48:45] message0
[01:48:48] message0

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo ./bin/tgctl clear /dev/telegram/chat0
chat cleared

-<dev@debian-dev:/mnt/kernel-module-dev>-                                                                                                              -<S0>-
-<%>- sudo cat /dev/telegram/chat0
```
