VERSION=0.01.05

CFLAGS += -Wall -Wextra -DVERSION='"$(VERSION)"'
LDFLAGS += -lpthread

BINDIR=/usr/bin
MANDIR=/usr/share/man/man8

OBJS = list.o pid.o proc.o syscall.o timeval.o fnotify.o event.o cpustat.o mem.o health-check.o

health-check: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

health-check.8.gz: health-check.8
	gzip -c $< > $@

dist:
	git archive --format=tar --prefix="health-check-$(VERSION)/" V$(VERSION) | \
		gzip > health-check-$(VERSION).tar.gz

clean:
	rm -f health-check health-check.o health-check.8.gz
	rm -f health-check-$(VERSION).tar.gz
	rm -f $(OBJS)

install: health-check health-check.8.gz
	mkdir -p ${DESTDIR}${BINDIR}
	cp health-check ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MANDIR}
	cp health-check.8.gz ${DESTDIR}${MANDIR}
