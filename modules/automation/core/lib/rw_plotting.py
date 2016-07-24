
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

'''
Description:    Misc constants, functions, classes for plotting in tests.
                This code was extracted from rw_common.py into this separate file.
Author:         Andrew Golovanov
Version:        1.0
Creation Date:  9/9/2014
Copyright:      RIFTio, Inc.
'''

import os, sys
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.colors as clr
from matplotlib.backends.backend_pdf import PdfPages

DBGMODE = '-DEBUG' in sys.argv

#--- PLOTTING FUNCTIONS

# Constants for graphics
colLst     = (
    'red', 'gray', 'fuchsia', 'blue', 'gold', 'green', 'hotpink',
    'firebrick', 'darkolivegreen', 'darkcyan', 'indigo')# list of preferred colors
colOpac    = 0.35                                       # color opacity
fSize      = 'xx-small'                                 # font size for inscription
docTit     = 0.45, 0.5                                  # document title block location
titBkgCol  = 'lightblue'                                # background color of the title inscription
titFntWght = 'bold'                                     # font weight of the title inscription
titFntSize = 'large'                                    # font size of the title inscription
mtrFntSize = 'medium'                                   # font size of the metrics inscription

def plotBar(barFile, docTitle, xLst, yLst, yLab1, yLab2, xRot=30, barBw=0.1, barNumb=8, capsize=2):
    '''
    This function plots a bar diagram with errors and pagination (if the X data is too long).

    Arguments:

    barFile - full pathname of the PDF file with the bar diagrams
    docTitle- document title
    xLst    - list of strings that are used as X-axis labels
    yLst    - three-dimensional list or tuple of stats that are used to depict Y-axis values and errors (3 dimensione
              means that every ultimate element can be accessed with 3 indexes):
                1st level - type of output metric (those will be shown on completely independent bar diagrams);
                            even if a single metric is plotted, this level of list must exist
                2nd level - represents individual bars in each group
                            even if a single bar in each group is plotted, this level of list must exist
                3rd level - just a list of 3-tuples (average, minimal, maximal stats counter); note that dimension
                            of this level must exactly match to xLst
    yLab1   - list or tuple of strings that are used as headers for the output metrics; the size of this list must
              match the dimension of the 1st level in yLst
    yLab2   - list or tuple of strings that are used as legends for the bars in every group; the size of this list
              st match the dimension of the 2nd level in yLst
    xRot    - rotation of X markers (integer degrees)
    barBw   - bar width (inch)
    barNumb - max number of bars in any bar group
    capsize - length of caps on the error bars
    '''
    errCfg  = {'ecolor' : '0.4', 'elinewidth' : '0.1'}   # default properties of error patterns

    # Create bar page
    barGw        = barBw * float(barNumb)             # group width
    bar          = plt.figure(1)                      # page for the bar diagrams
    barPageSizes = bar.get_size_inches()              # default sizes of the bar pages (inch)
    barPageDpi   = bar.get_dpi()                      # default dpi of the bar pages
    barNg        = int(barPageSizes[0] / barGw)       # number of bar groups per page
    xlen         = len(xLst)                          # number of X markers
    barNp        = int((xlen - 1) / barNg) + 1        # total number of pages (for each metric)
    color_names  = (int((len(yLab2) - 1) / len(colLst) + 1)) * colLst

    if DBGMODE:
        print('\n'.join((
            'PARAMETERS OF BAR DIAGRAM:',
            '\tpage sizes (inch):   {} x {} inch ({} x {})'.format(
                barPageSizes[0], barPageSizes[1], barPageDpi*barPageSizes[0], barPageDpi*barPageSizes[1]),
            '\tbar/group width:     {}/{}'                 .format(barBw, barGw),
            '\tmax groups per page: {}'                    .format(barNg),
            '\ttotal pages:         {}'                    .format(barNp),
            )))

    with PdfPages(barFile) as pdfBar:

        bar.text(
            docTit[0], docTit[1], docTitle,
            horizontalalignment='center', verticalalignment='center',
            backgroundcolor=titBkgCol, weight=titFntWght, size=titFntSize)
        pdfBar.savefig(bar)
        bar.clear()

        for y1Ind, y1Lbl in enumerate(yLab1):

            xInd    = 0
            pageInd = 0
            while True:        # pagination

                ng = min(barNg, xlen - xInd)   # actual number of groups per this page
                if ng <= 0: break
                ngAdd    = barNg - ng
                xIndMax  = xInd  + ng
                pageInd += 1

                for y2Ind, y2Lbl in enumerate(yLab2):

                    ypos = [barGw * x + barBw * y2Ind for x in range(barNg)]
                    tmp  = yLst[y1Ind][y2Ind][xInd:xIndMax] + ngAdd * [(0, 0, 0)]
                    xpos = [x[0] for x in tmp]
                    errs = list(zip(*[(x[0]-x[1], x[2]-x[0]) for x in tmp]))  # asymmetric errorbars

                    plt.bar(ypos, xpos, yerr=errs, width=barBw, color=clr.cnames[color_names[y2Ind]],
                        alpha=colOpac, label=y2Lbl, error_kw=errCfg, capsize=capsize)

                plt.xticks([barGw * x for x in range(barNg)], xLst[xInd:xIndMax] + ngAdd * [''],
                            rotation=xRot, size=fSize)
                plt.yticks(size=fSize)
                plt.title('{} (page {}/{})'.format(y1Lbl, pageInd, barNp), fontsize=mtrFntSize)
                plt.legend(bbox_to_anchor=(1.0, 1.10), fontsize=fSize, ncol=3)

                pdfBar.savefig(bar, bbox_inches='tight')
                #pdfBar.savefig(bar)
                bar.clear()
                xInd = xIndMax

