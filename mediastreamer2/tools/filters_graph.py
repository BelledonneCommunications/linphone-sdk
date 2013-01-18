#!/usr/bin/python
#
# mediastreamer2 library - modular sound and video processing and streaming
# Copyright (C) 2006-2013 Belledonne Communications, Grenoble
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

import argparse
import os
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description='Create an image of some mediastreamer2 filters graphs from traces.')
parser.add_argument('infile', metavar=('logfile'), help='The log traces file from which to generate the filters graph')
parser.add_argument('--png', dest='create_png', action='store_true', help='Generate a png file for the filters graph')
parser.add_argument('--show', dest='show', action='store_true', help='Show the generated png file')
args = parser.parse_args()

link_regexp = re.compile(r'^.+ms_filter_link: (\w+):0x(\w+),(\d)-->(\w+):0x(\w+),(\d)$')
unlink_regexp = re.compile(r'^.+ms_filter_unlink: (\w+):0x(\w+),(\d)-->(\w+):0x(\w+),(\d)$')
graphs = []
clusters = []
links = []
f = open(args.infile)
for line in f:
	result = unlink_regexp.match(line)
	if result:
		if links:
			clusters.append(links)
		if clusters:
			graphs.append(clusters)
		links = []
		clusters = []
	result = link_regexp.match(line)
	if result:
		links.append({ 'source': { 'name': result.group(1), 'address': result.group(2), 'pin': result.group(3) },
				'dest': { 'name': result.group(4), 'address': result.group(5), 'pin': result.group(6) } })
	else:
		if links:
			clusters.append(links)
			links = []
if clusters:
	graphs.append(clusters)

if graphs:
	f = open("%s.dot" % (args.infile), "w+")
	f.write('''digraph G {
  rankdir=TB;
  node [shape=record, fontname=Helvetica, fontsize=10, labelloc=b];
  edge [fontname=Helvetica, fontsize=10, labeldistance=1.5];
''')

	graph_num = 1
	node_num = 1
	for clusters in graphs:
		have_cluster = False
		cluster_num = 1
		f.write('''  subgraph cluster_graph_%d {
    color=darkgrey;
    label="Graph %d";
''' % (graph_num, graph_num))
		if len(clusters) > 1:
			have_cluster = True
		for links in clusters:
			if have_cluster:
				f.write('''  subgraph cluster_subgraph_%d {
    color=lightgrey;
    label="Subgraph %d";
''' % (cluster_num, cluster_num))
			nodes = {}
			for link in links:
				if not nodes.has_key(link['source']['address']):
					nodes[link['source']['address']] = node_num
					f.write('    %d [ label="%s [%s]"];\n' % (node_num, link['source']['name'], link['source']['address']))
					node_num += 1
				if not nodes.has_key(link['dest']['address']):
					nodes[link['dest']['address']] = node_num
					f.write('    %d [ label="%s [%s]"];\n' % (node_num, link['dest']['name'], link['dest']['address']))
					node_num += 1

			for link in links:
				f.write('    %d -> %d [taillabel="%s", headlabel="%s"];\n' % (nodes[link['source']['address']], nodes[link['dest']['address']], link['source']['pin'], link['dest']['pin']))

			if have_cluster:
				f.write('  }\n')
			cluster_num += 1
		f.write('  }\n')
		graph_num += 1

	f.write('}\n')
	f.close()

	if args.create_png or args.show:
		f = open("%s.png" % (args.infile), "w+")
		subprocess.call(["dot", "-Tpng", "%s.dot" % (args.infile)], stdout=f, stderr=sys.stderr)
		f.close()

		if args.show:
			subprocess.call(["xdg-open", "%s.png" % (args.infile)])
