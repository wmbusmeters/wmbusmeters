

test_iperl_discovery:
	@xmq iperl/telegram.json transform --stringparam=drv=../src/iperl.xmq ../to-ha-discovery.xslq to-json | jq . > generated.test.json
	@diff generated.test.json iperl/discover_an_iperl_meter.json ; if [ "$$?" = "0" ]; then echo "OK: iperl discovery"; else echo "ERROR: iperl discovery"; fi


test_kamwater_discovery:
	@xmq kamwater/telegram.json transform --stringparam=drv=../src/kamwater.xmq ../to-ha-discovery.xslq to-json | jq . > generated.test.json
	@diff generated.test.json kamwater/discover_a_kamwater_meter.json ; if [ "$$?" = "0" ]; then echo "OK: kamwater discovery"; else echo "ERROR: kamwater discovery"; fi
