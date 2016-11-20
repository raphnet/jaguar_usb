all:
	@echo "You must use Makefile.atmega8 or Makefile.atmega168"

clean:
	$(MAKE) -f Makefile.atmega8 clean
	$(MAKE) -f Makefile.atmega168 clean
