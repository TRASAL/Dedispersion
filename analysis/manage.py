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

def get_tables(queue):
    """Get a list of the tables"""
    queue.execute("SHOW TABLES")
    return queue.fetchall()

def create_table(queue, table):
    """Create a table to store auto-tuning results for dedispersion."""
    queue.execute("CREATE table " + table + "(id INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT, DMs INTEGER NOT NULL, channels INTEGER NOT NULL, samples INTEGER NOT NULL, splitSeconds TINYINT NOT NULL, local TINYINT NOT NULL, unroll INTEGER NOT NULL, samplesPerBlock INTEGER NOT NULL, DMsPerBlock INTEGER NOT NULL, samplesPerThread INTEGER NOT NULL, DMsPerThread INTEGER NOT NULL, GFLOPs FLOAT UNSIGNED NOT NULL, time FLOAT UNSIGNED NOT NULL, time_err FLOAT UNSIGNED NOT NULL, cov FLOAT UNSIGNED NOT NULL)")

def delete_table(queue, table):
    """Delete table."""
    queue.execute("DROP table " + table)

def load_file(queue, table, input_file):
    """Load input_file into a table in the database."""
    for line in input_file:
        if (line[0] != "#") and (line[0] != "\n"):
            items = line.split(sep=" ")
            queue.execute("INSERT INTO " + table + " VALUES (NULL, " + items[0] + ", " + items[1] + ", " + items[2] + ", " + items[3] + ", " + items[4] + ", " + items[5] + ", " + items[6] + ", " + items[7] + ", " + items[8] + ", " + items[9] + ", " + items[10] + ", " + items[11] + ", " + items[12] + ", " + items[13].rstrip("\n") + ")")

def print_results(confs):
    """Print the result tuples."""
    for conf in confs:
        for item in conf:
            print(item, end=" ")
        print()

def get_dm_range(queue, table, channels, samples):
    """Return the DMs used in the scenario."""
    queue.execute("SELECT DISTINCT DMs FROM " + table + " WHERE (channels = " + channels + " AND samples = " + samples + ") ORDER BY DMs")
    return queue.fetchall()

