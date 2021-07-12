# server-ntimes

n回write()してしばらく休んで、またn回writeするサーバー。

```
% ./server-ntimes -h
Usage: server [-b bufsize (1460)] [-r rate]
-b bufsize:    one send size (may add k for kilo, m for mega)
-p port:       port number (1234)
-N:            set TCP_NODELAY
-i interval_sec: default 1 seconds
-s sleep_usec:   sleep usec between each write()'s
-n write_count:  write() n times.  Default 10
```

デフォルトは10回データを送り、しばらく休止するのを
-i interval_secごとに行う。

```
 <-------------- interval sec ------------->
 +----------+                              +------
 ||||||||||||                              ||||||
_||         |______________________________||||||
------------------------------------------------------------> t
 <---------->                              <--------->
  n回write()                                n回write()
```
