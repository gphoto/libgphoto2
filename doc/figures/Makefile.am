# $Id$

CLEANFILES = *.pdf *.eps *.ps *.png *.jpg

EXTRA_DIST = architecture.fig libgphoto2-api.fig libgphoto2-camlib.fig libgphoto2_port.fig gphotofs.fig

figuredir = @HTML_DIR@/figures

if ENABLE_FIGURES

figure_DATA = \
	architecture.png libgphoto2-api.png libgphoto2-camlib.png libgphoto2_port.png gphotofs.png

# we are currently not using either of these
#noinst_DATA = \
#	architecture.eps libgphoto2-api.eps libgphoto2-camlib.eps libgphoto2_port.eps gphotofs.eps \
#	architecture.jpg libgphoto2-api.jpg libgphoto2-camlib.jpg libgphoto2_port.jpg gphotofs.jpg#

%.eps: %.fig
	fig2dev -L eps $< $@

%.tex: %.fig
	fig2dev -L latex $< $@

%.ps: %.fig
	fig2dev -L ps -z A4 -c $< $@

%.pdf: %.fig
	fig2dev -L pdf -z A4 -p dummy -c $< $@

%.png: %.fig
	fig2dev -L png -m 1.5 $< $@

%.jpg: %.fig
	fig2dev -L jpeg -m 1.5 $< $@

endif
