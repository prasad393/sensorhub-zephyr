# Sequences

## Read once
```
user -> shell: sensor read temp
shell -> drivers: vs_temp_read()
drivers -> shell: value
shell -> user: print °C
```

## Change the rate
```
user -> shell: rate set 250
shell: parse "250" with shell_strtol(), check 10..500
shell -> sensorhub: sensorhub_set_rate(250)        (atomic store)
shell -> user: ok rate=250 Hz
sensor_thread: next period uses 1000000 / 250 = 4000 us
logger_thread -> console: rate=250Hz rx=250 ...   (one second later)
```

## Streaming path
```
sensor_thread -> drivers: vs_temp_read(), vs_imu_read()
sensor_thread -> msgq: sensorhub_enqueue(sample)
  [queue full] msgq -> sensor_thread: oldest sample discarded, drops += 1
msgq -> logger_thread: sensorhub_dequeue(sample)
logger_thread -> mqtt (optional): mqtt_bridge_forward(sample)
logger_thread -> console: rate, rx, q, hwm, drops, idle (once per second)
```

## Statistics
```
sensor_thread: after each enqueue, under stats_lock: drops += n, hwm = max(hwm, depth)
user -> shell: stats
shell -> sensorhub: sensorhub_get_qstats()         (snapshot under stats_lock)
shell -> user: q=depth/capacity hwm=… drops=… idle=…% rate=…Hz
```
