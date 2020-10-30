#!/usr/bin/env python

import fileinput
import getopt
import sys
import datetime
import svgwrite

"""lsn-graph takes input of addb2dump.

   Some assumptions are made about the input stream:

       - all records are from the same process,

       - the time-stamps of incoming records are monotonically
         increasing. m0addb2dump output does not necessarily conform to this
         restriction. This can always be fixed by doing

             m0addb2dump -f | sort -k2.1,2.29 -s | m0addb2dump -d

"""
class record(object):
    def __init__(self, field):
# 0 1                2          3    4  5    6   7     8             9     10
# * 2020-05-02-04... tx-capture tid: 0, lsn: 11, addr: 4000000001d8, size: 56
        self.time = parsetime(field[1])
        self.tid  = int(field[4][:-1])
        self.lsn  = int(field[6][:-1])
        self.addr = int(field[8][:-1], 16)
        self.size = int(field[10])

class trace(object):
    def __init__(self, width, height, step = 100, outname = "lsn.svg",
                 verbosity = 0, label = True):
        self.timeformat = "%Y-%m-%d-%H:%M:%S.%f"
        self.L = {}
        self.M = {}
        self.S = {}
        self.T = {}
        self.addr_min = 0xffffffffffffffff
        self.addr_max = 0
        self.lsn_min  = 0xffffffffffffffff
        self.lsn_max  = 0
        self.time_min = datetime.datetime.max
        self.time_max = datetime.datetime.min
        
        self.label  = label
        self.width  = width
        self.height = height
        self.step   = datetime.timedelta(microseconds = step)
        self.verb   = verbosity
        self.out    = svgwrite.Drawing(outname, profile='full', \
                                       size = (str(width)  + "px",
                                               str(height) + "px"))
        self.lmargin = 100 #width * 0.05
        self.rmargin = 100 #width * 0.05
        self.tmargin = 100 #width * 0.05
        self.bmargin = 100 #width * 0.05

        self.axis = {
            "stroke"           : svgwrite.rgb(0, 0, 0, '%'),
            "stroke_width"     : 1,
            "stroke_dasharray" : "20,10,5,5,5,10"
        }
        self.addraxis = {
            "stroke"           : svgwrite.rgb(70, 70, 70, '%'),
            "stroke_width"     : 1,
            "stroke_dasharray" :"1,1"
        }
        self.dep         = svgwrite.rgb(0, 0, 0, '%')
        self.nxt         = svgwrite.rgb(100, 0, 0, '%')
        self.textstep    = 15
        self.scribbles   = set()
        self.dash        = {
            "stroke"           : svgwrite.rgb(0, 0, 0, '%'),
            "stroke_width"     : 1,
            "stroke_dasharray" :"1,1"
        }
        self.warnedlabel = False

    def add(self, field):
        rec = record(field)
        self.addr_min = min(self.addr_min, rec.addr)
        self.addr_max = max(self.addr_max, rec.addr)
        self.lsn_min  = min(self.lsn_min, rec.lsn)
        self.lsn_max  = max(self.lsn_max, rec.lsn)
        if rec.time < self.time_min:
            self.time_min = rec.time
        if rec.time > self.time_max:
            self.time_max = rec.time
        self.L[rec.lsn] = rec
        self.M[rec.addr] = rec

    def done(self):
        self.dx = self.width / len(self.M)
        self.dy = min(self.height / len(self.M),
                      self.getpos(self.time_min + self.step) -
                      self.getpos(self.time_min))
        i = 0
        for addr in sorted(self.M):
            self.S[addr] = i
            i += 1
        print("Addresses sorted.");
        for lsn in sorted(self.L):
            rec = self.L[lsn]
            if rec.addr in self.T:
                self.T[rec.addr].append(rec)
            else:
                self.T[rec.addr] = [rec]
        print("Per-addresses traces built.");
        for addr in self.M:
            x = self.getaddrpos(self.S[addr])
            self.line((x + self.dx/2, 0), (x + self.dx/2, self.height),
                      **self.dash)
            self.text("{0:x}".format(addr),
                      insert = (x - 110, 30 + (self.S[addr] % 6) * 30),
                      force = True)
        print("Address marks drawn.");
        t = self.time_min
        while t < self.time_max:
            y = self.getpos(t)
            self.line((0, y), (self.width, y), **self.axis)
            self.text(t.strftime(self.timeformat),
                      insert = (0, y - 10), force = True)
            t += self.step
        print("Time marks drawn.");
        for lsn, rec in self.L.items():
            self.rect(insert = (self.getaddrpos(self.S[rec.addr]) + self.dx/4,
                                self.getpos(rec.time) + self.dy/4),
                      size = (self.dx / 2, self.dy / 2),
                      fill = self.getcolour(str(rec.tid)))
            if rec == self.T[rec.addr][0]:
                self.T[rec.addr] = self.T[rec.addr][1:]
                if len(self.T[rec.addr]) > 0:
                    self.recline(rec, self.T[rec.addr][0],
                                 stroke = self.nxt, stroke_width = 1)
        print("Captured regions and traces drawn.");
        prev = -1
        for lsn in sorted(self.L):
            if prev != -1:
                self.recline(self.L[prev], self.L[lsn],
                             stroke = self.dep, stroke_width = 1)
                if prev + 1 != lsn:
                    print("Missing lsns [{} .. {}].".format(prev + 1, lsn - 1))
                if self.L[lsn].time < self.L[prev].time:
                    print("Going back in time: {} -> {}.".format(prev, lsn))
            prev = lsn
        print("Next-lsn links drawn.");
        self.out.save()

    def recline(self, rec0, rec1, **kw):
        self.line((self.getaddrpos(self.S[rec0.addr]) + self.dx/2,
                   self.getpos(rec0.time) + self.dy/2),
                  (self.getaddrpos(self.S[rec1.addr]) + self.dx/2,
                   # dy/4 sic.
                   self.getpos(rec1.time) + self.dy/4), **kw)
    def getpos(self, stamp):
        interval = (stamp - self.time_min).total_seconds()
        return (self.height - self.tmargin - self.bmargin) * interval / \
            (self.time_max - self.time_min).total_seconds() + self.tmargin

    def getaddrpos(self, addr_nr):
        return (self.width - self.lmargin - self.rmargin) * addr_nr / \
            len(self.M) + self.lmargin

    def getcolour(self, str):
        seed = str + "^" + str
        red   = hash(seed + "r") % 90
        green = hash(seed + "g") % 90
        blue  = hash(seed + "b") % 90
        return svgwrite.rgb(red, green, blue, '%')
        
    def rect(self, **kw):
        y = kw["insert"][1]
        h = kw["size"][1]
        if y + h >= 0 and y < self.height:
            self.out.add(self.out.rect(**kw))

    def line(self, start, end, **kw):
        if end[1] >= 0 and start[1] < self.height:
            self.out.add(self.out.line(start, end, **kw))

    def tline(self, start, end, **kw):
        if self.label:
            self.line(start, end, **kw)

    def text(self, text, connect = False, force = False, **kw):
        x = int(kw["insert"][0])
        y0 = y = int(kw["insert"][1]) // self.textstep * self.textstep
        if not self.label and not force:
            return (x, y)
        if y >= 0 and y < self.height:
            i = 0
            while (x, y // self.textstep) in self.scribbles:
                y += self.textstep
                if i > 30:
                    if not self.warnedlabel:
                        print("Labels are overcrowded. Increase image height.")
                        self.warnedlabel = True
                    break
                i += 1
            kw["insert"] = (x + 10, y)
            kw["font_family"] = "Courier"
            self.out.add(self.out.text(text, **kw))
            if connect:
                self.line((x, y0), (x + 10, y - 4), **self.dash)
            self.scribbles.add((x, y // self.textstep))
        return x + 10 + len(text) * 9.5, y - 4 # magic. Depends on the font.

def parsetime(stamp):
    #
    # strptime() is expensive. Hard-code.
    # cut to microsecond precision
    # datetime.datetime.strptime(stamp[0:-3], trace.timeformat)
    #
    # 2016-03-24-09:18:46.359427942
    # 01234567890123456789012345678
    return datetime.datetime(year        = int(stamp[ 0: 4]),
                             month       = int(stamp[ 5: 7]),
                             day         = int(stamp[ 8:10]),
                             hour        = int(stamp[11:13]),
                             minute      = int(stamp[14:16]),
                             second      = int(stamp[17:19]),
                             microsecond = int(stamp[20:26]))
    
def processinput(argv):
    kw = {
        "height"    :   10000,
        "width"     :    4000,
        "step"      :     100
    }
    xlate = {
        "-h" : ("height",    int, "Output image height in pixels."),
        "-w" : ("width",     int, "Output image width in pixels."),
        "-v" : ("verbosity", int, "Verbosity level."),
        "-s" : ("step",      int, "Milliseconds between timestamp axes."),
        "-L" : ("label",     int, "If 0, omit text labels,"),
        "-o" : ("outname",   str, "Output file name.")
    }
    try:
        opts, args = getopt.getopt(argv[1:], "h:w:s:o:v:L:")
    except getopt.GetoptError:
        print("{} [options]\n\nOptions:\n".format(argv[0]))
        for k in xlate:
            print("    {} : {}".format(k, xlate[k][2]))
        sys.exit(2)
    for opt, arg in opts:
        xl = xlate[opt]
        kw[xl[0]] = xl[1](arg)
   
    tr = trace(**kw)
    rec = None;
    for line in fileinput.input([]):
        if line[0] != "*":
            continue;
        field = line.split()
        if field[2] != "tx-capture":
            continue;
        tr.add(field)
    tr.done()

if __name__ == "__main__":
    processinput(sys.argv)
