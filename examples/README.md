# MQTT + InfluxDB backend (Ubuntu)
```
curl -sL https://repos.influxdata.com/influxdb.key | sudo apt-key add -
source /etc/lsb-release
echo "deb https://repos.influxdata.com/ubuntu ${DISTRIB_CODENAME} stable" | sudo tee /etc/apt/sources.list.d/influxdb.list
sudo apt update
sudo apt-get install influxdb telegraf mosquitto mosquitto-clients
# bind to 127.0.0.1:8086 in /etc/influxdb/influxdb.conf
sudo systemctl start influxd
# create user, database and retention policies
# copy telegraph custom configuration
sudo systemctl start telegraf
```

```
wget -q -O - https://packages.grafana.com/gpg.key | sudo apt-key add -
sudo add-apt-repository "deb https://packages.grafana.com/oss/deb stable main"
sudo apt-get update
sudo apt-get install grafana
sudo /bin/systemctl daemon-reload
sudo /bin/systemctl enable grafana-server
sudo /bin/systemctl start grafana-server
```

### Troubleshooting
```
mosquitto_sub -t "#"
``` 
