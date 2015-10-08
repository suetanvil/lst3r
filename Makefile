
all:
	(cd src && make all)

tags:
	(cd src && make tags)

clean:
	(cd src && make clean)
	rm -f lst3 buildImage systemImage



# install:
# 	mkdir -p /usr/share/lst3
# 	cp systemImage /usr/share/lst3
# 	cp lst3 /usr/bin
