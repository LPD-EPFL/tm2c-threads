PAPER=iRCCE_manual

default: latex

all: latex ps pdf

latex: $(PAPER).dvi


$(PAPER).dvi: *.tex
	latex $(PAPER) ; latex $(PAPER) ; bibtex $(PAPER) ; latex $(PAPER) ; latex $(PAPER)


ps: $(PAPER).ps

$(PAPER).ps: $(PAPER).dvi
	dvips -Pcmz $(PAPER)


pdf: $(PAPER).pdf

$(PAPER).pdf: $(PAPER).dvi
	dvipdf $(PAPER).dvi


clean:
	rm -rf *~ *.bak *.inx *.log *.aux *.bbl *.blg *.out
	rm -rf $(PAPER).dvi $(PAPER).ps $(PAPER).pdf

