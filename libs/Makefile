build:
	$(MAKE) -C prelude build
	$(MAKE) -C base build
	$(MAKE) -C contrib build
	$(MAKE) -C effects build
	$(MAKE) -C pruviloj build
	$(MAKE) -C freertos build


install:
	$(MAKE) -C prelude install
	$(MAKE) -C base install
	$(MAKE) -C contrib install
	$(MAKE) -C effects install
	$(MAKE) -C pruviloj install
	$(MAKE) -C freertos install

clean:
	$(MAKE) -C prelude clean
	$(MAKE) -C base clean
	$(MAKE) -C contrib clean
	$(MAKE) -C effects clean
	$(MAKE) -C pruviloj clean
	$(MAKE) -C freertos clean

doc:
	$(MAKE)	-C prelude doc
	$(MAKE)	-C base doc
	$(MAKE)	-C contrib doc
	$(MAKE)	-C effects doc
	$(MAKE)	-C pruviloj doc
	$(MAKE)	-C freertos doc

doc_clean:
	$(MAKE)	-C prelude doc_clean
	$(MAKE)	-C base doc_clean
	$(MAKE)	-C contrib doc_clean
	$(MAKE)	-C effects doc_clean
	$(MAKE)	-C pruviloj doc_clean
	$(MAKE)	-C freertos doc_clean

.PHONY: build install clean doc doc_clean
