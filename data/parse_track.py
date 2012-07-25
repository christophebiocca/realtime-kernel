#!/usr/bin/env python
import sys
from ctypes import c_uint

def djb2(s):
    h = c_uint(5381)
    for c in s:
        h = c_uint(((h.value << 5) + h.value) + ord(c))
    return h.value

########################################################################
#### Usage and Options.
from optparse import OptionParser
usage = '''%prog [OPTIONS] TRACK-FILE...
e.g. %prog tracka trackb'''
parser = OptionParser(usage=usage)
parser.add_option('-C', dest='c', default='track_data.c',
  help='output .c file (default is track_data.c)',
  metavar='OUTPUT-C-FILE')
parser.add_option('-H', dest='h', default='track_data.h',
  help='output .h file (default is track_data.h)',
  metavar='OUTPUT-H-FILE')
parser.add_option('-S', dest='s',
  help='Compute hashes with hash table of size SIZE',
  metavar='SIZE')
(options, args) = parser.parse_args()
if len(args) == 0 or not options.c or not options.h or not options.s:
  parser.print_help()
  exit(0)

########################################################################
#### Parsers for the track data files.
# Unfortunately, this ended up being more code than I was hoping, since
# we want to check for errors, and also since we want provide a useful
# message if one is detected.

def remove_junk(line):
  # Remove comments and the trailing newlines.
  line = line.rstrip('\n')
  idx = line.find('#')
  if idx != -1:
    line = line[:idx]
  return line
class parser_state:
  def __init__(self, fname):
    self.fname = fname
    fh = open(fname)
    self.lines = [remove_junk(line) for line in fh.readlines()]
    fh.close()
    self.y = 0
    self.errors = 0
  def line(self):
    if self.y >= len(self.lines): return None
    return self.lines[self.y]
  def next_line(self):
    self.y += 1
  def error(self, msg, y=None):
    if y == None:
      y = self.y
    print ('%s:%d: %s' % (self.fname, y+1, msg))
    sys.stdout.flush()
    self.errors += 1

class track:
  def __init__(self, state):
    # Read the global properties.
    self.function = None
    while state.line() != None and not state.line().endswith(':'):
      line = state.line().split()
      if len(line) == 0:
        # Empty line, do nothing.
        pass
      elif line[0] == 'function':
        # function <FUNCTION_NAME>
        if self.function != None:
          state.error("duplicate 'function'")
        if len(line) != 2:
          state.error("expected 'function <FUNCTION_NAME>'")
        else:
          self.function = line[1]
      else:
        # Unknown property.
        state.error("unknown property '%s'" % line[0])
      state.next_line()
    if self.function == None:
      state.error("missing property 'function <FUNCTION_NAME>'")
    # Read the nodes and edges.
    self.nodes = []
    self.name2node = {}
    self.edges = []
    while state.line() != None:
      line = state.line()
      assert line.endswith(':')
      line = line[:-1].split()
      if line[0] == 'node':
        n = node(state)
        n.index = len(self.nodes)
        self.nodes.append(n)
        if n.name in self.name2node:
          state.error("duplicate node '%s'" % n.name)
        self.name2node[n.name] = n
      elif line[0] == 'edge':
        self.edges.append(edge(state))
      else:
        state.error("unknown type '%s'" % line[0])
        state.next_line()
        while  state.line() != None and not state.line().endswith(':'):
          state.next_line()
    assert state.line() == None
    # Convert node names to the actual nodes.
    for nd in self.nodes:
      for dir in ['reverse', 'ahead', 'straight', 'curved']:
        name = nd.__dict__[dir]
        if name != None:
          nd.__dict__[dir] = self.name2node.get(name)
          if nd.__dict__[dir] == None:
            state.error("no node with name '%s'" % name, \
              nd.__dict__[dir+'_y'])
    # Add the edge data to the nodes.
    for e in self.edges:
      good = True
      for end in ['node1', 'node2']:
        name = e.__dict__[end]
        e.__dict__[end] = self.name2node.get(name)
        if e.__dict__[end] == None:
          state.error("no node with name '%s'" % name, e.y)
          good = False
      if good:
        e.node1.add_edge(state, e.node2, e)
        if e.node1.reverse and e.node2.reverse:
          e.node2.reverse.add_edge(state, e.node1.reverse, e)
    # Check for missing edge data.
    for nd in self.nodes:
      for dir in ['ahead', 'straight', 'curved']:
        dest = nd.__dict__[dir]
        if dest != None:
          if nd.__dict__[dir+"_edge"] == None:
            state.error("missing edge from '%s' to '%s'" % \
              (nd.name, dest.name))

