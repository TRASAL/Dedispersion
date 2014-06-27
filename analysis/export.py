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

def print_results(confs):
    for conf in confs:
        for item in conf:
            print(item, end=" ")
        print()

def get_dm_range(queue, table):
    queue.execute("SELECT DISTINCT DMs FROM " + table + " ORDER BY DMs")
    return queue.fetchall()

def tune(queue, table, operator, channels, samples, flags):
    confs = list()
    if operator.casefold() == "max" or operator.casefold() == "min":
        dms_range = get_dm_range(queue, table)
        for dm in dms_range:
            if flags[0]:
                if flags[1]:
                    queue.execute("SELECT local,samplesPerBlock,DMsPerBlock,samplesPerThread,DMsPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + "))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
                else:
                    if flags[2]:
                        queue.execute("SELECT local,samplesPerBlock,DMsPerBlock,samplesPerThread,DMsPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 1))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 1)")
                    else:
                        queue.execute("SELECT local,samplesPerBlock,DMsPerBlock,samplesPerThread,DMsPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 0))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND local = 0)")
                best = queue.fetchall()
                confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4], best[0][5], best[0][6], best[0][7], best[0][8]])
            else:
                queue.execute("SELECT samplesPerThread,DMsPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + "))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
                best = queue.fetchall()
                confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4], best[0][5]])
    return confs

def tune_no_reuse(queue, table, operator, channels, samples, opencl):
    confs = list()
    if operator.casefold() == "max" or operator.casefold() == "min":
        dms_range = get_dm_range(queue, table)
        for dm in dms_range:
            if opencl:
                no_reuse = "(DMsPerBlock = 1 AND DMsPerThread = 1)"
                queue.execute("SELECT local,samplesPerBlock,samplesPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + "))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + ")")
                best = queue.fetchall()
                confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4], best[0][5], best[0][6]])
            else:
                no_reuse = "(DMsPerThread = 1)"
                queue.execute("SELECT samplesPerThread,GFLOPS,GFLOPS_err,time,time_err FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + "))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + ")")
                best = queue.fetchall()
                confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4]])
    return confs

def statistics(queue, table, channels, samples):
    confs = list()
    dms_range = get_dm_range(queue, table)
    for dm in dms_range:
        queue.execute("SELECT MIN(GFLOPs),AVG(GFLOPs),MAX(GFLOPs),STDDEV_POP(GFLOPs) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        line = queue.fetchall()
        confs.append([dm[0], line[0][0], line[0][2], line[0][1], line[0][3], (line[0][2] - line[0][1]) / line[0][3]])
    return confs

