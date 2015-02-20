rm bin/reader bin/device_driver2 bin/writer
ln -s /usr/local/eecs340/device_driver2 bin/
ln -s /usr/local/eecs340/reader bin/
ln -s /usr/local/eecs340/writer bin/

cd fifos/
chmod a+w ether2mon
chmod a+w ether2mux
cd ../