class node:
  def __init__(self, state):
    line = state.line()[:-1].split()
    self.name = None
    if len(line) != 2:
      state.error("expected 'node <NAME>'")
    else:
      self.name = line[1]
    # Read the node properties.
    self.nodetype = None
    self.num = None
    self.reverse = None
    self.ahead = None
    self.straight = None
    self.curved = None
    self.ahead_edge = None
    self.straight_edge = None
    self.curved_edge = None
    state.next_line()
    while state.line() != None and not state.line().endswith(':'):
      line = state.line().split()
      if len(line) == 0:
        # Empty line, do nothing.
        pass
      elif line[0] in ['sensor', 'sensor_dead']:
        # sensor <SENSOR_NUM>
        if self.nodetype != None:
          state.error("duplicate node type")
        self.nodetype = line[0]
        try:
          assert len(line) == 2
          self.num = int(line[1])
        except (AssertionError, ValueError):
          state.error("expected 'sensor(_dead) <SENSOR_NUM>'")
      elif line[0] in ['branch', 'branch_curved', 'branch_straight', 'merge']:
        # branch/merge <SWITCH_NUM>
        if self.nodetype != None:
          state.error("duplicate node type")
        self.nodetype = line[0]
        try:
          assert len(line) == 2
          self.num = int(line[1])
        except (AssertionError, ValueError):
          state.error("expected '%s <SWITCH_NUM>'" % line[0])
      elif line[0] in ['enter', 'exit']:
        # enter/exit
        if self.nodetype != None:
          state.error("duplicate node type")
        self.nodetype = line[0]
        try:
          assert len(line) == 1
        except (AssertionError, ValueError):
          state.error("expected '%s'" % line[0])
      elif line[0] in ['reverse', 'ahead', 'straight', 'curved']:
        # reverse/ahead/straight/curved <NODE>
        if self.__dict__[line[0]] != None:
          state.error("duplicate '%s'" % line[0])
        if len(line) != 2:
          state.error("expected '%s <NODE>'" % line[0])
        else:
          self.__dict__[line[0]] = line[1]
          self.__dict__[line[0]+'_y'] = state.y
      else:
        # Unknown property.
        state.error("unknown property '%s'" % line[0])
      state.next_line()
    # Make sure that we have all the required data.
    if self.nodetype == None:
      state.error("missing node type")
    if self.reverse == None:
      state.error("missing property 'reverse <NODE>'")
    if self.nodetype in ['sensor', 'enter', 'merge']:
      try:
        assert self.ahead    != None
        assert self.straight == None
        assert self.curved   == None
      except (AssertionError):
        state.error("%s node expects 'ahead' edge" % self.nodetype)
    elif self.nodetype in ['exit']:
      try:
        assert self.ahead    == None
        assert self.straight == None
        assert self.curved   == None
      except (AssertionError):
        state.error("%s node expects NO edges" % self.nodetype)
    elif self.nodetype in ['branch', 'branch_curved', 'branch_straight']:
      try:
        assert self.ahead    == None
        assert self.straight != None
        assert self.curved   != None
      except (AssertionError):
        state.error("%s node expects 'straight' and 'curved' edges" \
          % self.nodetype)
          
  def add_edge(self, state, dest, e):
    count = 0
    for dir in ['ahead', 'straight', 'curved']:
      if self.__dict__[dir] == dest:
        if self.__dict__[dir+"_edge"] != None:
          state.error("duplicate edge from '%s' to '%s'" % \
            (self.name, dest.name), e.y)
        self.__dict__[dir+"_edge"] = e
        count += 1
    if count == 0 and self.nodetype not in ('branch_curved', 'branch_straight'):
      state.error("there is no edge from '%s' to '%s' in the node data"\
        % (self.name, dest.name), e.y)
    elif count > 1:
      state.error("too many edges from '%s' to '%s' in the node data" \
        % (self.name, dest.name), e.y)

