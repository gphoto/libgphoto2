# $Id$

CLEANFILES = *.pdf *.eps *.ps *.png *.jpg

EXTRA_DIST = architecture.fig libgphoto2-api.fig libgphoto2-camlib.fig libgphoto2_port.fig

if BUILD_DOC_FIGURES
all-local: \
	architecture.jpg libgphoto2-api.jpg libgphoto2-camlib.jpg libgphoto2_port.jpg \
 	architecture.png libgphoto2-api.png libgphoto2-camlib.png libgphoto2_port.png \
	architecture.pdf libgphoto2-api.pdf libgphoto2-camlib.pdf libgphoto2_port.pdf \
	architecture.ps libgphoto2-api.ps libgphoto2-camlib.ps libgphoto2_port.ps \
	architecture.eps libgphoto2-api.eps libgphoto2-camlib.eps libgphoto2_port.eps
endif

%.eps: %.fig
	fig2dev -L eps $< $@

%.ps: %.fig
	fig2dev -L ps -z A4 -c $< $@

%.pdf: %.fig
	fig2dev -L pdf -z A4 -p dummy -c $< $@

%.png: %.fig
	fig2dev -L png -m 1.5 $< $@

%.jpg: %.fig
	fig2dev -L jpeg -m 1.5 $< $@

%.pdf: %.ps
