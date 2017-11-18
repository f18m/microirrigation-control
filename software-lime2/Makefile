all: restart_websocket_srv

start_websocket_srv:
	php bin/lime2node_websocket_srv.php &

stop_websocket_srv:
	kill $(shell pgrep -f lime2node_websocket_srv.php)

restart_websocket_srv:
	-kill $(shell pgrep -f lime2node_websocket_srv.php)
	php bin/lime2node_websocket_srv.php &

install_ratchet:
	cd bin && php -r "copy('https://getcomposer.org/installer', 'composer-setup.php');"
	cd bin && php composer-setup.php
	cd bin && php composer.phar require cboden/ratchet
