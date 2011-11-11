CXX=g++
INSTALL=install
#CFLAGS=-Wall -O3
CFLAGS=-Wall -g
CFLAGS+=-I/usr/include/httpd/ -I/usr/include/apr-1/
CFLAGS+=-shared -fPIC -lstdc++

TARGET=mod_unlinkfile.so
TARGETDIR=/usr/lib/httpd/modules/

SRC=mod_unlinkfile.cpp
HEADERS=



all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(SRC)

install: $(TARGET)
	sudo $(INSTALL) $(TARGET) $(TARGETDIR)

clean:
	@rm -f $(TARGET)


#--- for debug targets ---
APACHECTL=apachectl
TESTCONF=unlink.conf
TESTCONFDIR=/etc/httpd/conf.d/
TESTCGI=unlink.cgi
TESTCGIDIR=/var/www/cgi-bin/

configtest:
	$(APACHECTL) -t

start:
	@echo -n "Starting apache..."
	@sudo $(APACHECTL) -k start
	@echo "done."

stop: __stop __wait

reload: stop start

update: stop all __install start

test:
	@sudo $(INSTALL) t/$(TESTCGI) $(TESTCGIDIR)
	@rm -f recvdata
	@curl -D /dev/tty -s localhost/cgi-bin/$(TESTCGI) > recvdata && ls -l recvdata
	@md5sum recvdata ~/Downloads/ruby-1.9.2-p180.tar.bz2

debug: stop install
	sudo gdb /usr/sbin/httpd

__stop:
	@echo -n "Stopping apache..."
	@sudo $(APACHECTL) -k stop
	@echo "done."

__wait:
	@HTTPD=`$(APACHECTL) 2>&1 |head -1|awk '{ print $$2 }'`; \
	while [ -n "`pgrep -f $$HTTPD`" ]; do sleep 1; done

__install:
	@sudo make install
	sudo $(INSTALL) -m 644 t/$(TESTCONF) $(TESTCONFDIR)
