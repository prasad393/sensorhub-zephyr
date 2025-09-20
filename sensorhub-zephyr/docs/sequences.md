# Sequences

## Read once
```
user -> shell: sensor read temp
shell -> drivers: vs_temp_read()
drivers -> shell: value
shell -> user: print °C
```

## Streaming path
```
sensor_thread -> msgq: enqueue sample
msgq -> logger_thread: dequeue sample
logger_thread -> console: print (rate/drops)
logger_thread -> mqtt (optional): publish JSON
```
