#!/usr/bin/env python
# Copyright 2014 Alessio Sclocco <a.sclocco@vu.nl>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import manage

def statistics(queue, table, channels, samples, flags):
    confs = list()
    dms_range = manage.get_dm_range(queue, table)
    for dm in dms_range:
        if flags[0]:
            queue.execute("SELECT MIN(GFLOPs),AVG(GFLOPs),MAX(GFLOPs),STDDEV_POP(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        elif flags[1]:
            queue.execute("SELECT MIN(GFLOPs),AVG(GFLOPs),MAX(GFLOPs),STDDEV_POP(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 1)")
        else:
            queue.execute("SELECT MIN(GFLOPs),AVG(GFLOPs),MAX(GFLOPs),STDDEV_POP(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 0)")
        line = queue.fetchall()
        confs.append([dm[0], line[0][0], line[0][2], line[0][1], line[0][3], (line[0][2] - line[0][1]) / line[0][3]])
    return confs

def histogram(queue, table, channels, samples, flags):
    hists = list()
    dms_range = manage.get_dm_range(queue, table)
    for dm in dms_range:
        if flags[0]:
            queue.execute("SELECT MAX(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        elif flags[1]:
            queue.execute("SELECT MAX(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 1)")
        else:
            queue.execute("SELECT MAX(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 0)")
        maximum = int(queue.fetchall()[0][0])
        hist = [0 for i in range(0, maximum + 1)]
        if flags[0]:
            queue.execute("SELECT GFLOPs FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        elif flags[1]:
            queue.execute("SELECT GFLOPs FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 1)")
        else:
            queue.execute("SELECT GFLOPs FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 0)")
        flops = queue.fetchall()
        for flop in flops:
            hist[int(flop[0])] = hist[int(flop[0])] + 1
        hists.append(hist)
    return hists

