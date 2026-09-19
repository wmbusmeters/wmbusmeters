

test_iperl_discovery:
	@xmq iperl/telegram.json transform --stringparam="drv=../src/iperl.xmq" ../to-ha-discovery.xslq to-json | jq . > generated.test.json
	@diff generated.test.json iperl/discover_an_iperl_meter.json ; if [ "$$?" = "0" ]; then echo "OK: iperl discovery"; else echo "ERROR: iperl discovery"; fi


test_kamwater_discovery:
	@xmq kamwater/telegram.json transform --stringparam="drv=../src/kamwater.xmq" ../to-ha-discovery.xslq to-json | jq . > generated.test.json
	@diff generated.test.json kamwater/discover_a_kamwater_meter.json ; if [ "$$?" = "0" ]; then echo "OK: kamwater discovery"; else echo "ERROR: kamwater discovery"; fi

prep_iperl_telegram:
	../../build/wmbusmeters --telegramdetails=first --format=json 1844AE4C4455223368077A55000000_041389E20100023B0000 MyMeter iperl 33225544 NOKEY | jq . > iperl/telegram.json

prep_kamwater_telegram:
	../../build/wmbusmeters --telegramdetails=first --format=json 4444372C834208543C168D209B836F9820755878_02FF200000041362ED0000441334C90000426C4137523B3303615B1161670F5167178101E7FF0F1606FF1B077000077000 MyMeter kamwater 54084283 NOKEY | jq . > kamwater/telegram.json
