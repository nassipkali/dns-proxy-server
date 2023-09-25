# dns-proxy-server
DNS proxy server

#Example of usage
Build:
```sh
make
```
Example of config file:
dns_proxy_config.txt
```
upstream_dns_server = 172.25.112.1
blacklist_file = blacklist.txt
default_response = miss
```
Example of blacklist file:
blacklist.txt
```
yandexru
googlecom
```