class edge:
  def __init__(self, state):
    self.y = state.y
    line = state.line()[:-1].split()
    self.node1 = self.node2 = ''
    if len(line) != 3:
      state.error("expected 'edge <NODE1> <NODE2>'")
    else:
      self.node1 = line[1]
      self.node2 = line[2]
    # Read the edge properties.
    self.dist = None
    state.next_line()
    while state.line() != None and not state.line().endswith(':'):
      line = state.line().split()
      if len(line) == 0:
        # Empty line, do nothing.
        pass
      elif line[0] == 'distance':
        # distance <INTEGER> mm
        if self.dist != None:
          state.error("duplicate 'distance'")
        try:
          assert len(line) == 3
          assert line[2] == 'mm'
          self.dist = int(line[1])
        except (AssertionError, ValueError):
          state.error("expected 'distance <INTEGER> mm'")
      else:
        # Unknown property.
        state.error("unknown property '%s'" % line[0])
      state.next_line()
    # Make sure that we have all the required data.
    if self.dist == None:
      state.error("missing property 'distance <INTEGER> mm'")

########################################################################
#### Parse each of the input files.
tracks = {}
errors = 0
for fname in args:
  state = parser_state(fname)
  tr = track(state)
  if state.errors > 0:
    errors += 1
  else:
    tracks[tr.function] = tr
if errors > 0:
  exit(1)

########################################################################
#### Output the .c code.
maxidx = max([len(tracks[function].nodes) for function in tracks])

fh = open(options.c, 'w')
fh.write('''/* THIS FILE IS GENERATED CODE -- DO NOT EDIT */

#include <lib.h>
#include <user/track_data.h>
#include <user/clock.h>

struct TrackNode nodes[TRACK_MAX];
struct TrackHashNode hashtbl[MAX_HASHNODES];

static void *memset(void *s, int c, unsigned int n) {
  unsigned char *p = s;
  while(n --> 0) { *p++ = (unsigned char)c; }
  return s;
}
''')
for fun in tracks:
  hashtbl = {}
  max_collisions = 0

  fh.write('''
void %s(struct TrackNode *track, struct TrackHashNode *hashtbl) {
  memset(track, 0, TRACK_MAX*sizeof(struct TrackNode));
''' % fun)
  for nd in tracks[fun].nodes:
    idx = nd.index
    fh.write("  track[%d].idx = %d;\n" % (idx, idx))
    fh.write("  track[%d].name = \"%s\";\n" % (idx, nd.name))
    nodetype = 'NODE_' + nd.nodetype.upper()
    fh.write("  track[%d].type = %s;\n" % (idx, nodetype))
    if nd.num != None:
      fh.write("  track[%d].num = %s;\n" % (idx, nd.num))
    fh.write("  track[%d].reverse = &track[%d];\n" % \
      (idx, nd.reverse.index))
    for dir in ['ahead', 'straight', 'curved']:
      if nd.__dict__[dir] != None:
        idx2 = nd.__dict__[dir].index
        dist = nd.__dict__[dir+"_edge"].dist
        for dir2 in ['ahead', 'straight', 'curved']:
          if nd.__dict__[dir].reverse.__dict__[dir2] == nd.reverse:
            fh.write(("  track[%d].edge[DIR_%s].reverse =" + \
                      " &track[%d].edge[DIR_%s];\n") % \
              (idx, dir.upper(), nd.__dict__[dir].reverse.index, \
               dir2.upper()))
        fh.write("  track[%d].edge[DIR_%s].src = &track[%d];\n" % \
          (idx, dir.upper(), idx))
        fh.write("  track[%d].edge[DIR_%s].dest = &track[%d];\n" % \
          (idx, dir.upper(), idx2))
        fh.write("  track[%d].edge[DIR_%s].dist = %s;\n" % \
          (idx, dir.upper(), dist))
        fh.write("  track[%d].edge[DIR_%s].reserved = -1;\n" % \
          (idx, dir.upper()))


    djb_hash = djb2(nd.name) % int(options.s)
    if djb_hash not in hashtbl:
        hashtbl[djb_hash] = 0

    fh.write('  hashtbl[%d].chain[%d] = &track[%d];\n' %
        (djb_hash, hashtbl[djb_hash], idx))

    hashtbl[djb_hash] += 1
    if hashtbl[djb_hash] > max_collisions:
        max_collisions = hashtbl[djb_hash]

  for i in range(0, maxidx - len(tracks[fun].nodes)):
    fh.write("  track[%d].type = NODE_NONE;\n" % (len(tracks[fun].nodes) + i))

  for i in range(int(options.s)):
      if i in hashtbl:
          val = hashtbl[i]
      else:
          val = 0

      fh.write('  hashtbl[%d].length = %d;\n' %
        (i, val))


  print max_collisions
  tracks[fun].max_collisions = max_collisions

  fh.write("}\n")


