# LED Matrix Live Coding

Clone:
```
git clone --recursive https://github.com/jdhagood/Raspberry_Pi_LED_Matrix_Live_Coding.git
```

The control of the HUB75E LED pannels is done with the [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix.git) repo.



## Commands

In one terminal start the daemon
```
source venv/bin/activate



cd ~/led_matrix_live_coding
sudo ./bin/matrix_daemon \
  --led-rows=64 \
  --led-cols=64 \
  --led-chain=1 \
  --led-parallel=1 \
  --led-gpio-mapping=regular \
  --led-slowdown-gpio=3 \
  --led-pwm-bits=8 \
  --led-brightness=60
```


In another terminal start the website
```
cd ~/Raspberry_Pi_LED_Matrix_Live_Coding/
source venv/bin/activate
python web/server.py
```
