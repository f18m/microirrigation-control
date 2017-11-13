all: install_ratchet

install_ratchet:
	cd bin && php -r "copy('https://getcomposer.org/installer', 'composer-setup.php');"
	cd bin && php composer-setup.php
	cd bin && php composer.phar require cboden/ratchet