fh.write(r'''
static unsigned int djb2(const char *str) {
    unsigned int hash = 5381;

    for (; *str != '\0'; ++str) {
        hash = ((hash << 5) + hash) + *str;
    }

    return hash;
}

struct TrackNode *lookupTrackNode(struct TrackHashNode *hashtbl, const char *name) {
    struct TrackHashNode *node = &hashtbl[djb2(name) %% %s];

    switch (node->length) {
        case 3:
            if (strcmp(name, node->chain[2]->name) == 0) {
                return node->chain[2];
            }
        case 2:
            if (strcmp(name, node->chain[1]->name) == 0) {
                return node->chain[1];
            }
        case 1:
            if (strcmp(name, node->chain[0]->name) == 0) {
                return node->chain[0];
            }
        default:
            return (struct TrackNode*) 0;
    }
}

struct TrackNode *randomTrackNode(struct TrackNode *track) {
    unsigned int time = Time();
    struct TrackNode *dest = &track[time %% TRACK_MAX];

    // only try 5 times max to get a new destination
    for (int i = 0; dest->type == NODE_NONE && i < 5; ++i) {
        time <<= i;
        dest = &track[time %% TRACK_MAX];
    }

    if (dest->type == NODE_NONE) {
        // failed 5 times, just go to first node
        dest = &track[0];
    }

    return dest;
}
''' % options.s)

fh.close()

########################################################################
#### Output the .h code.
# This is the right place to make changes that you want to appear in
# the generated file (as opposed to in the file itself, since it will
# be overwritten when this script is run again).
maxhn = max([tracks[function].max_collisions for function in tracks])
fh = open(options.h, 'w')
fh.write('''#ifndef USER_TRACK_DATA_H
#define USER_TRACK_DATA_H 1

/* THIS FILE IS GENERATED CODE -- DO NOT EDIT */

enum NodeType {
  NODE_NONE = 1<<0,
  NODE_SENSOR = 1<<1,
  NODE_SENSOR_DEAD = 1<<2,
  NODE_BRANCH = 1<<3,
  NODE_BRANCH_STRAIGHT = 1<<4,
  NODE_BRANCH_CURVED = 1<<5,
  NODE_MERGE = 1<<6,
  NODE_ENTER = 1<<7,
  NODE_EXIT = 1<<8
};

#define DIR_AHEAD 0
#define DIR_STRAIGHT 0
#define DIR_CURVED 1

struct TrackNode;

struct TrackEdge {
  struct TrackEdge *reverse;
  struct TrackNode *src, *dest;
  int dist;              /* in millimetres */
  volatile int reserved; /* train id of whoever owns this track, -1 otherwise */
};

struct TrackNode {
  const char *name;
  enum NodeType type;
  int num;              /* sensor or switch number */
  int idx;
  struct TrackNode *reverse;  /* same location, but opposite direction */
  struct TrackEdge edge[2];
};

#define MAX_HASHNODE_CHAIN_LENGTH %d

struct TrackHashNode {
  int length;
  struct TrackNode *chain[MAX_HASHNODE_CHAIN_LENGTH];
};

#define MAX_HASHNODES %d

// The track initialization functions expect an array of this size.
#define TRACK_MAX %d

struct TrackNode *lookupTrackNode(struct TrackHashNode *hashtbl, const char *name);
struct TrackNode *randomTrackNode(struct TrackNode *track);

extern struct TrackNode nodes[TRACK_MAX];
extern struct TrackHashNode hashtbl[MAX_HASHNODES];

''' % (maxhn, int(options.s), maxidx))
for fun in tracks:
  fh.write("void %s(struct TrackNode *track, struct TrackHashNode *hashtbl);\n" % fun)

fh.write('#endif\n')
fh.close()
