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

def tune(queue, table, operator, channels, samples, flags):
    confs = list()
    if operator.casefold() == "max" or operator.casefold() == "min":
        condition = str()
        if flags[0] == 1:
            condition = "local == 1"
        elif flags[0] == 2:
            condition = "local == 0"
        if flags[1] == 1:
            if flags[0] != 0:
                condition += "AND splitSeconds == 1"
            else:
                condition = "splitSeconds == 1"
        elif flags[1] == 2:
            if flags[0] != 0:
                condition += "AND splitSeconds == 0"
            else:
                condition = "splitSeconds == 0"
        dms_range = manage.get_dm_range(queue, table, channels, samples)
        for dm in dms_range:
            if flags[0] == 0 and flags[1] == 0:
                queue.execute("SELECT splitSeconds,local,unroll,samplesPerBlock,DMsPerBlock,samplesPerThread,DMsPerThread,GFLOPS,time,time_err,cov FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + "))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
            else:
                queue.execute("SELECT splitSeconds,local,unroll,samplesPerBlock,DMsPerBlock,samplesPerThread,DMsPerThread,GFLOPS,time,time_err,cov FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND (" + condition + ")))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND (" + condition + "))")
            best = queue.fetchall()
            confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4], best[0][5], best[0][6], best[0][7], best[0][8], best[0][9], best[0][10]])
    return confs

def tune_no_reuse(queue, table, operator, channels, samples, flags):
    confs = list()
    if operator.casefold() == "max" or operator.casefold() == "min":
        condition = str()
        if flags[0] == 1:
            condition = "local == 1"
        elif flags[0] == 2:
            condition = "local == 0"
        if flags[1] == 1:
            if flags[0] != 0:
                condition += "AND splitSeconds == 1"
            else:
                condition = "splitSeconds == 1"
        elif flags[1] == 2:
            if flags[0] != 0:
                condition += "AND splitSeconds == 0"
            else:
                condition = "splitSeconds == 0"
        dms_range = manage.get_dm_range(queue, table, channels, samples)
        no_reuse = "(DMsPerBlock = 1 AND DMsPerThread = 1)"
        for dm in dms_range:
            if flags[0] == 0 and flags[1] == 0:
                queue.execute("SELECT splitSeconds,local,unroll,samplesPerBlock,samplesPerThread,GFLOPS,time,time_err,cov FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + "))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + ")")
            else:
                queue.execute("SELECT splitSeconds,local,unroll,samplesPerBlock,samplesPerThread,GFLOPS,time,time_err,cov FROM " + table + " WHERE (GFLOPS = (SELECT " + operator + "(GFLOPS) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + " AND (" + condition + ")))) AND (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND " + no_reuse + " AND (" + condition + "))")
            best = queue.fetchall()
            confs.append([dm[0], best[0][0], best[0][1], best[0][2], best[0][3], best[0][4], best[0][5], best[0][6], best[0][7], best[0][8]])
    return confs

