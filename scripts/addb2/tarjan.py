import sys

#
# Tarjan's strongly connected components algorithm
#
# https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
#

class node(object):
    def __init__(self, name):
        self.index = -1
        self.lowlink = -1
        self.onStack = False
        self.name = name
        self.dep = []

class graph(object):
    def __init__(self):
        self.V = []

    def node(self, name):
        for n in self.V:
            if (n.name == name):
                return n
        n = node(name);
        self.V.append(n)
        return n

    def dep(self, n0, n1):
        n0.dep.append(n1)

    def tarjan(self):
        self.index = 0
        self.S = []
        self.C = [] # list of components

        for n in self.V:
            if (n.index == -1):
                self.strongconnect(n)
        return self.C

    def strongconnect(self, v):
        v.index = self.index
        v.lowlink = self.index
        self.index += 1
        self.S.append(v)
        v.onStack = True

        # Consider successors of v
        for w in v.dep:
            if w.index == -1:
                # Successor w has not yet been visited; recurse on it
                self.strongconnect(w)
                v.lowlink = min(v.lowlink, w.lowlink)
            elif w.onStack:
                # Successor w is in stack S and hence in the current SCC

                # If w is not on stack, then (v, w) is an edge pointing to
                # an SCC already found and must be ignored
                
                # Note: The next line may look odd - but is correct.
                # It says w.index not w.lowlink; that is deliberate and
                # from the original paper
                v.lowlink = min(v.lowlink, w.index)
      
        # If v is a root node, pop the stack and generate an SCC
        if v.lowlink == v.index:
            ssc = []
            while True:
                w = self.S.pop()
                w.onStack = False
                ssc.append(w)
                if w == v:
                    break
            self.C.append(ssc)
        
g = graph()

for line in sys.stdin:
    #
    # Input lines are
    #
    #     n0 -> n1
    #
    # or
    #
    #     n0
    #
    w = line.split()
    if (len(w) == 1):
        g.node(w[0])
    elif (len(w) == 3):
        n0 = g.node(w[0])
        n1 = g.node(w[2])
        g.dep(n0, n1)
    else:
        break

SSC = g.tarjan()
for c in SSC:
    print("{} [ ".format(len(c)), end = "")
    for n in c:
        print(n.name + " ", end = "")
    print(" ]");
