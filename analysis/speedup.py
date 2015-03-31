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

def speedup(queue, table, reference, channels, samples):
    confs = list()
    dms_range = manage.get_dm_range(queue, table, channels, samples)
    for dm in dms_range:
        queue.execute("SELECT MIN(time) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        best = queue.fetchall()[0][0]
        queue.execute("SELECT MIN(time) FROM " + reference + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        ref = queue.fetchall()[0][0]
        confs.append([dm[0], ref / best])
    return confs

def speedupNoReuse(queue, table, channels, samples):
    confs = list()
    dms_range = manage.get_dm_range(queue, table, channels, samples)
    for dm in dms_range:
        queue.execute("SELECT MIN(time) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + ")")
        best = queue.fetchall()[0][0]
        queue.execute("SELECT MIN(time) FROM " + table + " WHERE (DMs = " + str(dm[0]) + " AND channels = " + channels + " AND samples = " + samples + " AND DMsPerBlock = 1 and DMsPerThread = 1)")
        ref = queue.fetchall()[0][0]
        confs.append([dm[0], ref / best])
    return confs

