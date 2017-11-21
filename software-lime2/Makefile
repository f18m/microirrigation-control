current_dir = $(shell pwd)

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

install-systemd:
        # IMPORTANT: apparently symlinks do not work well with SystemD, so we must copy files;
        #            if we don't do that, we will be able to start/stop services but not to enable them on boot!
	cp -f $(current_dir)/etc/system.d/websocketsrv.service /lib/systemd/system/
	systemctl daemon-reload
	systemctl enable websocketsrv   # start it on boot