def plotLine(lnFile, docTitle, xLst, yLst, yLab1, yLab2, xRot=30, lnWd=1.0, valign='top', halign='center', linecolors=[], xlabel=None, ylabel=None, y2LblFirst=False):
    '''
    This function plots a line diagram with min/max and no pagination (no matter how long X-axis is).

    Arguments:

    lnFile  - full pathname of the PDF file with the line diagrams
    docTitle- document title
    xLst    - list of strings that are used as X-axis labels
    yLst    - three-dimensional list or tuple of stats that are used to depict Y-axis values and errors (3 dimensione
              means that every ultimate element can be accessed with 3 indexes):
                1st level - type of output metric (those will be shown on completely independent bar diagrams);
                            even if a single metric is plotted, this level of list must exist
                2nd level - represents individual bars in each group
                            even if a single bar in each group is plotted, this level of list must exist
                3rd level - just a list of 3-tuples (average, minimal, maximal stats counter); note that dimension
                            of this level must exactly match to xLst
    yLab1   - list or tuple of strings that are used as headers for the output metrics; the size of this list must
              match the dimension of the 1st level in yLst
    yLab2   - list or tuple of strings that are used as legends for the bars in every group; the size of this list
              st match the dimension of the 2nd level in yLst
    xRot    - rotation of X markers (integer degrees)
    lnWd    - X-width on the page allocated per single X value (inch)
    '''
    xlen        = len(xLst)                 # number of X markers
    ln          = plt.figure(2)             # page for the line diagrams
    lnPageSizes = ln.get_size_inches()      # default sizes of the line pages (inch)
    ln.set_size_inches((max(lnWd * xlen, lnPageSizes[0]), lnPageSizes[1]))
    color_names = (int((len(yLab2) - 1) / len(colLst) + 1)) * colLst

    if DBGMODE:
        lnPageDpi   = ln.get_dpi()              # default dpi of the line pages
        lnPageSizes = ln.get_size_inches()      # modified sizes of the line pages (inch)
        print('\n'.join((
            'PARAMETERS OF LINE DIAGRAM:',
            '\tpage sizes (inch):   {} x {} inch ({} x {})'.format(
                lnPageSizes[0], lnPageSizes[1], lnPageDpi*lnPageSizes[0], lnPageDpi*lnPageSizes[1]),
        )))

    with PdfPages(lnFile) as pdfLn:

        ln.text(
            docTit[0], docTit[1], docTitle,
            horizontalalignment='center', verticalalignment='center',
            backgroundcolor=titBkgCol, weight=titFntWght, size=titFntSize)
        pdfLn.savefig(ln)
        ln.clear()

        for y1Ind, y1Lbl in enumerate(yLab1):
            for y2Ind, y2Lbl in enumerate(yLab2):
                xpos = [lnWd * x for x in range(xlen)]
                tmp  = yLst[y1Ind][y2Ind]
                numsamples = len(tmp[0])
                if len(linecolors) > 1:
                    for k in range(numsamples):
                        plt.plot(xpos, [x[k] for x in tmp], color=clr.cnames[linecolors[k]])                              # min
                else:
                    plt.plot(xpos, [x[0] for x in tmp], color=clr.cnames[color_names[y2Ind]])   # average
                    plt.plot(xpos, [x[1] for x in tmp], 'k--')                              # min
                    plt.plot(xpos, [x[2] for x in tmp], 'k--')                              # max
                plt.axhline(y=max([x[0] for x in tmp]))
                plt.xticks([lnWd * x for x in range(xlen)], xLst, rotation=xRot, size=fSize, verticalalignment=valign, horizontalalignment=halign)
                plt.yticks(size=fSize)
                plt.title('{}, {}'.format(y1Lbl, y2Lbl), fontsize=mtrFntSize)
                if y2LblFirst:
                    plt.title('{}\n\n({})'.format(y2Lbl, y1Lbl), fontsize=mtrFntSize)
                plt.grid()
                if xlabel: plt.xlabel(xlabel)
                if ylabel: plt.ylabel(ylabel)
                #pdfLn.savefig(ln, bbox_inches='tight')
                pdfLn.savefig(ln)
                ln.clear()

