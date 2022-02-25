#! /bin/sh

wget https://pypi.python.org/packages/0a/da/9f61d28a20c42b4963334efacfd257c85150ede96d0cd2509b37da69da47/bitarray-0.8.1.tar.gz
tar xf bitarray-0.8.1.tar.gz -C .
cd bitarray-0.8.1
sudo python3 setup.py install

 wget https://pypi.python.org/packages/1f/3b/ee6f354bcb1e28a7cd735be98f39ecf80554948284b41e9f7965951befa6/pyserial-3.2.1.tar.gz#md5=7142a421c8b35d2dac6c47c254db023d
 tar xf pyserial-3.2.1.tar.gz -C .
 cd pyserial-3.2.1
 sudo python3 setup.py install

